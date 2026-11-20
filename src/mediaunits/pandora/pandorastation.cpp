///
/// Pandora Station implementation.
/// @file       pandorastation.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-03-17
/// @copyright  Copyright (c) 2015-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cassert>

#include <string>

#include "pandoratypes.h"
#include "pandorastation.h"
#include "pandoramessages.h"
#include "pandora.h"

namespace Pandora {
    const int StationListCacheTime = 1800;
    const int StationDetailCacheTime = 10000;

    /*
     *                  Skip Tracking
     */
     
    /** Construct a skip tracker.  The tracker is a simple linked list.

        When a skip occurs, the time of its expiration is added to the list.  When checks
        are done, the front time is checked; if that time is past, it is removed.  Skips
        are allowed when the size of the list is below the skip limit.

        @param limit The number of skips to allow.
        @param interv The duration over which the limit applies. */
    SkipTracker::SkipTracker (int limit, int interv) : skip_limit (limit), interval (interv) {
    }

    /** Check if (and when) a skip is available.
        @param when If non-null, is set on return to the time at which the next skip is allowed.
        @return True if a skip is allowed, false otherwise. */
    bool SkipTracker::canSkip (time_t *when) {
        purge();
        if (when) {
            if (size() >= skip_limit) {
                *when = front();
            } else {
                *when = time (nullptr);
            }
        }
        return (size() < skip_limit);
    }

    /** Check if a skip is available, and record one if it is.
        @param when If non-null, is set on return to the time at which the next skip is allowed.
        @return True if a skip happened, false if it was not allowed. */
    bool SkipTracker::skip (time_t *when) {
        purge();
        if (canSkip (when)) {
            push_back (time (nullptr) + interval);
            return true;
        }
        return false;
    }

    /*
     *              Pandora Stations (Playlists)
     */

    Station::Station (Source * const owner, const Parsnip::Data &message)
    : EncapsulatedPlaylist (owner),
      skips (owner->userFeatures().station_skip_limit, 3600) {
        playlistId (message ["stationId"].asString());
        playlistName (message ["stationName"].asString());
        station_token = message ["stationToken"].asString();
        is_shared = message ["isShared"].asBoolean();
        may_rename = message ["allowRename"].asBoolean();
        if (message.contains ("genre")) {
            std::string gen;
            for (const auto &g : message ["genre"]) {
                const std::string &gp = g.asString();
                // Assemble genres, but don't go over 30 characters.
                if (gen.empty()) {
                    gen = gp;
                } else if (gen.size() + gp.size() < 30) {
                    gen += ", " + gp;
                }
            }
            genre (gen);
        }
    };

    /** This is not a full-copy assignment, this is to update station
        details in the master when changes are received. */
    Station &Station::operator= (const Station &update) {
        assert (playlistId() == update.playlistId());
        playlistName (update.playlistName());
        is_shared = update.is_shared;
        may_rename = update.may_rename;
        genre (update.genre());
        return *this;
    }

    /** Convert a shared Pandora station into a private one.
        Shared stations cannot be renamed or have their seeds adjusted by the listener. */
    void Station::takePossession() {
        if (is_shared) {
            RequestTransformStation transform (this);
            Status status = pandora()->executeRequest (transform);
            if (status != Status::Ok) {
                throw CommandError (E_NAK, status_strerror (status));
            }
        }
    }

    void Station::rename (const std::string &new_name) {
        takePossession();
        if (!may_rename) {
            throw CommandError (E_UNSUPPORTED, "Rename of station disallowed");
        }
        RequestRenameStation rename (this, new_name);
        Status status = pandora()->executeRequest (rename);
        if (status != Status::Ok) {
            throw CommandError (E_NAK, status_strerror (status));
        }
        playlistName (new_name);
    }

    void Station::erase() {
        RequestRemoveStation remove (this);
        Status status = pandora()->executeRequest (remove);
        if (status != Status::Ok) {
            throw CommandError (E_NAK, status_strerror (status));
        }
        pandora()->removeStationByStationId (this->playlistId());
    }

    /*
     *              Mix support
     */
     
    bool Station::includedInMix (void) const {
        return in_quick_mix;
    }

    void Station::includedInMix (bool include) {
        in_quick_mix = include;
    }

    /** Update the quickmix/station shuffle by sending
        a list of desired stations to Pandora. */
    void Source::pushMixToServers(bool mix_all) {
        StationList mix_stations;
        for (auto &station : stations) {
            if (mix_all || station.second->includedInMix()) {
                mix_stations.push_back (station.second);
            }
        }

        SetQuickMixStations request (std::move (mix_stations));
        Status status = comm.execute (request);
        if (status != Status::Ok) {
            flog (LOG_WHERE (Log::ERROR), "Could not push updated mix stations to server.");
        }
    }

    /// Retrieve the mix playlist.
    PianodPlaylist *Source::getMixPlaylist (void) {
        return mix_playlist.get();
    };

    /// Retrieve the everything playlist.
    PianodPlaylist *Source::getEverythingPlaylist (void) {
        return everything_playlist.get();
    };

}  // namespace Pandora
