///
/// Output using ffmpeg's libavdevices libraries.
/// @file       avdeviceoutput.h - pianod2
/// @author     Perette Barella
/// @date       2016-02-04
/// @copyright  Copyright (c) 2016-2018 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <memory>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
#pragma GCC diagnostic pop

#include "ffmpegcpp.h"

#include "audio/audiooutput.h"

namespace Audio {
    /// Output class for libavdevice.
    class AvDeviceOutput : public Output {
        ffmpeg::AVCodecContext codec_context;
        ffmpeg::AVPacket packet;
        AVFormatContext *context = nullptr;
        AVStream *stream = nullptr;
        int min_flush_time = 0; ///< milliseconds
        int max_flush_time = 2000; ///< milliseconds
        const int flush_time_quanta = 100; ///< milliseconds
    public:
        AvDeviceOutput (const AudioSettings &settings,
                        const AudioFormat &format);
        virtual ~AvDeviceOutput ();
        virtual bool play (void *buffer, unsigned numberOfBytes) override;
    };

}

