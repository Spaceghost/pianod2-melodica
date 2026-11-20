///
/// Adapter from a ffmpeg audio player to a generic audio output.
/// @file       ffmpegadapter.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-11-18
/// @copyright  Copyright (c) 2015-2023 Devious Fish. All rights reserved.
///

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>

#include <stdexcept>
#include <chrono>
#include <thread>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}
#pragma GCC diagnostic pop

#include "fundamentals.h"
#include "utility.h"
#include "logging.h"
#include "audio/audiooutput.h"
#include "ffmpegplayer.h"


#if LIBAVCODEC_VERSION_MAJOR >= 60
#define HAVE_AV_CH_LAYOUT
#endif

namespace Audio {
    /** Log an ffmpeg error.
        @param func The function name causing the error.
        @param err The ffmpeg error number. */
    void flogav (const char *func, int err) {
        char buffer [256];
        av_strerror (err, buffer, sizeof (buffer));
        flog (Log::ERROR, func, ": ", buffer);
    }

    /** Create an AudioException from a ffmpeg error.
        @param func The name of the function originating the error.
        @param error The ffmpeg error number. */
    LavAudioException::LavAudioException (const char *func, int error) {
        char buffer [256];
        av_strerror (error, buffer, sizeof (buffer));
        reason = std::string (func) + ": " + buffer;
        flog (Log::ERROR, reason);
    }

    /*
     * LavAdapter - infastructure to adapt ffmpeg output to other things.
     */

    /** Add a filter to the filter stack.
        @param prior_filter The prior filter, or nullptr if it is the first.
        (used to wire the filters into sequence).
        @param name The ffmpeg name of the filter.
        @param options_format A printf-style format string (and values to follow)
        to create an options string for the filter.
        @return The new filter.
        @throw AudioException if anything goes wrong. */
    AVFilterContext *LavAdapter::pushFilter (AVFilterContext *prior_filter,
            const char *name,
            const char *options_format, ...) {
        assert (name);

        const AVFilter *filter_type = avfilter_get_by_name (name);
        if (!filter_type)
            throw AudioException (std::string ("Filter not found: ") + name);
        char *options = nullptr;
        if (options_format) {
            // If there are options, format a string for them.
            va_list parameters;
            va_start (parameters, options_format);
            int length = vasprintf (&options, options_format, parameters);
            if (length < 0)
                throw std::bad_alloc ();

        }
        AVFilterContext *new_filter = nullptr;
        int status = avfilter_graph_create_filter (&new_filter,
                     filter_type, NULL, options, NULL,
                     filter_stack.get());
        free (options);
        if (status < 0)
            throw LavAudioException ("avfilter_graph_create_filter", status);

        if (prior_filter) {
            status = avfilter_link (prior_filter, 0, new_filter, 0);
            if (status < 0) {
                throw LavAudioException ("avfilter_link", status);
            }
        }
        return new_filter;
    }

