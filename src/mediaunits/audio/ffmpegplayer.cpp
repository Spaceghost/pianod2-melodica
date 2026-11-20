///
/// ffmpeg audio player and metadata reader.
/// @file       ffmpegplayer.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-03-02
/// @copyright  Copyright (c) 2015-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <stdio.h>

#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"
#pragma GCC diagnostic ignored "-Wdeprecated"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/replaygain.h>
#include <libavfilter/avfilter.h>
#ifdef HAVE_LIBAVFILTER_AVFILTERGRAPH_H
#include <libavfilter/avfiltergraph.h>
#endif
#include <libavutil/dict.h>
}
#pragma GCC diagnostic pop

#include "ffmpegcpp.h"

#include "fundamentals.h"
#include "logging.h"
#include "mediaplayer.h"
#include "audio/audiooutput.h"
#include "ffmpegplayer.h"

#if LIBAVCODEC_VERSION_MAJOR >= 60
#define BUFFER_SIZE_TYPE size_t
#else
#define BUFFER_SIZE_TYPE    int
#endif

#if LIBAVCODEC_VERSION_MAJOR < 60
#define HAVE_AV_SIDE_DATA_IN_STREAM
#endif


namespace Audio {

    /** Base abstract class for reading a media file or URL using ffmpeg.
        @param media_url The filename or URL of the media.
        @param timeout The timeout for reading the media stream, in seconds. */
    LibavMediaReader::LibavMediaReader (const std::string &media_url,
                                        int timeout) :
    url (media_url) {
        is_network = (strncasecmp (media_url.c_str(), "http", 4) == 0);

        std::string timeout_str = std::to_string (timeout * 1000000); // microseconds
        AVDictionary *options = nullptr;
        av_dict_set (&options, "timeout", timeout_str.c_str(), 0);
        // Request 2MB TCP/IP buffering
        std::string buffer_str = std::to_string (1024*1024*2);
        if (is_network) {
            av_dict_set (&options, "recv_buffer_size", buffer_str.c_str(), 0);
        }

        int status = avformat_open_input (&transport, url.c_str(), nullptr, &options);
        av_dict_free (&options);
        if (status < 0) {
            throw LavAudioException ("avformat_open_input", status);
        }
    }


    LibavMediaReader::~LibavMediaReader (void) {
        avformat_close_input (&transport);
    }


    /** Process a replaygain packet by extracting the track gain,
        or if not found then then the album gain. */
    void LibavMediaReader::processReplayGain (AVReplayGain *gain_info, int size) {
        assert (gain_info);
        if (size != sizeof (AVReplayGain)) {
            flog (LOG_WHERE (Log::WARNING),
                  "Replay gain packet is ", size, "bytes, expected", sizeof (AVReplayGain));
            return;
        }

        // Docs say gain is in "microbels", millionths (1E-6) but also
        // instruct division by 100,000 (so 1E-5).
        float gain;
        if (gain_info->track_gain != INT32_MIN) {
            gain = gain_info->track_gain / 100000.0;
        } else if (gain_info->album_gain != 0) { // Yay consistency!
            gain = gain_info->album_gain / 100000.0;
        } else {
            flog (LOG_WHERE (Log::WARNING|Log::AUDIO), "Received replay gain with null values");
            return;
        }
        setGain (gain);
    }



    /** Prepare a ffmpeg source for playing its audio stream.
        Chooses codec, extracts replay gain if available, and sets
        streams other than the audio are set to ignore.
        @param codec_context On return, is set to an initialized
        codec_context with appropriate codec selected for the media.
        @return The audio stream index.
        @throw LavAudioException if something goes wrong. */
    int LibavMediaReader::initializeStream (ffmpeg::AVCodecContext &codec_context) {
        int status = avformat_find_stream_info (transport, nullptr);
        if (status < 0){
            throw LavAudioException ("avformat_find_stream_info", status);
        }

        if (logging_enabled (Log::AUDIO)) {
            av_dump_format (transport, 0, url.c_str(), false);
        }
        // Find the audio stream within multiplexed data
        unsigned audio_stream = av_find_best_stream (transport, AVMEDIA_TYPE_AUDIO,
                                                -1, -1, // Not stream/related stream
                                                NULL, 0);
        if (audio_stream < 0)
            throw LavAudioException ("av_find_best_stream", status);

        // Don't listen on other streams
        for (unsigned i = 0; i < transport->nb_streams; i++) {
            if (i != audio_stream) {
                transport->streams [i]->discard = AVDISCARD_ALL;
            }
        }
    
        // Find an appropriate codec for the stream
        AVCodecParameters *audio_parameters = transport->streams [audio_stream]->codecpar;
        AVCodecID codec_id =  audio_parameters->codec_id;
        const AVCodec *codec = avcodec_find_decoder (codec_id);
        if (codec == NULL)
            throw AudioException ("Codec not found");
        
        // Get the replay gain from the side data
#ifdef HAVE_AV_SIDE_DATA_IN_STREAM
        const AVStream *stream = transport->streams [audio_stream];
        const AVPacketSideData *replay_gain = nullptr;
        for (int i = 0; i < stream->nb_side_data; i++) {
            if (stream->side_data [i].type == AV_PKT_DATA_REPLAYGAIN) {
                replay_gain = &stream->side_data[i];
            }
        }
#else
        const AVPacketSideData *replay_gain = av_packet_side_data_get (audio_parameters->coded_side_data,
                                                                       audio_parameters->nb_coded_side_data,
                                                                       AV_PKT_DATA_REPLAYGAIN);
#endif
        if (replay_gain) {
            processReplayGain ((AVReplayGain *) replay_gain->data, replay_gain->size);
        }

        codec_context.reset (avcodec_alloc_context3 (codec));
        if (!codec_context) {
            throw AudioException ("avcodec_alloc_context3 returned null");
        }
        status = avcodec_parameters_to_context(codec_context.get(), audio_parameters);
        if (status < 0) {
            throw LavAudioException ("avcodec_parameters_to_context", status);
        }

        // Initialize the codec
        status = avcodec_open2 (codec_context.get(), codec, nullptr);
        if (status < 0)
            throw LavAudioException ("avcodec_open2", status);

        return audio_stream;
    }

    


