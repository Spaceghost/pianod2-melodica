///
/// Read media file metadata using taglib.
/// @file       taglibreader.h - pianod2
/// @author     Perette Barella
/// @date       2015-11-30
/// @copyright  Copyright (c) 2015-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include "metadata/metadata.h"

namespace Media {
    /// A class to extract metadata from media files using Taglib.
    class TaglibReader : public Metadata {
    public:
        TaglibReader (const std::string &path);
        static float getFileGain (const std::string &path);
    };
}

