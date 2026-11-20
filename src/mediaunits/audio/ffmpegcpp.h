///
/// Wrap some of ffmpeg's types in C++ to take advantage of RAII.
/// @file       ffmpegcpp.h - pianod2
/// @author     Perette Barella
/// @date       2025-12-28
/// @copyright  Copyright (c) 2025 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <memory>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
}
#pragma GCC diagnostic pop

#if LIBAVCODEC_VERSION_MAJOR <= 60
#define HAVE_AV_CODEC_CLOSE
#endif

namespace ffmpeg {
    struct CodecDeleter {
        inline void operator() (::AVCodecContext *context) {
#ifdef HAVE_AV_CODEC_CLOSE
            avcodec_close (context);
#endif
            avcodec_free_context (&context);
        }
    };
    using AVCodecContext = std::unique_ptr<::AVCodecContext, CodecDeleter>;

    struct PacketDeleter {
        inline void operator() (::AVPacket *frame) {
            av_packet_free (&frame);
        }
    };
    using AVPacket = std::unique_ptr<::AVPacket, PacketDeleter>;

    struct FilterGraphDeleter {
        inline void operator() (AVFilterGraph *filter) {
            avfilter_graph_free (&filter);
        }
    };
    using AVFilterGraph = std::unique_ptr<::AVFilterGraph, FilterGraphDeleter>;

    struct FrameDeleter {
        inline void operator() (AVFrame *frame) {
            av_frame_free (&frame);
        }
    };
    using AVFrame = std::unique_ptr<::AVFrame, FrameDeleter>;
}  // namespace ffmpeg
