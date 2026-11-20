///
/// Pandora Playlist (station) support.
/// @file       mediaunits/pandora/pandorastation.h - pianod project
/// @author     Perette Barella
/// @date       2020-04-04
/// @copyright  Copyright 2020-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>
#include <unordered_map>
#include <list>

#include <parsnip/parsnip.h>

#include "fundamentals.h"
#include "musictypes.h"
#include "mediaparameters.h"
#include "encapmusic.h"
#include "retainer.h"
#include "retainedlist.h"

namespace Pandora {
    class Source;

    extern const int StationListCacheTime;
    extern const int StationDetailCacheTime;

    /// A class to manage Pandora skips.
    class SkipTracker : private std::list<time_t> {
        size_t skip_limit;  ///< The limit on the number of skips.
        time_t interval;    ///< The duration over which the limit applies.

        /// Remove expired skips from the front of the list.
        inline void purge() {
            while (!empty() && front() < time (nullptr)) {
                pop_front();
            }
        }

    public:
        SkipTracker (int limit, int interv);
        bool canSkip (time_t *when);
        bool skip (time_t *when = nullptr);
        inline void setLimit (int limit) {
            skip_limit = limit;
        }
    };

    // Helper class for storing/tracking seeds and ratings.
    template <typename InfoType>
    class StationDefinitionDetails {
        using ListType = RetainedList <InfoType *>;
        friend class Station;
        ListType info_list; ///< The authoritative list of details.
        MusicThingie::Type type; ///< The primary type stored herein.
        mutable std::unordered_map<std::string, Retainer<InfoType *>> cache; ///< Faster access to info

        InfoType *find (const MusicThingie *primary) const;

    private:
        inline StationDefinitionDetails (MusicThingie::Type ty) : type (ty) {};
        inline const ListType &list() const {
            return info_list;
        }
        void operator= (ListType &&list);
        bool isSeedPresent (const MusicThingie *seed) const;
    public:
        void storeDetail (const MusicThingie *primary, InfoType *rating);
        InfoType *getDetail (const MusicThingie *primary) const;
        void erase (InfoType *item);
    };

    class Station : public EncapsulatedPlaylist {
        friend class RequestTransformStation;

        std::string station_token;
        bool is_shared{ false };     ///< The station is owned by another listener (like a symlink)
        bool may_rename{ false };    ///< The user may rename the station.
        bool in_quick_mix{ false };  ///< The station is participating in the quickmix/shuffle.

        time_t details_expiration = 0;  ///< Time at which seeds expire.

    public:
        Station (Source *const owner, const Parsnip::Data &message);
        Station &operator= (const Station &update);

        void forceRefreshDetails();
        bool seedAppliesToStation (const MusicThingie *seed) const;
        void refreshSeedsAndRatings();
        void takePossession();

        SkipTracker skips{ 6, 3600 };
        StationDefinitionDetails<SongRating> feedback {MusicThingie::Type::Song};
        StationDefinitionDetails<SongSeed> song_seeds {MusicThingie::Type::Song};
        StationDefinitionDetails<ArtistSeed> artist_seeds {MusicThingie::Type::Artist};
        StationDefinitionDetails <GenreSeed> genre_seeds {MusicThingie::Type::Playlist};

    public:
        inline Source *const pandora() const {
            return reinterpret_cast<Source *const> (source());
        }

        // API implementation
        virtual bool includedInMix (void) const override final;
        virtual void includedInMix (bool include) override final;
        virtual bool canSeed (MusicThingie::Type seedType) const override;
        virtual bool seed (MusicThingie::Type seedType, const MusicThingie *music) const override;
        virtual void seed (MusicThingie::Type seedType, MusicThingie *music, bool value) override;
        virtual ThingieList getSeeds (void) const override;
        virtual void rename (const std::string &newname) override;
        virtual void erase() override;

        inline const std::string &getToken() const {
            return station_token;
        }
    };

    /// Class representing the Pandora quick mix station.
    class QuickMixStation : public Station {
    public:
        QuickMixStation (Source *const src, const Parsnip::Data &message) : Station (src, message) {
            playlistType (PianodPlaylist::MIX);
        }
    };

    /// Class representing a mix of all of a user's Pandora stations.
    class MixEverythingStation : public Station {
    public:
        MixEverythingStation (Source *const src, const Parsnip::Data &message) : Station (src, message) {
            playlistType (PianodPlaylist::EVERYTHING);
            playlistName ("Everything");
        }
    };

    using StationList = RetainedList<Station *>;

    /** Station lookup.
        Hash table is keyed on station id, not  *PANDORA* id or pianod ID. */
    class StationLookup : public std::unordered_map<std::string, Retainer<Station *>> {};

}  // namespace Pandora
