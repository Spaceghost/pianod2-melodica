///
///
/// Pandora media source for pianod.
/// @file       mediaunits/pandora/pandorasource.cpp - pianod project
/// @author     Perette Barella
/// @date       2014-10-23
/// @copyright  Copyright 2012-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <vector>
#include <algorithm>

#include <cassert>

#include "fundamentals.h"
#include "fileio.h"
#include "sources.h"
#include "mediaunit.h"
#include "filter.h"
#include "querylist.h"

#include "pandoracomm.h"
#include "pandora.h"


namespace Pandora {
    namespace Key {
        const char *CacheData = "cache";
        const char *CommunicationParameters = "communication";
        const char *GenreStations = "genreStations";
        const char *GenreStationChecksum = "genreStationChecksum";
    }  // namespace Key

    Source::Source (const ConnectionParameters &params)
    : Media::Source (new ConnectionParameters (params)),
      comm (params.username,
            params.password,
            params.control_proxy.empty() ? params.proxy : params.control_proxy,
            params.protocol),
      library (this, ThingiePoolParameters{}) {
        ThingiePoolParameters pool;
        pool.minimum_retained_items = params.cache_minimum;
        pool.maximum_retained_items = params.cache_maximum;
        library.setParameters (pool);

        try {
            recovery.reset (new Parsnip::Data{ retrieveJsonFile (filename(), 371) });
            comm.restore ((*recovery) [Key::CommunicationParameters]);
        } catch (std::exception &ex) {
            flog (LOG_WHERE (Log::ERROR), ex.what());
        }
    }

    Source::~Source() {
    }

    /*
     *                       Internal support
     */

    /** @internal Retrieve station from station list by station ID.
       @param station_id The station ID of the station.
       @return The station. */
    Station *Source::getStationByStationId (const std::string &station_id) {
        auto it = stations.find (station_id);
        return (it == stations.end() ? nullptr : it->second.get());
    }

    /** @internal Delete station from station list by station ID.
        @param station_id The station ID of the station. */
    void Source::removeStationByStationId (const std::string &station_id) {
        auto it = stations.find (station_id);
        assert (it != stations.end());
        stations.erase (it);
    }

    /*
     *                      Identity
     */

    const char *Source::kind (void) const {
        return SourceName::Pandora;
    }

    /*
     *                      Capabilities
     */

    bool Source::requireNameForCreatePlaylist (void) const {
        return false;
    }

    /*
     *                  Playlist methods
     */
    PlaylistList Source::getPlaylists (const Filter &filter) {
        PlaylistList list;
        for (const auto &station : stations) {
            if (filter.matches (station.second.get())) {
                list.push_back (station.second);
            }
        }
        return list;
    };

    MusicThingie *Source::getAnythingById (const Media::SplitId &id) {
        MusicThingie *thing = library.get (id.wholeId);
        if (!thing && id.type == MusicThingie::Type::Playlist) {
            // If we didn't find it, and it's a playlist, see if it's one of ours.
            auto it = stations.find (id.innerId);
            if (it != stations.end()) {
                thing = it->second.get();
            }
        }
        return thing;
    }

    /*
     *              Miscellaneous API methods
     */

