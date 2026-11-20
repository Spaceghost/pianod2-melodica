///
/// ffmpeg audio player and metadata reader.
/// @file       ffmpegplayer.h - pianod2
/// @author     Perette Barella
/// @date       2015-03-02
/// @copyright  Copyright (c) 2015-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>
#include <memory>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#ifdef HAVE_LIBAVFILTER_AVFILTERGRAPH_H
#include <libavfilter/avfiltergraph.h>
#endif
#include <libavutil/replaygain.h>
}
#pragma GCC diagnostic pop

#include "ffmpegcpp.h"

#include "mediaplayer.h"
#include "audio/audiooutput.h"
#include "fundamentals.h"

namespace Audio {

    extern void flogav (const char *func, int err);

    /// Exception for ffmpeg audio output problems.
    class LavAudioException : public AudioException {
    public:
        LavAudioException (const char *func, int error);
    };


    /// Adapter to feed a ffmpeg stream to an audio output.
    class LavAdapter {
        AVFilterContext *buffer_filter = nullptr;
        AVFilterContext *volume_filter = nullptr;
        AVFilterContext *format_filter = nullptr;
        AVFilterContext *endpoint_filter = nullptr;
        ffmpeg::AVFilterGraph filter_stack;

        // Used to retrieve data from the endpoint filter.
        // This should be local, but needs to be alloced/dealloced.
        ffmpeg::AVFrame output_frame;
        std::mutex adapter_mutex;

        AVFilterContext *pushFilter (AVFilterContext *prior_filter,
                                     const char *name,
                                     const char *optionformat = nullptr, ...);

    protected:
        LavAdapter (const AVCodecContext *codec,
                    const AVStream *stream,
                    const AVSampleFormat output_format);
        virtual bool playFrame (AVFrame *frame) = 0;
        bool playFilteredPackets (const bool eof_return_value = true);
        void flush ();
    public:
        virtual ~LavAdapter ();
        bool play (AVFrame *frame);
        void setVolume (float level);

        static LavAdapter *getOutput (const AudioSettings &settings,
                                      const AVCodecContext *context,
                                      const AVStream *stream);
    };


    /// Adapter to feed a ffmpeg stream to a generic audio output.
    class LavGenericAdapter : public LavAdapter {
        std::unique_ptr <Output> output;

    public:
        LavGenericAdapter (const AVCodecContext *codec, const AVStream *stream,
                           Output *out, const AVSampleFormat output_format);
        ~LavGenericAdapter ();
        virtual bool playFrame (AVFrame *frame) override;
    };

    /// Base class for reading media streams via ffmpeg
    class LibavMediaReader {
    protected:
        const std::string url; ///< Media URL or filename
        bool is_network = false; ///< True if media is network streaming.
        AVFormatContext *transport = nullptr;

        // Private functions
        int initializeStream (ffmpeg::AVCodecContext &codec_context);
        void processReplayGain (AVReplayGain *gain, int size);
        virtual void setGain (float gain) = 0;
    public:
        LibavMediaReader (const std::string &media_url, int timeout = 15);
        virtual ~LibavMediaReader (void);
    };



    /// ffmpeg audio player
    class LavPlayer: public Media::ThreadedPlayer, public LibavMediaReader {
    private:
        AudioSettings audio; ///< Output settings
        float gain = 0; ///< Replay gain from track

        volatile float duration = -1; ///< Length of song, in seconds
        volatile float playpoint = 0; ///< Current point in song, in seconds
        volatile Media::Player::State state = Initializing;
        std::unique_ptr <Audio::LavAdapter> output;

        // Private functions
        bool playPacket (AVFrame *frame,
                         AVCodecContext *codec,
                         AVPacket *packet);
        RESPONSE_CODE playStream (AVFormatContext *format,
                                  AVCodecContext *codec,
                                  const int audio_stream);
        virtual void setGain (float gain) override;

        // Implement MediaPlayer API
        virtual void setVolume (float volume) override;
        virtual State currentState (void) const override;
        virtual float trackDuration (void) const override;
        virtual float playPoint (void) const override;
        virtual RESPONSE_CODE playerThread (void) override;
        virtual void pausing (void) override;
        virtual void resuming (void) override;
    public:
        LavPlayer (const AudioSettings &AudioSettings,
                   const std::string &media_url,
                   float audio_gain = 0);
    };

}