    /** Adapt whatever output for input from ffmpeg.
        @param codec The ffmpeg codec context.
        @param stream The stream from whence audio will arrive.
        @param output_format The format required by the outputter. */
    LavAdapter::LavAdapter (const AVCodecContext *codec,
                            const AVStream *stream,
                            const AVSampleFormat output_format) {
        assert (codec);
        assert (stream);

        int status;

        filter_stack.reset (avfilter_graph_alloc ());
        if (!filter_stack)
            throw std::bad_alloc();

        output_frame.reset (av_frame_alloc());
        if (!output_frame)
            throw std::bad_alloc();

        // Unput buffer
        AVRational time_base = stream->time_base;
        char channel_layout_name [256];
#ifdef HAVE_AV_CH_LAYOUT
        av_channel_layout_describe (&codec->ch_layout, channel_layout_name, sizeof (channel_layout_name));
#else
        av_get_channel_layout_string (channel_layout_name, sizeof (channel_layout_name),
                                      codec->channels, codec->channel_layout);
        // Provide sanity where ffmpeg's avcodec doesn't.
        if (codec->channel_layout == 0) {
            switch (codec->channels) {
                case 1:
                    strcpy (channel_layout_name, "mono");
                    break;
                case 2:
                    strcpy (channel_layout_name, "stereo");
                    break;
                case 4:
                    strcpy (channel_layout_name, "quad");
                    break;
                default:
                    // do nothing and let the filter creation.
                    break;
            }
        }
#endif
        buffer_filter = pushFilter (nullptr, "abuffer",
                                    "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
                                    time_base.num, time_base.den, codec->sample_rate,
                                    av_get_sample_fmt_name (codec->sample_fmt),
                                    channel_layout_name);

        // Volume filter
        volume_filter = pushFilter (buffer_filter, "volume");

        if (output_format != codec->sample_fmt) {
            /* Convert samples to required format */
            format_filter = pushFilter (volume_filter, "aformat",
                                        "sample_fmts=%s",
                                        av_get_sample_fmt_name (output_format));
        }

        // Buffer from which to retrieve filter output
        endpoint_filter = pushFilter(format_filter ? format_filter : volume_filter,
                                     "abuffersink");

        status = avfilter_graph_config (filter_stack.get(), NULL);
        if (status < 0) {
            throw LavAudioException ("avfilter_graph_config", status);
        }
    };

    LavAdapter::~LavAdapter() { }

    /** Play any frames buffered in the filter.
        This must be invoked by subclasses during their destruction, as their
        specializations are gone by the time this class's destructor is called. */
    void LavAdapter::flush () {
        play (NULL);
        const int quanta = 100;
        int timeout;
        bool status;
        for (timeout = 0; (status = playFilteredPackets (false)) && timeout < 2000; timeout += quanta) {
            std::this_thread::sleep_for (std::chrono::milliseconds (quanta));
        }
        if (status) {
            flog (LOG_WHERE (Log::WARNING|Log::AUDIO), "Filter did not fully flush.");
        } else if (timeout) {
            flog (LOG_WHERE (Log::AUDIO), "Filter took ", timeout, "ms to flush");

        }
    }

    /** Play a frame from a ffmpeg decoder.
        This involves passing it through the filter stack to adjust
        volume and format in accordance with the output's needs.
        @param frame The audio frame to play.
        @return True on success, false on error. */
    bool LavAdapter::play (AVFrame *frame) {
        // Push the frame through the filter stack
        std::lock_guard<std::mutex> lock (adapter_mutex);
        int status = av_buffersrc_write_frame (buffer_filter, frame);
        if (status < 0) {
            flogav("av_buffersrc_write_frame", status);
            return false;
        }

        return playFilteredPackets();
    }


    /** Read all the resulting frames coming out of the filter stack,
        and push them to the outputter.
        @param eof_return_value Value to return if EOF is encountered.
        @return On error, false.  On eof, up to caller. Otherwise true. */
    bool LavAdapter::playFilteredPackets (const bool eof_return_value) {
        bool ok;
        do {
            int status = av_buffersink_get_frame (endpoint_filter, output_frame.get());
            if (status == AVERROR_EOF)
                return eof_return_value;
            if (status == AVERROR (EAGAIN))
                return true;
            if (status < 0) {
                flogav("av_buffersink_get_frame", status);
                return false;
            }
            ok = playFrame (output_frame.get());
            av_frame_unref (output_frame.get());
        } while (ok);
        return false;
    }


    void LavAdapter::setVolume (float level) {
        /* Mutex is needed to prevent crash if player thread and main
           thread try to concurrently adjust volume (cross-fading
           makes this especially likely) or if the main thread tries
           to set the volume while the player thread is actively
           adding/removing packets from the filter. */
        std::lock_guard<std::mutex> lock (adapter_mutex);

        assert (filter_stack);
        assert (volume_filter);
        std::string gain = std::to_string (level) + "dB";
        int status = avfilter_graph_send_command (filter_stack.get(), "volume", "volume",
                     gain.c_str(), NULL, 0, 0);
        if (status < 0) {
            flogav("avfilter_graph_send_command", status);
        }
    }


