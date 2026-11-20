///
/// Media source for filesystem/local audio files.
/// @file       filesystemsource.cpp - pianod
/// @author     Perette Barella
/// @date       2015-02-25
/// @copyright  Copyright 2015-2023 Devious Fish.  All rights reserved.
///

#include <config.h>

#include "fundamentals.h"
#include "sources.h"
#include "filesystem.h"
#include "encapmusic.h"
#include "retainedlist.h"
#include "musiclibrary.h"
#include "musiclibraryparameters.h"
#include "mediaunit.h"
#include "mediaparameters.h"

namespace Filesystem {
    /*
     *                  Identity
     */
    using namespace Media;

    Source::Source (const Parameters &params)
    : Media::Source (new Parameters (params)),
      library (this, params.persistence != PersistenceMode::Temporary, params.path, params.scan_frequency) {
        MusicAutoReleasePool pool;

        // Create a reusable mix playlist
        mix_playlist = new (std::nothrow) MetaPlaylist (this, PianodPlaylist::PlaylistType::MIX);

        // Create a reusable everything playlist
        everything_playlist = new (std::nothrow) MetaPlaylist (this, PianodPlaylist::PlaylistType::EVERYTHING);

        if ((params.scan_frequency == MusicLibrary::ScanFrequency::NEXTSTARTUP
             || params.scan_frequency == MusicLibrary::ScanFrequency::EVERYSTARTUP)
            && library.searches.empty()) {
            library.startScan();
        }
    }

    const char *Source::kind (void) const {
        return SourceName::FileSystem;
    };

    /*
     *                      Capabilities
     */

    bool Source::canExpandToAllSongs (void) const {
        return true;
    };

    bool Source::flush (void) {
        return (parameters->persistence == PersistenceMode::Temporary ? true : library.flush());
    };

    float Source::periodic (void) {
        float next_request = library.periodic();
        if (library.isReady()) {
            if (library.valid && state < State::READY)
                state = State::READY;
            else if (!library.valid && state == State::READY)
                state = State::VALID;
            auto params = static_cast<const Parameters *> (parameters);
            if (params->scan_frequency == MusicLibrary::ScanFrequency::DAILY
                && library.last_scan + 86400 /* One day */ < time (nullptr)) {
                try {
                    library.startScan();
                } catch (...) {
                    flog (LOG_WHERE (Log::FILESYSTEM|Log::WARNING), params->path, "Cannot start periodic filesystem scan");
                }
            }
        }
        return next_request;
    }

    /*
     *                      Playlist Methods
     */

    PlaylistList Source::getPlaylists (const Filter &filter) {
        PlaylistList list;
        for (auto &item : library.playlists) {
            if (filter.matches (item.second)) {
                list.push_back (item.second);
            }
        }
        return list;
    };

    MusicThingie *Source::getAnythingById (const Media::SplitId &id) {
        return library.getById (id.type, id.innerId);
    };

    PianodPlaylist *Source::getMixPlaylist (void) {
        return mix_playlist.get();
    }

    PianodPlaylist *Source::getEverythingPlaylist (void) {
        return everything_playlist.get();;
    }

    PianodPlaylist *Source::createPlaylist (const char *name, MusicThingie::Type type, MusicThingie *from) {
        assert (name);
        return library.createPlaylist (name, type, from);
    };

    PianodPlaylist *Source::createPlaylist (const char *name, const Filter &filter) {
        assert (name);
        return library.createPlaylist (name, filter);
    }

    PianodPlaylist *Source::getTransientPlaylist (const Filter &criteria) {
        return library.formTransientPlaylist (criteria);
    }

    // Miscellaneous

    SongList Source::getRandomSongs (PianodPlaylist *playlist, const UserList &users, SelectionMethod selectionMethod) {
        auto params = static_cast<const Parameters *> (parameters);
        return library.getRandomSongs (playlist, users, selectionMethod, *params);
    }

    ThingieList Source::getSuggestions (const Filter &filter, SearchRange where) {
        return ThingieList (library.getSuggestions (filter, where));
    }

    // Typecast thing to an equivalent Filesystem suggestion
    MusicThingie *Source::getSuggestion (MusicThingie *thing, MusicThingie::Type type, SearchRange where) {
        assert (thing->source() != this);
        return Media::Source::getSuggestion (thing, type, where, true);
    }

    void Source::playbackProblem (void) {
        library.checkValidity();
    };

    void Source::rescan (bool reset) {
        if (reset)
            library.clear();
        library.startScan();
    }

}  // namespace Filesystem
