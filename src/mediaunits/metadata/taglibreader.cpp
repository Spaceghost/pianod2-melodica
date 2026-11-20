///
/// Read media file metadata using taglib.
/// @file       taglibreader.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-11-30
/// @copyright  Copyright (c) 2015-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cstring>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"
#include <taglib/fileref.h>
#include <taglib/tpropertymap.h>
#include <taglib/tag.h>
#pragma GCC diagnostic pop


#include "fundamentals.h"
#include "utility.h"
#include "logging.h"

#include "taglibreader.h"

namespace Media {
    /** Acquire a metadata field by name.
        @param file A taglib file handle.
        @param tagname The metadata field name.
        @return The data found, or an empty string. */
    static std::string getTagByName (const TagLib::FileRef &file,
                                const char *tagname) {
        // Get the iTunes "SoundCheck" property to equalize volumes.
        // Libao reports replay gain figures, which will override this
        // if encountered.
        const TagLib::PropertyMap &tags = file.file()->properties();
        auto tag = tags.find (tagname);
        // Make sure we found the tag, and it maps to 1 or more values
        // Note: TagLib::StringList doesn't implement empty()
        if (tag != tags.end() &&
            tag->second.begin() != tag->second.end()) {
            std::string value (tag->second [0].to8Bit());
            return trim (value);
        }
        return "";
    }

    /** @internal
        Extract replay gain.  libavformat does this sometime, but since there are
        so many standards to choose from (ID3, APE, iTunes, some others)
        it doesn't over them all.  Here we cover the iTunes case, which
        libavformat & gstreamer ignore. */
    static float getFileGain (const TagLib::FileRef &file) {
        return Metadata::gainFromiTunesNormalization (getTagByName (file, "COMMENT:ITUNNORM"));
    }


    float TaglibReader::getFileGain (const std::string &path) {
        TagLib::FileRef file (path.c_str(), true, TagLib::AudioProperties::Accurate);
        if (file.isNull()) {
            flog (LOG_WHERE (Log::METADATA), path, ": Not an audio file");
            return 0.0;
        }
        return Media::getFileGain (file);
    };



    TaglibReader::TaglibReader (const std::string &path) {
        TagLib::FileRef file (path.c_str(), true, TagLib::AudioProperties::Accurate);
        if (file.isNull()) {
            flog (LOG_WHERE (Log::METADATA), path, ": Not an audio file");
            throw MediaException ("Not an audio file");
        }
        TagLib::Tag *tag = file.tag();
        if (!tag || tag->isEmpty()) {
            flog (LOG_WHERE (Log::WARNING|Log::METADATA),
                  path, ": Missing or corrupt audio tag data");
            throw MediaException ("Missing or corrupt metadata");
        }

        // Taglib has an internal string type.
        // to8Bit converts to normal strings, (true) parameter asks for UTF8.
        artist = tag->artist().to8Bit (true);
        if (artist.empty())
            artist = getTagByName (file, "ALBUMARTIST");

        album = tag->album().to8Bit (true);
        if (album.empty())
            album = getTagByName (file, "ALBUM");

        title = tag->title().to8Bit (true);
        if (title.empty())
            title = getTagByName (file, "TITLE");

        if (title.empty()) {
            // Punt to the filename
            title = path.substr (path.rfind ('/') + 1);
            std::string::size_type extpos = title.rfind ('.');
            if (extpos > 5 && extpos > title.size() - 10) {
                title.erase (extpos);
            }
        }

        genre = tag->genre().to8Bit (true);
        cddb_id = getTagByName (file, "COMMENT:ITUNES_CDDB_IDS");

        track_number = tag->track();
        year = tag->year();
        gain = Media::getFileGain (file);

        std::string track = getTagByName (file, "TRACKNUMBER");
        if (!track.empty()) {
            int old_track = track_number;
            splitOf (track, &track_number, &track_count);
            if (old_track && old_track != track_number) {
                flog (LOG_WHERE (Log::WARNING|Log::METADATA), path, ": Disagreement on track number ",
                      track_number, " vs. ", old_track);
            }
        }

        std::string disc = getTagByName (file, "DISCNUMBER");
        if (!disc.empty()) {
            splitOf (disc, &disc_number, &disc_count);
        }

        try {
            TagLib::AudioProperties *properties = file.audioProperties();
            if (properties) {
                duration = properties->length();
            }
        } catch (const std::exception &e) {
            flog (LOG_WHERE (Log::WARNING|Log::METADATA), "Exception: ", e.what());
        }
    }
}