    ThingieList Source::getSuggestions (const Filter &filter, SearchRange where) {
        if (where == SearchRange::KNOWN) {
            return library.get (filter);
        } else if (where == SearchRange::REQUESTS || where == SearchRange::REQUESTABLE) {
            throw CommandError (E_UNSUPPORTED, "Pandora source does not support requests");
        }

        // Get a querylist representing the filter.
        Query::Constraints constraints;
        constraints.canSubstringMatch [Filter::Field::Search] = true;

        constraints.participatesInFuzzy [Filter::Field::Search] = true;
        constraints.participatesInFuzzy [Filter::Field::Artist] = true;
        constraints.participatesInFuzzy [Filter::Field::Title] = true;
        constraints.participatesInFuzzy [Filter::Field::Genre] = true;

        constraints.fieldInGeneralSearch [Filter::Field::Artist] = true;
        constraints.fieldInGeneralSearch [Filter::Field::Title] = true;
        constraints.fieldInGeneralSearch [Filter::Field::Name] = true;
        constraints.fieldInGeneralSearch [Filter::Field::Genre] = true;

        constraints.andCapable = false;

        Query::List queries (filter, constraints);
        // In Shallow/suggestion mode, if it's not a simple query, don't refilter.
        // Otherwise, request the data and then refine to that allowed by the filter.
        const Filter &filt = (where == SearchRange::SHALLOW && queries.size() <= 1 ? Filter::All : filter);
        ThingieList results;
        for (auto const &query : queries) {
            assert (query.size() == 1);
            assert (query [0].searchMethod == Query::SubstringMatch || query [0].searchMethod == Query::Fuzzy);
            std::string results_name = query [0].value;
            const ThingieList *suggestions = nullptr;
            auto it = prior_searches.find (results_name);
            if (it != prior_searches.end()) {
                suggestions = &it->second;
            } else {
                SearchRequest search (this, query [0].value, true);
                Status status = comm.execute (search);
                if (status == Status::Ok) {
                    library.add (search.getResponse());
                    prior_searches [results_name] = search.getResponse();
                    suggestions = &prior_searches [results_name];
                }
            }
            if (suggestions) {
                for (auto suggestion : *suggestions) {
                    if (filt.matches (suggestion)) {
                        results.push_back (suggestion);
                    }
                }
            } else {
                flog (LOG_WHERE (Log::ERROR), "Query failed: ", query [0].value);
            }
        }
        // Append any matching genre stations
        updateGenreStations();
        for (auto suggestion : genre_stations) {
            if (filter.matches (suggestion)) {
                results.push_back (suggestion);
            }
        }
        return results;
    }

    // Typecast thing to an equivalent Pandora suggestion
    MusicThingie *Source::getSuggestion (MusicThingie *thing, MusicThingie::Type type, SearchRange where) {
        if (type == MusicThingie::Type::Album)
            throw CommandError (E_UNSUPPORTED);
        if (where != SearchRange::KNOWN) {
            try {
                return Media::Source::getSuggestion (thing, type, SearchRange::KNOWN, false);
            } catch (const CommandError &) {
                // fall through and try again
            }
        }
        return Media::Source::getSuggestion (thing, type, where, false);
    }

    PianodPlaylist *Source::createPlaylist (const char *name, MusicThingie::Type seed_type, MusicThingie *music) {
        assert (MusicThingie::isPrimary (seed_type));
        assert (music);
        if (seed_type != MusicThingie::Type::Artist && seed_type != MusicThingie::Type::Song
            && seed_type != MusicThingie::Type::Playlist)
            throw CommandError (E_MEDIA_ACTION);

        if (seed_type == MusicThingie::Type::Playlist && !dynamic_cast<GenreSuggestion *> (music)) {
            throw CommandError (E_WRONGTYPE, "Only genre stations can be used as station seeds");
        }
        if (dynamic_cast<Advert *> (music)) {
            throw CommandError (E_WRONGTYPE, "Cannot construct station from advert");
        }
        RequestCreateStation create (this, seed_type, music->internalId (music->type()));
        Status status = comm.execute (create);
        if (status == Status::Ok) {
            Station *new_station = create.getStation();
            stations [new_station->playlistId()] = new_station;
            if (name) {
                RequestRenameStation rename_station (new_station, name);
                status = comm.execute (rename_station);
                // If renaming didn't succeed... well, just roll with it.
                (void) status;
            }
            return new_station;
        }
        throw CommandError (E_NAK);
    }

