///
/// Read media file metadata using ffmpeg.
/// @file       ffmpegmetadata.h - pianod2
/// @author     Perette Barella
/// @date       2015-12-01
/// @copyright  Copyright (c) 2015-2016 Devious Fish. All rights reserved.
///

#ifndef __pianod2__ffmpegmetadata__
#define __pianod2__ffmpegmetadata__

#include <config.h>

#include "metadata/metadata.h"
#include "../audio/ffmpegplayer.h"

namespace Media {
    /// Read media stream metadata via ffmpeg
    class LavMetadataReader : public Metadata, public Audio::LibavMediaReader {
    public:
        virtual void setGain (float gain) override;
        LavMetadataReader (const std::string &path);
    };
}


#endif /* defined(__pianod2__ffmpegmetadata__) */
