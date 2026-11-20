///
/// Pandora data types for pianod.
/// @file       mediaunits/pandora/pandora.h - pianod project
/// @author     Perette Barella
/// @date       2014-10-23
/// @copyright  Copyright 2012-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <ctime>

#include <string>
#include <unordered_map>
#include <functional>

#include "fundamentals.h"
#include "musictypes.h"
#include "musiccache.h"
#include "mediaplayer.h"
#include "mediaunit.h"
#include "mediaparameters.h"

#include "pandoratypes.h"
#include "pandoraparameters.h"
#include "pandorastation.h"
#include "pandoracomm.h"

/// Pandora source, player and related datatype specializations.
namespace Pandora {
    class Source;

    class Library : public PersistentPool {
    public:
        using PersistentPool::PersistentPool;
        inline Source *pandora() {
            return reinterpret_cast <Source *> (source);
        }
        virtual MusicThingie *reconstruct (MusicThingie::Type type, const Parsnip::Data &data) override;
    };

    class Source : public Media::Source {
        using SearchResults = std::unordered_map<std::string, ThingieList>;
    private:
        // Member variables
        StationLookup stations;                   ///< A collection of stations, indexed by station ID.
        time_t station_list_expiration = 0;       ///< When cached station list expires
        std::string station_list_checksum;        ///< Checksum from last station list received.
        time_t genre_stations_expiration = 0;     ///< When genre stations list expires
        std::string genre_stations_checksum;      ///< Checksum from last genre station list received.
        PlaylistList genre_stations;              ///< The genre stations
        Retainer<Station *> mix_playlist;         ///< The mix/shuffle meta-playlist.
        Retainer<Station *> everything_playlist;  ///< The everything meta-playlist.
        Communication comm;                       ///< Pandora server communicator.
        std::unique_ptr<Parsnip::Data> recovery;  ///< Data from persistence file

    public:
        Library library;               ///< Lookup table of things we gave out.
        SearchResults prior_searches;  ///< Search results saved for reuse;
        SkipTracker skips{60, 86400};  ///< Skip counts for limiting

        inline const ConnectionParameters *connectionParams() const {
            return static_cast<const ConnectionParameters *> (parameters);
        }
        Source (const ConnectionParameters &params);
        ~Source();
        using Media::Source::alert;

        // Identity
        virtual const char *kind (void) const override;

        // Capabilities
        virtual bool requireNameForCreatePlaylist (void) const override;

        // General source stuff
        virtual bool flush (void) override;
        virtual float periodic (void) override;

        // Playlist methods
        virtual PlaylistList getPlaylists (const Filter &filter = Filter::All) override;
        virtual PianodPlaylist *getMixPlaylist (void) override;
        virtual PianodPlaylist *getEverythingPlaylist (void) override;
        virtual Media::Player *getPlayer (const AudioSettings &audio, PianodSong *song) override;

        virtual MusicThingie *getAnythingById (const Media::SplitId &id) override;
        virtual ThingieList getSuggestions (const Filter &filter, SearchRange where) override;

        // Creating stations
        virtual PianodPlaylist *createPlaylist (const char *name, MusicThingie::Type type, MusicThingie *from) override;

        // Miscellaneous
        virtual SongList getRandomSongs (PianodPlaylist *playlist, const UserList &, Media::SelectionMethod) override;

        // Typecasts
        virtual MusicThingie *getSuggestion (MusicThingie *thing, MusicThingie::Type type, SearchRange where) override;

        // Functions for use within Pandora library.
        Station *getStationByStationId (const std::string &station_id);
        void removeStationByStationId (const std::string &station_id);
        inline Status executeRequest (Request &request) {
            return comm.execute (request);
        }
        inline const UserFeatures &userFeatures() const {
            return comm.getFeatures();
        }

    private:
        // Internal functions
        bool updateStationList (PianodPlaylist::PlaylistType mixSetting = PianodPlaylist::SINGLE);
        void updateGenreStations ();
        bool initializeMix();
        void pushMixToServers (bool mix_all = false);
        inline void setMixAllOnServers() {
            pushMixToServers (true);
        }
        bool restore ();
        bool persist () const;
    };
}  // namespace Pandora