    SongList Source::getRandomSongs (PianodPlaylist *playlist, const UserList &, Media::SelectionMethod) {
        updateStationList (playlist->playlistType());

        switch (playlist->playlistType()) {
            case PianodPlaylist::MIX: {
                // Return empty if no stations are selected.
                bool have_stations = false;
                for (const auto &station : stations) {
                    have_stations = have_stations || station.second->includedInMix();
                }
                if (!have_stations) {
                    return SongList{};
                }
                this->pushMixToServers();
                break;
            }
            case PianodPlaylist::EVERYTHING:
                this->setMixAllOnServers();
                break;
            case PianodPlaylist::SINGLE:
                break;
            case PianodPlaylist::TRANSIENT:
                assert (!"Pandora has no transient playlist.");
                return SongList{};
        }

        reportStatus ("Retrieving new playlist");
        RequestQueueTracks request (static_cast<Station *> (playlist));
        Status status = comm.execute (request);
        if (status != Status::Ok) {
            alert (F_PANDORA, "Unable to get playlist", status_strerror (status).c_str());
            return SongList{};
        }
        SongList songs = request.getResponse();
        for (const auto song : songs) {
            if (!dynamic_cast<Advert *> (song)) {
                library.update (song);
            }
        }
        return songs;
    }

    /** Get initial or refresh station list.
        @param mixSetting The playing mix setting (single playlist, mix, or everything).
        @return true on success, false on failure. */
    bool Source::updateStationList (PianodPlaylist::PlaylistType mixSetting) {
        assert (state == State::VALID || state == State::READY);
        if (state != State::VALID && state != State::READY) {
            return false;
        }

        // Assume station lists are good for at least a few minutes.
        time_t now = time (nullptr);
        if (now < station_list_expiration) {
            return true;
        }

        if (comm.retryTime() > now) {
            station_list_expiration = comm.retryTime();
            return false;
        }

        // If updating, get the checksum and skip full update if matching.
        if (!station_list_checksum.empty()) {
            RequestStationListChecksum checksum_request;
            Status status = comm.execute (checksum_request);
            if (status != Status::Ok) {
                station_list_expiration = comm.retryTime();
                return false;
            }
            if (checksum_request.getChecksum() == station_list_checksum) {
                station_list_expiration = now + StationListCacheTime;
                return true;
            }
        }

        reportStatus ("Retrieving/updating station list");
        RequestStationList station_list_request (this);
        Status status = comm.execute (station_list_request);
        if (status != Status::Ok) {
            station_list_expiration = comm.retryTime();
            return false;
        }

        bool stations_added = false;
        std::unordered_map<std::string, bool> still_exists;
        for (const auto &station : stations) {
            still_exists [station.second->playlistId()] = false;
        }

        // Add new stations to the station list
        for (auto station : station_list_request.getStations()) {
            Retainer<Station *> &existing_station = stations [station->playlistId()];
            if (!existing_station) {
                stations_added = true;
                existing_station = station;
            } else {
                *existing_station = *station;
            }
            still_exists [station->playlistId()] = true;
        }

        // Remove deleted stations from the station list
        bool stations_removed = false;
        for (const auto &check : still_exists) {
            if (!check.second) {
                auto it = stations.find (check.first);
                assert (it != stations.end());
                stations.erase (it);
            }
        }

        // If we have a QuickMix, mark mixing stations as in the mix.
        if (!mix_playlist) {
            auto mixers = station_list_request.getMixStationList();
            for (const auto &item : mixers) {
                // Pandora sometimes gives mix lists with stations that don't exist,
                // or at least it doesn't tell us about.
                auto station = stations.find (item);
                if (station != stations.end()) {
                    station->second->includedInMix (true);
                } else {
                    flog (LOG_WHERE (Log::WARNING), "Pandora QuickMix member station ",
                          item, " does not exist");
                }
            }
            mix_playlist = station_list_request.getQuickMix();
            everything_playlist = station_list_request.getEverythingStation();
        }

        // Announce any changes
        if (stations_added || stations_removed) {
            alert (V_PLAYLISTS_CHANGED);
        }

        station_list_expiration = now + StationListCacheTime;
        station_list_checksum = station_list_request.getChecksum();
        return true;
    }

