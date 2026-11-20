///
/// Read media file metadata using gstreamer.
/// @file       gstreammetadata.h - pianod2
/// @author     Perette Barella
/// @date       2016-08-22
/// @copyright  Copyright © 2016 Devious Fish. All rights reserved.
///

#ifndef __pianod2__gstreammetadata__
#define __pianod2__gstreammetadata__

#include <config.h>

#include <mutex>
#include <condition_variable>

#include "metadata/metadata.h"
#include "../audio/gstreamplayer.h"

namespace Media {
    /// Read media stream metadata via gstreamer.
    class GstreamerMetadataReader : public Metadata, public Audio::GstreamerMediaReader {
    private:
        bool notification (GstMessage *message);
public:
        GstreamerMetadataReader (const std::string &path);
    };
}


#endif /* defined(__pianod2__gstreammetadata__) */
