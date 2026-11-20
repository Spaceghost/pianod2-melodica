///
/// Output using ffmpeg's libavdevices libraries.
/// @file       avdeviceoutput.cpp - pianod2
/// @author     Perette Barella
/// @date       2016-02-04
/// @copyright  Copyright (c) 2016-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <chrono>
#include <thread>
#include <exception>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavutil/avutil.h>
#include <libavutil/avstring.h>
#include <libavutil/opt.h>
}
#pragma GCC diagnostic pop

#include "utility.h"
#include "logging.h"

#include "avdeviceoutput.h"
#include "ffmpegplayer.h"

#if LIBAVCODEC_VERSION_MAJOR >= 60
#define HAVE_AV_CH_LAYOUT
#endif

namespace Audio {
    /** Find the audio output device.  If a driver is specified in the settings,
        use that.  Otherwise, use the first device found.
        @param settings The audio settings to match. */
    static const AVOutputFormat *findDevice (const AudioSettings &settings) {
        const AVOutputFormat *output = nullptr;
#ifdef HAVE_AV_OUTPUT_AUDIO_DEVICE_NEXT
        if (settings.output_driver.empty()) {
            output = av_output_audio_device_next (nullptr);
            if (output) return output;
        }
#endif
#ifdef HAVE_AV_MUXER_ITERATE
        void *muxer_iterator = nullptr;
        while ((output = av_muxer_iterate (&muxer_iterator))) {
#else
        while ((output = av_oformat_next (output))) {
#endif
            if ((settings.output_driver.empty() && output->flags & AVFMT_NOFILE) ||
                    strcasecmp (settings.output_driver, output->name) == 0) {
                return output;
            }
        }
        return nullptr;
    };


    /** Create an audio outputter for ffmpeg's libavdevice.
        @param settings The output settings.
        @param format The format of the data to be played. */
    AvDeviceOutput::AvDeviceOutput (const AudioSettings &settings,
                                    const AudioFormat &format) {
        const AVOutputFormat *device = findDevice (settings);
        if (!device)
            throw std::invalid_argument (settings.output_driver.empty() ?
                                    "No audio output found" :
                                    "Requested audio output not found.");

        // open the output file, if needed */
        if (!(device->flags & AVFMT_NOFILE)) {
            if (settings.output_server.empty())
                throw AudioException ("No output destination specified.");

        }

        int status = avformat_alloc_output_context2 (&context,
                     const_cast <AVOutputFormat *> (device),
                     device->name, settings.output_server.c_str());
        if (status < 0)
            throw LavAudioException ("avformat_alloc_output_context2", status);

        try {
            bytes_per_sample_set = format.sampleGroupSize();
            const AVCodec *codec = avcodec_find_encoder (device->audio_codec);
            assert (device->audio_codec == AV_CODEC_ID_NONE || codec);

            stream = avformat_new_stream (context, codec);
            if (!stream)
                throw std::bad_alloc();

            AVCodecParameters *codec_parameters = stream->codecpar;
            if (codec_parameters->codec_type == AVMEDIA_TYPE_UNKNOWN) {
                codec_parameters->codec_type = AVMEDIA_TYPE_AUDIO;
            }

            // Set in parameter structure and copy into context at end.
            codec_parameters->bit_rate = bytes_per_sample_set * format.rate;
            codec_parameters->sample_rate = format.rate;
#ifdef HAVE_AV_CH_LAYOUT
            codec_parameters->ch_layout.nb_channels = format.channels;
#else
            codec_parameters->channels = format.channels;
#endif
            if (!format.isNativeArrangement())
                throw AudioException ("Non-native byte ordering not supported");
            switch (format.bits) {
                case 8:
                    if (format.signedness == Signed)
                        throw AudioException ("Signed 8-bit format not supported");
                    codec_parameters->format = AV_SAMPLE_FMT_U8;
                    break;
                case 16:
                    if (format.signedness == Unsigned)
                        throw AudioException ("Unsigned 16-bit format not supported");
                    codec_parameters->format = AV_SAMPLE_FMT_S16;
                    break;
                case 32:
                    if (format.signedness == Unsigned)
                        throw AudioException ("Unsigned 32-bit format not supported");
                    codec_parameters->format = AV_SAMPLE_FMT_S32;
                    break;
                default:
                    throw AudioException (std::to_string (format.bits) + "-bit format not supported");
            }

            packet.reset (av_packet_alloc ());
            if (!packet) {
                throw AudioException ("av_packet_alloc: allocation failed");
            }
            
            codec_context.reset (avcodec_alloc_context3 (codec));
            if (!codec_context) {
                throw AudioException ("avcodec_alloc_context3: allocation failed");
            }

            status = avcodec_parameters_to_context (codec_context.get(), codec_parameters);
            if (status < 0) {
                throw LavAudioException ("avcodec_parameters_to_context", status);
            }

            // some formats want stream headers to be separate
            if (context->oformat && context->oformat->flags & AVFMT_GLOBALHEADER) {
#ifdef CODEC_FLAG_GLOBAL_HEADER
                codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
#else
                codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
#endif
            }

            av_dump_format (context, 0, settings.output_driver.empty() ?
                            "Default audio output driver" : settings.output_driver.c_str(), 1);

            AVDictionary *options = nullptr;
            status = av_dict_parse_string (&options, settings.output_options.c_str(), "=", ";", 0);
            if (status < 0) {
                av_dict_free (&options);
                throw LavAudioException ("av_dict_parse_string", status);
            }

            if (auto entry = av_dict_get (options, "min_flush_time", NULL, 0)) {
                min_flush_time = strtol (entry->value, NULL, 10);
            }

            if (auto entry = av_dict_get (options, "max_flush_time", NULL, 0)) {
                max_flush_time = strtol (entry->value, NULL, 10);
            }

            status = avcodec_open2 (codec_context.get(), codec, &options);
            av_dict_free (&options);
            if (status < 0) {
                throw LavAudioException ("avcodec_open2", status);
            }

            try {
                status = avformat_write_header (context, nullptr);
                if (status < 0)
                    throw LavAudioException ("avformat_write_header", status);
            } catch (...) {
                throw;
            }
        } catch (...) {
            avformat_free_context (context);
            throw;
        }
    }

    AvDeviceOutput::~AvDeviceOutput () {
        int status, timeout;
        std::this_thread::sleep_for (std::chrono::milliseconds (min_flush_time));
        for (timeout = min_flush_time;
                (status = av_write_frame (context, NULL)) == 0 && timeout < max_flush_time;
                timeout += flush_time_quanta) {
            std::this_thread::sleep_for (std::chrono::milliseconds (flush_time_quanta));
        }
        if (status < 0) {
            flogav ("av_write_frame", status);
        } else if (status == 0) {
            flog (LOG_WHERE (Log::WARNING|Log::AUDIO), "Output did not fully flush.");
        } else if (timeout > min_flush_time) {
            flog (LOG_WHERE (Log::AUDIO), "Output took ", timeout, "ms to flush");
        }

        // std::this_thread::sleep_for (std::chrono::milliseconds (2000));
        if ((status = av_write_trailer (context)) != 0) {
            flogav ("av_write_trailer", status);
        }
    }

    /** Play output. */
    bool AvDeviceOutput::play (void *buffer, unsigned numberOfBytes) {
        packet->data = (uint8_t *) buffer;
        packet->size = numberOfBytes;
        int status = av_write_frame (context, packet.get());
        if (status != 0)
            flogav("av_write_frame", status);
        return (status == 0);
    }
}