    /** Get initial or refresh genre station list. */
    void Source::updateGenreStations() {
        assert (state == State::VALID || state == State::READY);
        if (state != State::VALID && state != State::READY) {
            return;
        }

        // Assume station lists are good for at least a few minutes.
        time_t now = time (nullptr);
        if (now < genre_stations_expiration) {
            return;
        }

        // Get the current checksum
        RequestStationListChecksum checksum_request (RequestStationListChecksum::GenreStations);
        Status stat = comm.execute (checksum_request);
        if (stat != Status::Ok) {
            station_list_expiration = comm.retryTime();
            return;
        }

        // If the checksum matches what we already have, skip updating.
        if (checksum_request.getChecksum() != genre_stations_checksum) {
            reportStatus ("Retrieving/updating genre stations");
            RequestGenreStationList genre_list_request (this);
            stat = comm.execute (genre_list_request);
            if (stat != Status::Ok) {
                genre_stations_expiration = comm.retryTime();
                return;
            }

            genre_stations = genre_list_request.getStations();
            ThingieList response;
            std::copy (genre_stations.begin(), genre_stations.end(), std::back_inserter (response));
            library.update (response);

            genre_stations_checksum = checksum_request.getChecksum();
            persist();
        }
        genre_stations_expiration = now + StationListCacheTime;
        return;
    }

    float Source::periodic (void) {
        time_t offline = comm.offlineUntil();
        time_t now = time (nullptr);
        switch (state) {
            case State::INITIALIZING: {
                Status status = comm.partnerAuthenticate();
                if (status != Status::Ok)
                    break;
                skips.setLimit (comm.getFeatures().daily_skip_limit);
                state = State::VALID;
                /* FALLTHRU */
            }
            case State::VALID:
                if (!offline && updateStationList()) {
                    state = State::READY;
                    if (recovery) {
                        restore();
                    }
                } else {
                    library.periodic();
                }
                break;
            default: {
                library.periodic();
                if (library.writeIsDue()) {
                    flush();
                }
            }
        }
        if (comm.isFailed()) {
            if (state != State::DEAD) {
                alert (F_AUTHENTICATION, "Invalid Pandora credentials");
            }
            state = State::DEAD;
        }
        if (offline) {
            time_t next = now - offline;
            return (next < 300 ? next : 300);
        }
        return 5;
    }  // namespace Pandora

    bool Source::restore() {
        assert (genre_stations.empty());
        assert (genre_stations_checksum.empty());
        bool status = false;
        try {
            library.restore (recovery->at (Key::CacheData));
            bool missed = false;
            for (const Parsnip::Data &data : recovery->at (Key::GenreStations)) {
                std::string id = std::to_string (serialNumber()) + char (MusicThingie::Type::PlaylistSuggestion)
                                 + data.asString();
                GenreSuggestion *suggestion = dynamic_cast<GenreSuggestion *> (library.get (id));
                if (!suggestion) {
                    missed = true;
                } else {
                    genre_stations.push_back (suggestion);
                }
            }
            if (!missed) {
                genre_stations_checksum = recovery->at (Key::GenreStationChecksum).asString();
                status = true;
            }
        } catch (std::exception &ex) {
            flog (LOG_WHERE (Log::ERROR), ex.what());
        }
        recovery.reset();
        return status;
    }

    bool Source::persist() const {
        bool status = true;
        if (parameters->persistence != Media::PersistenceMode::Temporary) {
            Parsnip::Data genre_station_ids{ Parsnip::Data::List };
            for (const auto station : genre_stations) {
                genre_station_ids.push_back (station->playlistId());
            }

            // clang-format off
            Parsnip::Data document{ Parsnip::Data::Dictionary,
                Key::CacheData, library.persist(),
                Key::CommunicationParameters, comm.persist(),
                Key::GenreStationChecksum, genre_stations_checksum,
                Key::GenreStations, std::move (genre_station_ids)
            };
            // clang-format on
            status = carefullyWriteFile (filename(), document);
        }
        if (status)
            library.clearDirty();
        return status;
    }

    bool Source::flush() {
        if (!library.isDirty())
            return true;
        return persist();
    }

}  // namespace Pandora
