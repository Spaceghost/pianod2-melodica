///
/// gstreamer player and metadata reader.
/// @file       gstreamplayer.cpp - pianod2
/// @author     Perette Barella
/// @date       2016-08-02
/// @copyright  Copyright © 2016-2023 Devious Fish. All rights reserved.
///


#include <config.h>

#include <cmath>
#include <string>
#include <mutex>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"
#pragma GCC diagnostic ignored "-Wshadow"
#include <gst/gst.h>
#pragma GCC diagnostic pop

#include "fundamentals.h"
#include "logging.h"
#include "mediaplayer.h"
#include "audio/audiooutput.h"
#include "gstreamplayer.h"

namespace Audio {
    GstreamerAudioException::GstreamerAudioException (const char *func, const char *error, const char *what, const char *what2) {
        reason = std::string {func} + ": " + error;
        if (what) {
            reason = reason + ": " + what;
        }
        if (what2) {
            reason = reason + ", " + what2;
        }
        flog (Log::ERROR, reason);
    };


    GstreamerAudioException::GstreamerAudioException (const char *func, GError *error, const char *what)
    : GstreamerAudioException (func, gst_error_get_message (error->domain, error->code), what) {
    };


    /*
     * gstreamer simple pipeline
     */


    GstreamerSimplePipeline::GstreamerSimplePipeline () {
        pipeline = gst_pipeline_new (nullptr);
        if (!pipeline) {
            throw GstreamerAudioException ("gst_pipeline_new",
                                           "Factory returned null pointer");
        }
        gst_object_ref_sink (pipeline);
    }

    GstreamerSimplePipeline::~GstreamerSimplePipeline () {
        setPipelineState (GST_STATE_NULL);
        gst_object_unref (pipeline);
    }


    /** "Throw" an exception from an asyncrhonous player thread.
        This stores a copy of the exception for throwing later, when
        getting the thread's completion status. */
    void GstreamerSimplePipeline::throwDeferredException (const GstreamerAudioException &exception,
                                                          bool asynchronously) const {
        std::lock_guard<std::recursive_mutex> lock (state_mutex);
        if (!pipeline_exception) {
            pipeline_exception.reset (new (std::nothrow) GstreamerAudioException (exception));
        }
        // If we're not asynchrous, begin shutting down the pipeline.
        if (!asynchronously) {
            gst_element_set_state (pipeline, GST_STATE_NULL);
        }
    }



    /** Query the pipeline's current state */
    GstState GstreamerSimplePipeline::currentPipelineState () const {
        GstState current_state, next_state;
        if (gst_element_get_state (pipeline, &current_state, &next_state, 0) == GST_STATE_CHANGE_FAILURE) {
            throwDeferredException (GstreamerAudioException ("gst_element_get_state", "asynchronous state change failed",
                                                             gst_element_get_name (pipeline)));
        }
        return current_state;
    }

    /** Set the pipeline's state.
        We need locking at this point, because the state may be set by a bus
        message or the pianod main thread.  This creates a race condition where
        an error shuts it down, then pianod requests playback start/resume.
       
        Furthermore, hammering state changes causes Gstreamer to become deranged
        and deadlock waiting on a condition variable.  To avoid this, give the
        prior state transition a chance to complete.
     
        But as a bonus, some elements only get to READY state; perhaps they need
        stream data to move on, but we're done and need to tear them down and move
        on.  Whatever, the case, we occasionally need to go ahead with a state
        change, even if the last one is unfinished.  Hence, we have a 1-second
        timeout for the last state change to finish, before proceeding recklessly
        with a state change.

        @param state The new state. */
    void GstreamerSimplePipeline::setPipelineState (GstState state) {
        std::lock_guard<std::recursive_mutex> lock (state_mutex);
        if (!pipeline_exception) {
            // Give chance for previous state changes to complete.
            GstState current_state, next_state;
            GstStateChangeReturn last_result = gst_element_get_state (pipeline, &current_state,
                                                                      &next_state, 1E9 /* 1 second */);
            switch (last_result) {
                case GST_STATE_CHANGE_FAILURE:
                    throwDeferredException (GstreamerAudioException ("gst_element_get_state",
                                                                     "asynchronous state change failed",
                                                                     gst_element_get_name (pipeline)));
                case GST_STATE_CHANGE_ASYNC:
                    flog (LOG_WHERE (Log::WARNING|Log::AUDIO), "setting state before prior state change completed ",
                          gst_element_get_name (pipeline));
                    /* FALLTHRU */
                case GST_STATE_CHANGE_SUCCESS:
                case GST_STATE_CHANGE_NO_PREROLL:
                    if (gst_element_set_state (pipeline, state) == GST_STATE_CHANGE_FAILURE) {
                        throwDeferredException (GstreamerAudioException ("gst_element_set_state",
                                                                         "state change failed",
                                                                         gst_element_get_name (pipeline)));
                    }
            }
        }
    }



