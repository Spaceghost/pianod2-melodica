///
/// gstreamer player and metadata reader.
/// @file       gstreamplayer.h - pianod2
/// @author     Perette Barella
/// @date       2016-08-02
/// @copyright  Copyright © 2016-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>
#include <memory>
#include <mutex>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"
#pragma GCC diagnostic ignored "-Wshadow"
#include <gst/gst.h>
#pragma GCC diagnostic pop

#include "mediaplayer.h"
#include "audio/audiooutput.h"
#include "fundamentals.h"

namespace Audio {
    /// Exception for gstreamer audio output problems.
    class GstreamerAudioException : public AudioException {
    public:
        GstreamerAudioException (const char *func, const char *error,
                                 const char *what = nullptr, const char *what2 = nullptr);
        GstreamerAudioException (const char *func, GError *error, const char *what = nullptr);

//        GstreamerAudioException (const std::string &func, const Glib::ustring &error);
    };



    /// Base class for gstreamer pipelines.
    class GstreamerSimplePipeline {
    protected:
        GstElement *pipeline;         ///< Filter graph for elements.
        GstElement *chain_start = nullptr;   ///< First item pushed on the stream after setup
        GstElement *pipeline_last = nullptr; ///< Last item pushed on the stream

        void add (GstElement *element);

    protected:
        mutable std::recursive_mutex state_mutex;
        mutable std::unique_ptr<GstreamerAudioException> pipeline_exception; ///< Asynchronous exception.
        void throwDeferredException (const GstreamerAudioException &exception,
                                     bool asynchronously = false) const;
        inline void throwAsyncException (const GstreamerAudioException &exception) {
            throwDeferredException (exception, true);
        }

        void push (GstElement *item);
        GstElement *createElement (const char *name,
                                   const std::string &overrides = "");
        GstState currentPipelineState () const;
        void setPipelineState (GstState state);        

    public:
        GstreamerSimplePipeline ();
        virtual ~GstreamerSimplePipeline ();
    };

    /// Class for gstreamer pipelines that need sources wired to downstream
    /// elements dynamically when autoplugging.
    class GstreamerDoublePipeline : public GstreamerSimplePipeline {
        GstElement *input_pipeline_last = nullptr; ///< Last item pushed on the stream
        gulong signal_handler_id = 0;

        static void on_pad_added (GstElement *,
                                  GstPad *pad,
                                  gpointer instance);
        void connectOutput(GstPad *pad);

    protected:
        void push (GstElement *item);
        void pushSource (GstElement *item);

    public:
        GstreamerDoublePipeline () = default;
        virtual ~GstreamerDoublePipeline () = default;
    };


    /// Class for reading media streams via gstreamer
    class GstreamerMediaReader : public GstreamerDoublePipeline {
    protected:
        const bool is_network = false;              ///< True if media is network streaming.
        const std::string url;                      ///< Media URL or filename

    private:
        GstElement *media_source = nullptr;    ///< Source element.  Reads file or network.
        GstElement *decoder = nullptr;         ///< Autoplugger, which in turn wires up an audio decoder.

    public:
        GstreamerMediaReader (const std::string &media_url, int timeout = 15);
    };



    /// Base class for gstreamer pipelines, both readers and outputs.
    class GstreamerReaderWithBus : public GstreamerMediaReader {
    private:
        static gboolean on_bus_message (GstBus *bus, GstMessage *message, gpointer instance);
        bool notificationReceived (GstMessage *message);

    protected:
        virtual bool notification (GstMessage *message);

    public:
        GstreamerReaderWithBus (const std::string &media_url,
                                int timeout);
        virtual ~GstreamerReaderWithBus ();
    };






    /// Play media or a stream using gstreamer.
    class GstreamerPlayer : public GstreamerReaderWithBus, public Media::Player {
    private:
        AudioSettings audio; ///< Output settings

        GstElement *replay_gainer = nullptr;       ///< Apply replay gain as necessary.
        GstElement *volume_filter = nullptr;       ///< Make volume adjustments.
        GstElement *format_converter = nullptr;    ///< Convert audio to required format for output.
        GstElement *output_device = nullptr;       ///< Send audio to somewhere.

        // Regulate playback
        virtual void pause (void) override;
        virtual void abort (void) override;
        virtual void cue (void) override;
        virtual void play (void) override;
        virtual RESPONSE_CODE completionStatus (void) override;

        // Implement MediaPlayer API
        virtual void setVolume (float volume) override;
        virtual State currentState (void) const override;
        virtual float trackDuration (void) const override;
        virtual float playPoint (void) const override;

    public:
        GstreamerPlayer (const AudioSettings &audio_settings,
                         const std::string &media_url,
                         float initial_audio_gain);
        virtual ~GstreamerPlayer ();
    };


    
    /// Audio output class utilizing gstreamer for output.
    class GstreamerOutput : public Output, public GstreamerSimplePipeline {
    private:
        AudioSettings audio; ///< Output settings

        GstElement *application_source = nullptr;  ///< Element for application to push data into.
        GstElement *format_converter = nullptr;    ///< Convert audio to required format for output.
        GstElement *output_device = nullptr;       ///< Send audio to somewhere.

    public:
        GstreamerOutput (const AudioSettings &settings,
                         const AudioFormat &format);
        virtual bool play (void *buffer, unsigned numberOfBytes) override;
    };


}

