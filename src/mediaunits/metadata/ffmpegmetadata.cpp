///
/// Read media file metadata using ffmpeg.
/// @file       ffmpegmetadata.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-12-01
/// @copyright  Copyright (c) 2015-2016 Devious Fish. All rights reserved.
///

#include <config.h>

#include <string>
#include <memory>

#include "metadata/metadata.h"
#include "ffmpegmetadata.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/replaygain.h>
#include <libavutil/dict.h>
}

#include "ffmpegcpp.h"

// This is hacky, but we need the base class of the audio player
#include "../audio/ffmpegplayer.h"

namespace Media {

    LavMetadataReader::LavMetadataReader (const std::string &path) :
    Audio::LibavMediaReader (path, 3) {
        ffmpeg::AVCodecContext codec;
        int audio_stream = initializeStream (codec);
        if (transport->streams [audio_stream]->duration != AV_NOPTS_VALUE) {
            duration = (av_q2d (transport->streams [audio_stream]->time_base) *
                        (double) transport->streams [audio_stream]->duration);
        }

        av_dump_format(transport, 0, url.c_str(), false);
        try {
            AVDictionaryEntry *item;
            item = av_dict_get (transport->metadata, "title", nullptr, 0);
            if (item) {
                title = item->value;
            }
            item = av_dict_get (transport->metadata, "album", nullptr, 0);
            if (item) {
                album = item->value;
            }
            item = av_dict_get (transport->metadata, "artist", nullptr, 0);
            if (item) {
                artist = item->value;
            }
            item = av_dict_get (transport->metadata, "genre", nullptr, 0);
            if (item) {
                genre = item->value;
            }
            item = av_dict_get (transport->metadata, "date", nullptr, 0);
            if (item) {
                year = strtol (item->value, nullptr, 10);
            }
            item = av_dict_get (transport->metadata, "track", nullptr, 0);
            if (item) {
                splitOf(item->value, &track_number, &track_count);
            }
            item = av_dict_get (transport->metadata, "TPA", nullptr, 0);
            if (item) {
                splitOf(item->value, &disc_number, &disc_count);
            }
        }
    }

    void LavMetadataReader::setGain (float new_gain) {
        gain = new_gain;
    }

}
