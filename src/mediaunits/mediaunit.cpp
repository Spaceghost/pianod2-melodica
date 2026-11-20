///
/// Media unit type implementations.
/// @file       mediaunit.cpp - pianod
/// @author     Perette Barella
/// @date       2015-01-09
/// @copyright  Copyright 2015-2020 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <assert.h>

#include <string>
#include <stdexcept>

#include "mediamanager.h"
#include "ownership.h"
#include "interpreter.h"

namespace Media {
    const char *Key::Source = KEY_SOURCE;
    const char *Key::SourceId = "id";
    const char *Key::SourceKind = "type";
    const char *Key::SourceName = "name";

    /** Update playback statistics.
        @warning If we cared that much, this would have a mutex.  We don't. */
    void Source::playbackComplete (bool played, bool successfully) {
        // Also record this in the media manager.
        if (this != media_manager)
            media_manager->playbackComplete (played, successfully);
        if (played)
            statistics.tracks_played++;
        if (successfully) {
            statistics.sequential_failures = 0;
            lockout_until = 0;
        } else {
            statistics.sequential_failures++;
            if (statistics.sequential_failures > 2) {
                lockout_until = time(nullptr) + (1 << std::min (statistics.sequential_failures, 10));
            }
            statistics.playback_failures++;
        }
    }

    /** Split an ID to its component pieces (source, type, id).
        @param id The ID to split up.
        @throw CommandError if the ID is invalid. */
    SplitId::SplitId (const std::string &id) : wholeId (id) {
        const char *start = id.c_str();
        char *end;
        serialNumber = strtol(start, &end, 10);
        if (start != end && *end) {
            type = (MusicThingie::Type) *(end++);
            if (MusicThingie::isValidType(type) && *end) {
                innerId = end;
                source = media_manager->get (serialNumber);
                if (source)
                    return;
                throw CommandError (E_NOTFOUND, id);
            }
        }
        throw CommandError (E_INVALID, id);
    }
}