    /** Wire an item to the pipeline/filter graph.  Items are wired in the
        order they are pushed.
        @param item The item to insert into to the pipeline.
        @throw AudioException if the item can't be linked. */
    void GstreamerSimplePipeline::push (GstElement *item) {
        if (pipeline_last) {
            if (!gst_element_link (pipeline_last, item)) {
                throw GstreamerAudioException ("gst_element_link", "push: Could not link elements",
                                               gst_element_get_name (item),
                                               gst_element_get_name (pipeline_last));
            }
        } else {
            chain_start = item;
        }
        pipeline_last = item;
    }

    void GstreamerSimplePipeline::add (GstElement *element) {
        if (!gst_bin_add(GST_BIN (pipeline), element)) {
            GstreamerAudioException ex ("gst_bin_add", "could not add element to pipeline",
                                           gst_element_get_name (pipeline), gst_element_get_name (element));
            gst_object_unref (element);
            throw ex;
        }
    }

    /** Create a gstreamer element and add it to the pipeline/bin, unwired.
        @param name The name of the element.
        @param overrides A user-specified alternate element to use.
        @return Reference with constructed element.
        @throw GstreamerAudioException if the element creation fails. */
    GstElement *GstreamerSimplePipeline::createElement (const char *name,
                                                        const std::string &overrides) {
        GstElement *element;
        if (overrides.empty()) {
            element = gst_element_factory_make (name, nullptr);
        } else {
            GError *error = nullptr;
            element = gst_parse_launch (overrides.c_str(), &error);
            if (error) {
                assert (!element);
                GstreamerAudioException ex ("gst_parse_launch", error, overrides.c_str());
                g_error_free (error);
                throw ex;;
            }
        }
        if (!element) {
            throw GstreamerAudioException ("gst_element_factory_make",
                                           "Element factory returned null pointer", name);
        }
        add (element);
        return element;
    }






    /*
     * gstreamer double pipeline
     */

    /** Wire an element to the end of the source pipeline/filter graph.
        Items are wired in the order they are pushed.
        @param item The item to insert into to the pipeline.
        @throw AudioException if the item can't be linked. */
    void GstreamerDoublePipeline::pushSource (GstElement *item) {
        if (input_pipeline_last) {
            if (!gst_element_link (input_pipeline_last, item)) {
                throw GstreamerAudioException ("pushSource: gst_element_link",
                                               "Could not link to previous element",
                                               gst_element_get_name (input_pipeline_last),
                                               gst_element_get_name (item));
            }
        }
        input_pipeline_last = item;
    }

    /** Push an output element into the filter graph.  The input elements are
        later wired to the output elements when the input elements trigger a
        pad-added signal, which is set up when the first output element is added. */
    void GstreamerDoublePipeline::push (GstElement *item) {
        assert (input_pipeline_last);
        assert (item);
        if (!signal_handler_id) {
            signal_handler_id = g_signal_connect (input_pipeline_last, "pad-added", G_CALLBACK (&GstreamerDoublePipeline::on_pad_added), this);
        }
        GstreamerSimplePipeline::push (item);
    }


    /** Callback function for pad-added signal. */
    void GstreamerDoublePipeline::on_pad_added (GstElement *,
                                                GstPad *pad,
                                                gpointer instance) {
        reinterpret_cast <GstreamerMediaReader *> (instance)->connectOutput (pad);
    }

    /** Handle the pad added signal by connecting the decoder to the output chain. */
    void GstreamerDoublePipeline::connectOutput(GstPad *) {
        if (chain_start) {
            if (!gst_element_link (input_pipeline_last, chain_start)) {
                throwDeferredException (GstreamerAudioException ("connectOutput: gst_element_link",
                                                             "Could not link to previous element",
                                                             gst_element_get_name (input_pipeline_last),
                                                             gst_element_get_name (chain_start)),
                                        true);
            } else {
                g_signal_handler_disconnect (input_pipeline_last, signal_handler_id);
            }
        } else {
            flog (LOG_WHERE (Log::WARNING|Log::AUDIO), "No modules pushed on the reader");
        }
    }


    /*
     * gstreamer media reader
     */


