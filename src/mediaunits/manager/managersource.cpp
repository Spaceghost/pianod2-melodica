///
/// Media manager methods for behaving as a source.
/// The media manager maintains a list of sources, but can also
/// act as a source, in which case it passes requests through to the
/// respective sources and amalgamates responses.
/// @file       managersource.cpp - pianod
/// @author     Perette Barella
/// @date       2014-12-13
/// @copyright  Copyright 2014-2023 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <string>
#include <stdexcept>
#include <algorithm>

#include "sources.h"
#include "mediaunit.h"
#include "encapmusic.h"
#include "mediamanager.h"
#include "mediaparameters.h"
#include "retainedlist.h"

namespace Media {

    const char *Manager::kind (void) const {
        return SourceName::Manager;
    };

    /*
 *      Capability routines: Generally, the manager can if any source can.
 */

    bool Manager::canExpandToAllSongs (void) const {
        for (auto const &src : *this) {
            if (src.second != this && src.second->canExpandToAllSongs())
                return true;
        }
        return false;
    };

    PlaylistList Manager::getPlaylists (const Filter &filter) {
        PlaylistList completeList;
        for (auto src : getRealSources (State::READY)) {
            completeList.join (src->getPlaylists (filter));
        }
        return completeList;
    };

    PianodPlaylist *Manager::getMixPlaylist (void) {
        return mix_playlist.get();
    }

    PianodPlaylist *Manager::getEverythingPlaylist (void) {
        return everything_playlist.get();
    }

    /** Play a song, gathering statistics and substituting proxies if enabled. */
    Player *Manager::getPlayer (const AudioSettings &settings, PianodSong *song) {
        auto request_source = song->source();

        // If source is a proxy recipient, try to find a donor.
        auto player_source = request_source;
        Media::Player *player = nullptr;
        if (request_source->parameters->song_proxy == ProxyMode::Recipient) {
            for (auto source : getRealSources (State::READY)) {
                if (source->canExpandToAllSongs() && source->parameters->song_proxy == ProxyMode::Donor) {
                    try {
                        PianodSong *proxy = source->getSuggestion (song, SearchRange::KNOWN)->asSong();
                        assert (proxy);
                        if (proxy) {
                            player = source->getPlayer (settings, proxy);
                            player_source = proxy->source();
                            flog (LOG_WHERE (Log::GENERAL),
                                  (*song)(),
                                  ": ",
                                  player_source->identify(),
                                  " proxying for ",
                                  request_source->identify());
                            break;
                        }
                    } catch (const CommandError &) {
                        // Ignore it.
                    }
                }
            }
        }

        // Get a player from the original source if proxy player wasn't found.
        if (!player) {
            player_source = request_source;
            player = player_source->getPlayer (settings, song);
        }

        // Update statistics
        request_source->statistics.songs_attempted++;
        media_manager->statistics.songs_attempted++;
        if (player) {
            if (request_source != player_source) {
                request_source->statistics.songs_replaced++;
                player_source->statistics.songs_donated++;
                media_manager->statistics.songs_replaced++;
                media_manager->statistics.songs_donated++;
            }
        } else {
            player_source->statistics.playback_failures++;
            player_source->statistics.sequential_failures++;
            media_manager->statistics.playback_failures++;
            media_manager->statistics.sequential_failures++;
        }
        return player;
    }

    ThingieList Manager::getSuggestions (const Filter &filter, SearchRange what) {
        ThingieList results;
        bool for_request = forRequest (what);
        bool requests_supported = false;
        for (auto src : getRealSources (State::READY)) {
            try {
                ThingieList list = src->getSuggestions (filter, what);
                requests_supported = true;
                for (auto item : list) {
                    if (!for_request || item->canQueue()) {
                        results.push_back (item);
                    }
                }
            } catch (const CommandError &ex) {
                if (ex.reason() != E_UNSUPPORTED) {
                    throw;
                }
            }
        }
        if (!requests_supported) {
            throw CommandError (E_UNSUPPORTED, "Active sources do not support requests");
        }
        return results;
    }

    MusicThingie *Manager::getSuggestion (MusicThingie *, MusicThingie::Type, SearchRange) {
        throw CommandError (E_UNSUPPORTED, "Cannot add seeds to media manager");
    };

    PianodPlaylist *Manager::createPlaylist (const char *, MusicThingie::Type, MusicThingie *) {
        throw CommandError (E_MEDIA_MANAGER);
    }

    PianodPlaylist *Manager::createPlaylist (const char *, const Filter &) {
        throw CommandError (E_MEDIA_MANAGER);
    };

    /** If *any* source has the necessary capabilities, and some items match
        the criteria, then stash the criteria (needed when random songs are
        later requested) and return the transient playlist. */
    PianodPlaylist *Manager::getTransientPlaylist (const Filter &criteria) {
        for (auto src : getRealSources (State::READY)) {
            if (src->isReady() && src->canExpandToAllSongs()) {
                PianodPlaylist *list = src->getTransientPlaylist (criteria);
                if (list && list->songs().size() > 0) {
                    transient_criteria = criteria;
                    return transient_playlist.get();
                }
            }
        }
        return nullptr;
    }

    SongList Manager::getRandomSongs (PianodPlaylist *playlist,
                                      const UserList &users,
                                      Media::SelectionMethod selectionMethod) {
        assert (playlist->playlistType() != PianodPlaylist::SINGLE);
        assert (selectionMethod == SelectionMethod::Song || selectionMethod == SelectionMethod::Artist
                || selectionMethod == SelectionMethod::Album);

        bool mix = playlist->playlistType() == PianodPlaylist::MIX;
        bool transient = playlist->playlistType() == PianodPlaylist::TRANSIENT;
        const char *mix_kind = (mix ? "mix" : transient ? "transient" : "everything");

        SongList results;
        // We normally get 4 songs from each mediasource, but provide some headroom
        results.reserve (size() * 5);

        SourceList sources{ getRealSources (State::READY) };
        for (auto src : sources) {
            if (src->isReady() && !src->isLockedOut() && (!transient || src->canExpandToAllSongs())) {
                PianodPlaylist *subPlaylist = (mix         ? src->getMixPlaylist()
                                               : transient ? src->getTransientPlaylist (transient_criteria)
                                                           : src->getEverythingPlaylist());
                if (subPlaylist) {
                    SongList adds = subPlaylist->getRandomSongs (users, selectionMethod);
                    results.mixedMerge (adds);
                } else {
                    flog (LOG_WHERE (Log::WARNING),
                          "Source ",
                          src->kind(),
                          " ",
                          src->name(),
                          " did not provide ",
                          mix_kind,
                          " playlist");
                }
                // For Artist or Album mode, just return the first set so that
                // differing items don't get co-mingled.
                if (!results.empty() && selectionMethod != SelectionMethod::Song) {
                    break;
                }
            }
        }
        return results;
    };
}  // namespace Media
