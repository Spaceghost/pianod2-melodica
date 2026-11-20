///
/// Rating and seed storage.
/// Stores and retrieves station data (seeds & feedback) for stations.
/// @file       mediaunits/pandora/pandoraseeds.cpp - pianod
/// @author     Perette Barella
/// @date       2020-04-02
/// @copyright  Copyright 2014-2022 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <cassert>

#include <string>
#include <algorithm>

#include "filter.h"

#include "pandoratypes.h"
#include "pandorastation.h"
#include "pandoramessages.h"
#include "pandora.h"

namespace Pandora {

    /*
     *                  Seed & Rating Helper Class
     */

    /** Search the authoritative list for a matching item.
        @param search_item The item to search for.
        @return The detail record found, or a nullptr. */
    template <typename InfoType>
    InfoType *StationDefinitionDetails<InfoType>::find (const MusicThingie *search_item) const {
        Filter::DuplicationFlags fields (false);
        fields [Filter::Field::Artist] = true;
        if (type == MusicThingie::Type::Song) {
            assert (search_item->type() == MusicThingie::Type::Song);
        }
        try {
            Filter filter (search_item, type, fields);
            for (const auto item : info_list) {
                if (item->matches (filter)) {
                    assert (dynamic_cast<InfoType *> (item));
                    return static_cast<InfoType *> (item);
                }
            }
        } catch (std::invalid_argument &e) {
            // Filter construction failed due to empty fields (typically on adverts).
        }
        return nullptr;
    }

    /** See if details originated from this list.  This compares pointers only.
        @param detail The detail to search for.
        @return True if the detail is from this list, false otherwise. */
    template <typename InfoType>
    bool StationDefinitionDetails<InfoType>::isSeedPresent (const MusicThingie *detail) const {
        assert (dynamic_cast<const InfoType *> (detail));
        return (std::find (info_list.begin(), info_list.end(), static_cast<const InfoType *> (detail))
                != info_list.end());
    }

    /** Replace the details stored and clear cache.
        @param list A new set of details. */
    template <typename InfoType>
    void StationDefinitionDetails<InfoType>::operator= (ListType &&list) {
        info_list = std::move (list);
        cache.clear();
    }

    /** Add a single new detail item.
        @param primary An item for which details are added, for priming the cache.
        @param detail A detail record. */
    template <typename InfoType>
    void StationDefinitionDetails<InfoType>::storeDetail (const MusicThingie *primary, InfoType *detail) {
        info_list.push_back (detail);
        assert (primary->type() == MusicThingie::Type::Artist || primary->type() == MusicThingie::Type::Song);
        assert (type == MusicThingie::Type::Artist || primary->type() == MusicThingie::Type::Song);
        cache [primary->id()] = detail;
        erase (nullptr);  // Prior cache misses must be retried.
    }

    /** Retrieve details for an item.  If a cache entry is found, uses that; if not, searches and updates cache.
        @param primary The item for which details are being requested.
        @return The detail record, or nullptr if no applicable details are found. */
    template <typename InfoType>
    InfoType *StationDefinitionDetails<InfoType>::getDetail (const MusicThingie *primary) const {
        assert (primary->type() == MusicThingie::Type::Artist || primary->type() == MusicThingie::Type::Song);
        assert (type == MusicThingie::Type::Artist || primary->type() == MusicThingie::Type::Song);
        auto it = cache.find (primary->id());
        if (it != cache.end()) {
            return it->second.get();
        }
        auto &slot = cache [primary->id()];
        slot = find (primary);
        return slot.get();
    }

    /** Remove a detail record from the list and cache.
        @param item The detail record to remove. */
    template <typename InfoType>
    void StationDefinitionDetails<InfoType>::erase (InfoType *item) {
        for (auto it = info_list.begin(); it != info_list.end();) {
            if (*it == item) {
                it = info_list.erase (it);
            } else {
                it++;
            }
        }
        for (auto it = cache.begin(); it != cache.end();) {
            if (it->second.get() == item) {
                it = cache.erase (it);
            } else {
                it++;
            }
        }
    }

    // Explicit instantiation
    template class StationDefinitionDetails<SongRating>;
    template class StationDefinitionDetails<SongSeed>;
    template class StationDefinitionDetails<ArtistSeed>;
    template class StationDefinitionDetails<GenreSeed>;

    /*
     *                  Station Seed Support
     */

    bool Station::canSeed (MusicThingie::Type seed_type) const {
        return (seed_type == MusicThingie::Type::Artist || seed_type == MusicThingie::Type::Song
                || seed_type == MusicThingie::Type::Playlist);
    }

    /** Check if a seed record is applicable to the current station.
        @param seed The seed.
        @return True if the seed is from this station, otherwise false. */
    bool Station::seedAppliesToStation (const MusicThingie *seed) const {
        switch (seed->type()) {
            case MusicThingie::Type::ArtistSeed:
                return artist_seeds.isSeedPresent (seed);
            case MusicThingie::Type::SongSeed:
                return song_seeds.isSeedPresent (seed);
            case MusicThingie::Type::PlaylistSeed:
                return genre_seeds.isSeedPresent (seed);
            default:
                assert (!"Unexpected type in seed");
                return false;
        }
    }