    /** Base abstract class for reading a media file or URL using gstreamer.
     @param media_url The filename or URL of the media.
     @param timeout The timeout for reading the media stream, in seconds. */
    GstreamerMediaReader::GstreamerMediaReader (const std::string &media_url,
                                                int timeout) :
    is_network (strncasecmp (media_url.c_str(), "http", 4) == 0),
    url (is_network ? media_url : (std::string ("file://") + media_url)) {
        const char *reader = is_network ? "souphttpsrc" : "filesrc";
        media_source = createElement (reader);

        // Specify the URI to read from
        g_object_set (media_source, "location", media_url.c_str(), NULL);

        if (is_network) {
            // Set properties on the reader
            g_object_set (media_source, "timeout", (guint) timeout, nullptr);
            g_object_set (media_source, "compress", true, nullptr);
            g_object_set (media_source, "keep-alive", true, nullptr);
        }
        pushSource (media_source);

        // Create a decoder
        decoder = createElement ("decodebin");

        // Ask for some buffering, larger if streaming over Interner
        g_object_set (decoder, "max-size-bytes", (guint) (is_network ? 2 * 1024 * 1024 : 256 * 1024), nullptr);
        pushSource (decoder);
        
    }
    




    /*
     * gstreamer pipeline with notification handlers
     */

    GstreamerReaderWithBus::GstreamerReaderWithBus (const std::string &media_url,
                                                    int timeout)
    : GstreamerMediaReader (media_url, timeout)
    {
        /// Wire up an event dispatch loop to the notifications bus.
        GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
        //guint bus_watch_id =
        gst_bus_add_watch (bus, on_bus_message, this);
        gst_object_unref (bus);
    }

    GstreamerReaderWithBus::~GstreamerReaderWithBus () {
    }

    gboolean GstreamerReaderWithBus::on_bus_message (GstBus *, GstMessage *message, gpointer instance) {
        return reinterpret_cast <GstreamerReaderWithBus *> (instance)->notificationReceived (message);
    }

    /** Handle an event notification from the stream */
    bool GstreamerReaderWithBus::notificationReceived (GstMessage *message) {
        switch (GST_MESSAGE_TYPE (message)) {
            case GST_MESSAGE_ERROR:
            {
                GError *error = nullptr;
                gchar *debug_message = nullptr;
                gst_message_parse_error (message, &error, &debug_message);
                const char *error_message = gst_error_get_message (error->domain, error->code);
                flog (LOG_WHERE (Log::ERROR), "Error: ", error_message);
                if (debug_message) {
                    flog (LOG_WHERE (Log::ERROR), "Additional: ", debug_message);
                }
                throwDeferredException (GstreamerAudioException ("notificationReceived (Error)", error,
                                                                 gst_element_get_name (pipeline)));
                return false;
            }
            case GST_MESSAGE_EOS:
                setPipelineState (GST_STATE_NULL);
                return false;
            case GST_MESSAGE_WARNING:
            {
                GError *error = nullptr;
                gchar *debug_message = nullptr;
                gst_message_parse_warning (message, &error, &debug_message);
                flog (LOG_WHERE (Log::ERROR), "Warning: ", gst_error_get_message(error->domain, error->code));
                if (debug_message) {
                    flog (LOG_WHERE (Log::ERROR), "Additional: ", debug_message);
                }
                break;
            }
            case GST_MESSAGE_INFO:
            {
                GError *error = nullptr;
                gchar *debug_message = nullptr;
                gst_message_parse_info (message, &error, &debug_message);
                flog (LOG_WHERE (Log::ERROR), "Information: ", gst_error_get_message(error->domain, error->code));
                if (debug_message) {
                    flog (LOG_WHERE (Log::ERROR), "Additional: ", debug_message);
                }
                break;
            }
            case GST_MESSAGE_CLOCK_LOST:
                flog (LOG_WHERE (Log::ERROR), "Pipeline has lost its clock");
                throwDeferredException (GstreamerAudioException ("notificationReceived", "pipeline lost its clock",
                                                                 gst_element_get_name (pipeline)));
                return false;
            default:
                break;
        }
        return notification (message);
    }


    bool GstreamerReaderWithBus::notification (GstMessage *) {
        return true;
    };





    /*
     * gstreamer player
     */


    /** Play a media file or URL using gstreamer.
        @param audio_settings Describe the output device.
        @param media_url The filename or URL of the media.
        @param initial_audio_gain Gain to apply when playing file, in decibels.
        If ReplayGain is encountered during playback, that is preferred
        over this value. */
    GstreamerPlayer::GstreamerPlayer (const AudioSettings &audio_settings,
                                      const std::string &media_url,
                                      float initial_audio_gain) :
    GstreamerReaderWithBus (media_url, 15),
    audio (audio_settings) {
        replay_gainer = createElement ("rgvolume");
        g_object_set (replay_gainer, "fallback_gain", (gdouble) initial_audio_gain, nullptr);
        push (replay_gainer);

        volume_filter = createElement ("volume");
        g_object_set (volume_filter, "volume", (gdouble) pow (10, (audio.volume) / 20), nullptr);
        push (volume_filter);
        
        format_converter = createElement ("audioconvert");
        push (format_converter);

        output_device = createElement (audio.output_driver.empty() ? "autoaudiosink" : audio.output_driver.c_str(),
                                       audio.output_device);
        push (output_device);
    };

