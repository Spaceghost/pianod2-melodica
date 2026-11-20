///
/// Metadata support.
/// Extract metadata from media files and return in a standard format.
/// @file       metadata.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-11-30
/// @copyright  Copyright (c) 2015-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include "logging.h"

#include <cassert>
#include <cmath>

#include "metadata/metadata.h"

#ifdef WITH_TAGLIB
#include "taglibreader.h"
#elif defined (WITH_GSTREAMER)
#include "gstreammetadata.h"
#elif defined (WITH_FFMPEG)
#include "ffmpegmetadata.h"
#elif defined (WITH_AVFOUNDATION)
#include "osxmetadata.h"
#endif


namespace Media {

    /** Get a file's gain for playback.  This is currently done only if:
        - taglib is compiled in.
        - ffmpeg is the media engine.
        AVFoundation does this itself; taglib only improves over
        ffmpeg when there is iTunes normalization and *not*
        replay gain in the file's metadata. */
    float Metadata::getFileGain (const std::string &filename) {
#if defined(WITH_FFMPEG) && defined(WITH_TAGLIB)
        return TaglibReader::getFileGain (filename);
#else
        (void) filename;
        return 0.0f;
#endif
    };

    /** Get metadata using whatever best mechanism is available. */
    const Metadata Metadata::getMetadata (const std::string &filename) {
#ifdef WITH_TAGLIB
        return TaglibReader { filename };  // Holy slicing Batman!
#elif defined (WITH_GSTREAMER)
        try {
            return GstreamerMetadataReader {filename};
        } catch (const Audio::AudioException &e) {
            throw MediaException (e.what ());
        }
#elif defined (WITH_FFMPEG)
        try {
            return LavMetadataReader {filename};
        } catch (const Audio::LavAudioException &e) {
            throw MediaException (e.what());
        }
#elif defined (WITH_AVFOUNDATION)
        return OSXMetadataReader { filename };
#endif
        assert (!"Compiled without a metadata reader.");
        throw MediaException ("No metadata reader available");
    };

    /** Split a string `<number>/<number>` into its two components.
        @param value The string to split.
        @param first The value before the slash, or the single value.
        @param second The value after a slash, if present. */
    void Metadata::splitOf (const char *value, int *first, int *second) {
        int num, count;
        int matched = sscanf (value, "%d/%d", &num, &count);
        if (matched >= 1) {
            *first = num;
        }
        if (matched == 2) {
            *second = count;
        }
    }

    /** Get the iTunes normalization, a/k/a sound check.
        @param value The value from the iTunNORM ID3 comment.
        @return The gain specified by iTunNORM. */
    float Metadata::gainFromiTunesNormalization (const char *value) {
        long values [4];
        if (sscanf(value, "%lx %lx %lx %lx",
                   &values [0], &values [1], &values [2], &values [3]) == 4) {
            if (values [0] > 0) {
                float gain = -10 * log10 (double (values [0]) / 1000.0);
                flog (LOG_WHERE (Log::AUDIO), "Using iTunNORM gain ", gain, "dB");
                return gain;
            }
        }
        return 0.0;
    }

}