    bool Station::seed (MusicThingie::Type seed_type, const MusicThingie *music) const {
        assert (canSeed (seed_type));
        assert (music);

        // Seed check can be called from current-song announcements.  Don't throw.
        try {
            // Checking for seeding should not change anything, but we need to
            // break constness to refresh seed caches.
            const_cast<Station *> (this)->refreshSeedsAndRatings();
        } catch (CommandError &) {
            // Communication error; ignore it
        }

        // Seed determination
        if (music->isSeed()) {
            return seedAppliesToStation (music);
        } else if (seed_type == MusicThingie::Type::Artist) {
            assert (music->asArtist());
            return (artist_seeds.getDetail (music) != nullptr);
        } else if (seed_type == MusicThingie::Type::Song) {
            assert (music->asSong());
            return (song_seeds.getDetail (music) != nullptr);
        }
        return false;
    }

    void Station::seed (MusicThingie::Type seed_type, MusicThingie *music, bool value) {
        assert (MusicThingie::isPrimary (seed_type));
        assert (music);
        assert (canSeed (seed_type));

        refreshSeedsAndRatings();

        MusicThingie *seed_record{ nullptr };
        if (music->isSeed() && seedAppliesToStation (music)) {
            seed_record = music;
            if (seed_type != music->primaryType()) {
                throw CommandError (E_WRONGTYPE, "Seed type does not match item type.");
            }
        } else if (seed_type == MusicThingie::Type::Playlist) {
            seed_record = genre_seeds.getDetail (music);
        } else if (seed_type == MusicThingie::Type::Artist) {
            seed_record = artist_seeds.getDetail (music);
        } else {
            seed_record = song_seeds.getDetail (music);
        }

        if (value == (seed_record != nullptr)) {
            // Already in correct seed state.
            return;
        }

        if (value && seed_type == MusicThingie::Type::Playlist
            && music->type() != MusicThingie::Type::PlaylistSuggestion) {
            music = pandora()->getSuggestion (music, seed_type, SearchRange::KNOWN);
        } else if (value && music->type() != seed_type) {
            music = pandora()->getSuggestion (music, seed_type, SearchRange::SHALLOW);
        }

        takePossession();
        if (value) {
            std::string seed_id = music->internalId (seed_type);
            assert (!seed_id.empty());
            RequestAddStationSeed add_seed (this, seed_id);
            Status status = pandora()->executeRequest (add_seed);
            if (status != Status::Ok) {
                throw CommandError (E_NAK, status_strerror (status));
            }

            if (seed_type == MusicThingie::Type::Playlist) {
                genre_seeds.storeDetail (music, add_seed.getGenreSeed());
                pandora()->library.add (add_seed.getGenreSeed());
            } else if (seed_type == MusicThingie::Type::Artist) {
                artist_seeds.storeDetail (music, add_seed.getArtistSeed());
                pandora()->library.add (add_seed.getArtistSeed());
            } else {
                song_seeds.storeDetail (music, add_seed.getSongSeed());
                pandora()->library.add (add_seed.getSongSeed());
            }
        } else {
            std::string seed_id = seed_record->internalId (seed_type);
            assert (!seed_id.empty());

            RequestRemoveStationSeed remove_seed (this, seed_id);
            Status status = pandora()->executeRequest (remove_seed);
            if (status != Status::Ok) {
                throw CommandError (E_NAK, status_strerror (status));
            }
            if (seed_type == MusicThingie::Type::Playlist) {
                genre_seeds.erase (static_cast<GenreSeed *> (seed_record));
                pandora()->library.erase (seed_record);
            } else if (seed_type == MusicThingie::Type::Artist) {
                artist_seeds.erase (static_cast<ArtistSeed *> (seed_record));
                pandora()->library.erase (seed_record);
            } else {
                song_seeds.erase (static_cast<SongSeed *> (seed_record));
                pandora()->library.erase (seed_record);
            }
        }
    }

    /// Retrieve a station's seed information on file with the servers.
    void Station::refreshSeedsAndRatings() {
        if (details_expiration > time (nullptr)) {
            return;
        }

        MusicAutoReleasePool pool;
        RequestStationDetails detail_request (this);
        Status status = pandora()->executeRequest (detail_request);
        if (status != Status::Ok) {
            pandora()->alert (F_PANDORA, "Unable to refresh details", status_strerror (status).c_str());
            if (!details_expiration) {
                throw CommandError (E_NAK, status_strerror (status));
            }
            return;
        }

        genre_seeds = detail_request.getGenreSeeds();
        artist_seeds = detail_request.getArtistSeeds();
        song_seeds = detail_request.getSongSeeds();
        auto ratings = detail_request.positiveFeedback();
        ratings.join (detail_request.negativeFeedback());
        feedback = std::move (ratings);

        for (auto item : genre_seeds.list()) {
            pandora()->library.add (item);
        }
        pandora()->library.add (artist_seeds.list());
        pandora()->library.add (song_seeds.list());
        pandora()->library.add (feedback.list());
        details_expiration = time (nullptr) + StationDetailCacheTime;
    }

    // Forcibly update some ratings by expiring the cache and updating.
    void Station::forceRefreshDetails() {
        details_expiration = 1;
        refreshSeedsAndRatings();
    }

    ThingieList Station::getSeeds (void) const {
        // The API request thinks we don't need to change; we're just returning an existing list.
        // However, we may need to refresh the cache.  So, we're going to break constness.
        const_cast<Station *> (this)->refreshSeedsAndRatings();

        ThingieList results{ artist_seeds.list() };
        std::copy (song_seeds.list().begin(), song_seeds.list().end(), std::back_inserter (results));
        std::copy (genre_seeds.list().begin(), genre_seeds.list().end(), std::back_inserter (results));
        std::copy (feedback.list().begin(), feedback.list().end(), std::back_inserter (results));
        return results;
    }

}  // namespace Pandora