    /** Play a media file or URL using ffmpeg.
        @param AudioSettings Describe the output device.
        @param media_url The filename or URL of the media.
        @param audio_gain Gain to apply when playing file, in decibels.
        If ReplayGain is encountered during playback, that is preferred
        over this value. */
    LavPlayer::LavPlayer (const AudioSettings &AudioSettings,
                              const std::string &media_url,
                              float audio_gain) :
    LibavMediaReader (media_url), audio (AudioSettings), gain (audio_gain) {
    };

    void LavPlayer::setVolume (float volume) {
        audio.volume = volume;
        if (output) {
            output->setVolume (audio.volume + gain);
        }
    }

    float LavPlayer::trackDuration (void) const {
        return duration;
    };

    float LavPlayer::playPoint (void) const {
        return playpoint;
    }

    Media::Player::State LavPlayer::currentState (void) const  {
        return state;
    };

    /** Called when the playback thread is pausing. */
    void LavPlayer::pausing (void) {
        if (is_network) {
            av_read_pause (transport);
       }
    }
    /** Called when the playback thread is resuming playback. */
    void LavPlayer::resuming (void) {
        if (is_network) {
            av_read_play (transport);
        }
    }

    void LavPlayer::setGain (float new_gain) {
        // If the output is open, set the volume there.
        // Otherwise, it will be set when the output is opened.
        gain = new_gain;
        if (output) {
            output->setVolume (gain + audio.volume);
        }
        flog (LOG_WHERE (Log::AUDIO), "Audio stream replaygain: ", gain, "dB");
    }



    /** @internal
        Decode and play an audio frame from ffmpeg.
        @param frame One frame from the media stream.
        @param codec The codec state for decoding the stream.
        @param packet A packet from the stream, undecoded. */
    bool LavPlayer::playPacket (AVFrame *frame,
                                AVCodecContext *codec,
                                AVPacket *packet) {
        // Check for side-band replay gain data
        BUFFER_SIZE_TYPE size;
        auto gain_info = (AVReplayGain *) av_packet_get_side_data (packet,
                                                                   AV_PKT_DATA_REPLAYGAIN,
                                                                   &size);
        if (gain_info) {
            processReplayGain (gain_info, size);
        }

        int status = avcodec_send_packet (codec, packet);
        if (status != 0) {
            flogav("avcodec_send_packet", status);
            return false;
        }

        while (true) {
            status = avcodec_receive_frame (codec, frame);
            if (status == AVERROR (EAGAIN))
                return true;
            if (status != 0) {
                flogav("avcodec_receive_frame", status);
                return false;
            }
            output->play (frame);
        };
    }

        struct FrameDeleter {
            void operator()(AVFrame *frame) {
                av_frame_free (&frame);
            }
        };

        struct PacketDeleter {
            void operator()(AVPacket *frame) {
                av_packet_free (&frame);
            }
        };
    /** @internal
        Play a media stream from ffmpeg.
        @param format The multiplexed media thing.
        @param codec The codec context for the audio stream.
        @param audio_stream The index of the audio stream.
        @return Pianod S_OK or a F_* error code. */
    RESPONSE_CODE LavPlayer::playStream (AVFormatContext *format,
                                           AVCodecContext *codec,
                                           const int audio_stream) {
        assert (format);
        assert (codec);
        assert (output);

        std::unique_ptr<AVFrame, FrameDeleter> frame { av_frame_alloc () };
        if (!frame)
            return F_RESOURCE;

        std::unique_ptr<AVPacket, PacketDeleter> packet { av_packet_alloc () };
        if (!packet)
            return F_RESOURCE;

        int status;
        while ((status = av_read_frame (format, packet.get())) == 0) {
            if (packet->stream_index == audio_stream) {
                playPacket (frame.get(), codec, packet.get());
            }
            playpoint = (av_q2d (format->streams [audio_stream]->time_base) * packet->pts);
            av_packet_unref (packet.get());
            if (checkForPauseOrQuit()) {
                break;
            }
        }
        if (status != 0 && status != AVERROR_EOF) {
            flogav ("av_read_frame", status);
            return F_FAILURE;
        }
        return S_OK;
    }

    RESPONSE_CODE LavPlayer::playerThread (void) {
        RESPONSE_CODE response = F_FAILURE;

        try {
            ffmpeg::AVCodecContext codec;
            int audio_stream = initializeStream (codec);
            if (transport->streams [audio_stream]->duration != AV_NOPTS_VALUE) {
                duration = (av_q2d (transport->streams [audio_stream]->time_base) *
                            (double) transport->streams [audio_stream]->duration);
            }

            // Open the output.
            output.reset (LavAdapter::getOutput (audio, codec.get(), transport->streams [audio_stream]));
            if (output) {
                output->setVolume (audio.volume);
                state = Cueing;
                if (!checkForPauseOrQuit()) {
                    state = Playing;
                    response = playStream (transport, codec.get(), audio_stream);
                }
            }
        } catch (...) {
            state = Done;
            throw;
        }
        state = Done;
        return response;
    }


}
