///
/// Media source for the tone generator.
/// @file       tonegensource.cpp - pianod
/// @author     Perette Barella
/// @date       2014-12-04
/// @copyright  Copyright 2014-2021 Devious Fish.  All rights reserved.
///

#include <config.h>

#include "fundamentals.h"
#include "sources.h"
#include "tonegen.h"
#include "encapmusic.h"
#include "retainedlist.h"
#include "musiclibrary.h"
#include "musiclibraryhash.h"
#include "mediaunit.h"
#include "mediaparameters.h"

namespace ToneGenerator {
    /*
     *                  Identity
     */
    using namespace Media;

    Source::Source (const Parameters &params)
    : Media::Source (new Parameters (params)), library (this, params.persistence != PersistenceMode::Temporary) {
        MusicAutoReleasePool pool;
        // Create a reusable mix playlist
        mix_playlist = new (std::nothrow) MetaPlaylist (this,
                                                  PianodPlaylist::PlaylistType::MIX);
        if (mix_playlist)
            mix_playlist->retain();

        // Create a reusable everything playlist
        everything_playlist = new (std::nothrow) MetaPlaylist (this,
                                                         PianodPlaylist::PlaylistType::EVERYTHING);
        if (everything_playlist)
            everything_playlist->retain();
        
        state = State::READY;
    }

    Source::~Source () {
        if (mix_playlist) mix_playlist->release();
        if (everything_playlist) everything_playlist->release();
    }

    const char *Source::kind (void) const {
        return SourceName::ToneGenerator;
    };

    
    /*
     *                      Capabilities
     */

    bool Source::canExpandToAllSongs (void) const {
        return true;
    };

    bool Source::flush (void) {
        return (parameters->persistence == PersistenceMode::Temporary ? true :
                library.flush());
    };

    float Source::periodic (void) {
        return library.periodic();
    }

    /*
     *                      Playlist Methods
     */

    PlaylistList Source::getPlaylists (const Filter &filter) {
        PlaylistList list;
        for (auto item : library.playlists) {
            if (filter.matches (item.second)) {
                list.push_back (item.second);
            }
        }
        return list;
    };

    MusicThingie *Source::getAnythingById (const Media::SplitId &id) {
        MusicThingie *thing = library.getById (id.type, id.innerId);
        if (!thing && id.type == MusicThingie::Type::Song) {
            try {
                const SplitToneId tone_id (id.innerId);
                auto song = static_cast <const Song *> (library.getById (id.type, tone_id.media_id));
                if (song) {
                    thing = library.duplicate (song, tone_id.channel);
                }
            } catch (const std::invalid_argument &e) {
                // Ignore it.
            }
        }
        return thing;
    };

    PianodPlaylist *Source::getMixPlaylist (void) {
        return mix_playlist;
    }

    PianodPlaylist *Source::getEverythingPlaylist (void) {
        return everything_playlist;
    }

    PianodPlaylist *Source::createPlaylist (const char *name,
                                          MusicThingie::Type type,
                                          MusicThingie *from) {
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
    Media::Player *Source::getPlayer (const AudioSettings &audio, PianodSong *song) {
        // Handle missing song (possible by tinkering with persistence file).
        if (!song) return nullptr;
        
        Media::Player *p = new Player (audio, song);
        if (p)
            library.markDirty(); // last played time will have changed.
        return p;
    };

    SongList Source::getRandomSongs (PianodPlaylist *playlist,
                                     const UserList &users,
                                     Media::SelectionMethod selectionMethod) {
        auto *params = static_cast<const MusicLibrary::LibraryParameters *> (parameters);
        return library.getRandomSongs (playlist, users, selectionMethod, *params);
    }

    ThingieList Source::getSuggestions (const Filter &filter, SearchRange where) {
        return ThingieList (library.getSuggestions (filter, where));
    }

    // Typecast thing to an equivalent Tonegenerator suggestion
    MusicThingie *Source::getSuggestion (MusicThingie *thing,
                                         MusicThingie::Type type,
                                         SearchRange where) {
        assert (thing->source() != this);
        return Media::Source::getSuggestion (thing, type, where, true);
    }


} // </namespace>