    GstreamerPlayer::~GstreamerPlayer (void) {
    }



    void GstreamerPlayer::setVolume (float volume) {
        g_object_set (volume_filter, "volume", (gdouble) pow (10, (volume) / 20), nullptr);
    }

    float GstreamerPlayer::trackDuration (void) const {
        if (currentPipelineState() == GST_STATE_PLAYING ||
            currentPipelineState() == GST_STATE_PAUSED) {
            gint64 duration;
            if (gst_element_query_duration (pipeline, GST_FORMAT_TIME, &duration)) {
                return duration / 1E9;
            }
        }
        return -1;
    };

    float GstreamerPlayer::playPoint (void) const {
        if (currentPipelineState() == GST_STATE_PLAYING ||
            currentPipelineState() == GST_STATE_PAUSED) {
            gint64 position;
            if (gst_element_query_position (pipeline, GST_FORMAT_TIME, &position)) {
                return position / 1E9;
            }
        }
        return -1;
    }

    void GstreamerPlayer::pause (void) {
        setPipelineState (GST_STATE_PAUSED);
    }

    void GstreamerPlayer::play (void) {
        setPipelineState (GST_STATE_PLAYING);
    }

    void GstreamerPlayer::cue (void) {
        setPipelineState (GST_STATE_PAUSED);
    }

    void GstreamerPlayer::abort (void) {
        setPipelineState (GST_STATE_NULL);
    }

    Media::Player::State GstreamerPlayer::currentState (void) const {
        switch (currentPipelineState()) {
            case GST_STATE_PAUSED:
            case GST_STATE_PLAYING:
                return Player::State::Playing;
            case GST_STATE_NULL:
                return Player::State::Done;
            case GST_STATE_READY:
                return Player::State::Cueing;
            default:
                return Player::State::Initializing;
        }
    };

    RESPONSE_CODE GstreamerPlayer::completionStatus (void) {
        if (pipeline_exception)
            throw GstreamerAudioException { *pipeline_exception };
        return S_OK;
    }




    /*
     * gstreamer audio output
     */

    GstreamerOutput::GstreamerOutput (const AudioSettings &settings,
                                      const AudioFormat &format)
    : audio (settings) {
        application_source = createElement ("appsrc");

        std::string format_string { format.signedness == SampleSignedness::Signed ? 'S' : 'U' };
        format_string += std::to_string (format.bits);
        if (format.bits > 8) {
            format_string += format.realArrangement() == SampleArrangement::Little ? "LE" : "BE";
        }

        g_object_set (application_source, "max-latency", (gint64) 250E6, nullptr);
        g_object_set (application_source, "block", (gboolean) true, nullptr);
        g_object_set (application_source, "max-bytes", (guint64) 0x10000, nullptr);

        g_object_set (application_source, "caps",
                      gst_caps_new_simple ("audio/x-raw",
                                           "rate", G_TYPE_INT, format.rate,
                                           "channels", G_TYPE_INT, format.channels,
                                           "layout", G_TYPE_STRING, "interleaved",
                                           "format", G_TYPE_STRING, format_string.c_str(),
                                           nullptr), nullptr);
        push (application_source);

        format_converter = createElement ("audioconvert");
        push (format_converter);

        output_device = createElement (audio.output_driver.empty() ? "autoaudiosink" : audio.output_driver.c_str(),
                                       audio.output_device);
        push (output_device);

        setPipelineState (GST_STATE_PLAYING);
    }

    
    bool GstreamerOutput::play (void *buffer, unsigned number_of_bytes) {
        GstBuffer *buf = gst_buffer_new_allocate (nullptr, number_of_bytes, nullptr);
        if (buf) {
            gst_buffer_fill (buf, 0, buffer, number_of_bytes);
            GstFlowReturn status;
            g_signal_emit_by_name (application_source, "push-buffer", buf, &status);
            gst_buffer_unref (buf);
            return (status == GST_FLOW_OK);
        } else {
            flog (LOG_WHERE (Log::ERROR), "gst_buffer_new_allocate returned NULL: ", strerror (errno));
        }
        return false;
    }

}



