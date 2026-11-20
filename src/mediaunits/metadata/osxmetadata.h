///
/// Read media file metadata using AVFoundation.
/// @file       osxmetadata.h - pianod2
/// @author     Perette Barella
/// @date       2015-11-30
/// @copyright  Copyright (c) 2015-2017 Devious Fish. All rights reserved.
///

#ifndef __pianod2__osxmetadata__
#define __pianod2__osxmetadata__

#include <config.h>

#include "metadata/metadata.h"

namespace Media {
    /// A class to extract metadata from media files using AVFoundation.
    class OSXMetadataReader : public Metadata {
    public:
        OSXMetadataReader (const std::string &path);
    };
}


#endif /* defined(__pianod2__osxmetadata__) */