    /*
     * LavGenericAdapter - Concrete adapter to non-ffmpeg outputs.
     */

    /** Adapt whatever output for input from ffmpeg.
        @param codec The ffmpeg codec context.
        @param stream A stream from whence audio will arrive.
        @param out An output mechanism.
        @param output_format The format required by the outputter. */
    LavGenericAdapter::LavGenericAdapter (const AVCodecContext *codec,
                                          const AVStream *stream,
                                          Output *out,
                                          const AVSampleFormat output_format) :
        LavAdapter (codec, stream, output_format), output (out) {
        assert (out);
    }

    LavGenericAdapter::~LavGenericAdapter() {
        flush();
    }
    
    /** Send a processed frame to the audio output device.
        @param frame The audio frame to play.
        @return True on success, false on error. */
    bool LavGenericAdapter::playFrame (AVFrame *frame) {
#ifdef HAVE_AV_CH_LAYOUT
        const int channels = frame->ch_layout.nb_channels;
#else
        const int channels = av_get_channel_layout_nb_channels (frame->channel_layout);
#endif
        const int sample_bytes = av_get_bytes_per_sample ((AVSampleFormat) frame->format);
        assert (sample_bytes * channels == output->bytesPerSample());
        return output->play (frame->data[0], frame->nb_samples * channels * sample_bytes);
    }

    /*
     * Factory to assemble output and adapter.
     */

    /** Choose an output, then get an adapter for libav
        and set it up to match the output's format.
        @param settings Where to send the audio output.
        @param codec Audio format details.
        @param stream The stream from which audio will arrive.
        @return A ready-to-go output stack.
        @throw AudioException or a derivative if a suitable output was
        not located, opening/setting up the device failed, or
        the adapter setup failed. */
    LavAdapter *LavAdapter::getOutput (const AudioSettings &settings,
                                       const AVCodecContext *codec,
                                       const AVStream *stream) {
        AudioFormat output_format;
        AVSampleFormat adapter_format;

        switch (codec->sample_fmt) {
            case AV_SAMPLE_FMT_U8:
            case AV_SAMPLE_FMT_U8P:
                output_format.bits = 8;
                output_format.signedness = SampleSignedness::Unsigned;
                adapter_format = AV_SAMPLE_FMT_U8;
                break;
            case AV_SAMPLE_FMT_S16:
            case AV_SAMPLE_FMT_S16P:
                adapter_format = AV_SAMPLE_FMT_S16;
                break;
            case AV_SAMPLE_FMT_S32:
            case AV_SAMPLE_FMT_S32P:
            default: // Floats, doubles and 32-bit formats
                output_format.bits = 32;
                adapter_format = AV_SAMPLE_FMT_S32;
                break;
        }
#ifdef HAVE_AV_CH_LAYOUT
        output_format.channels = codec->ch_layout.nb_channels;
#else
        output_format.channels = codec->channels;
#endif
        output_format.rate = codec->sample_rate;

        Output *out = nullptr;
        try {
            out = Output::getOutput (settings, output_format);
        } catch (const AudioException &e) {
            // Some outputs may not support the more eclectic combinations.
            // Consider something commonplace.
            if (output_format.bits == 16 &&
                    output_format.signedness == SampleSignedness::Signed) {
                throw;
            }
            output_format.bits = 16;
            output_format.signedness = SampleSignedness::Signed;
            adapter_format = AV_SAMPLE_FMT_S16;
        }
        if (!out)
            out = Output::getOutput (settings, output_format);
        if (!out)
            throw AudioException ("Could not acquire audio output");
        try {
            return new LavGenericAdapter (codec, stream, out, adapter_format);
        } catch (...) {
            delete out;
            throw;
        }
    }
}
