///
/// Public interface for sources, which provide an interface
/// interfaces to select available music, adjust playlists, etc.
/// @file       mediaunit.h - pianod project
/// @author     Perette Barella
/// @date       2014-10-23
/// @copyright  Copyright 2012–2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <cstdio>

#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

#include "fundamentals.h"
#include "musictypes.h"
#include "filter.h"
#include "ownership.h"

namespace Parsnip {
    class Data;
}

/// Media source, source parameters and player interfaces.
namespace Media {
    class SourceParameters;
    class Player;
    class Manager;
    class SplitId;

    namespace Key {
        extern const char *Source;
        extern const char *SourceId;
        extern const char *SourceKind;
        extern const char *SourceName;
    }

    /// Statistics on media played from the sources.
    struct Statistics {
        int songs_attempted = 0;
        int tracks_played = 0;
        int playback_failures = 0;
        int sequential_failures = 0;
        int songs_replaced = 0;
        int songs_donated = 0;
    };

    /// The manner in which shuffling is performed.
    enum class SelectionMethod {
        Song,       ///< Songs are picked randomly from the union of applicable playlists.
        Album,      ///< An entire album is picked randomly from a playlist.
        Artist,     ///< An artist is picked randomly, then a few of their songs are picked.
        Playlist,   ///< A playlist is picked randomly, then a few of its songs randomly.
        Random      ///< One of the above methods is randomly chosen.
    };

    /** Base class that wraps any audio source, such as Pandora, Spotify, or local music.
    Provides a standard interface that pianod can work with.  Players can extend
    their interface with custom commands by deriving from both this and PianodInterpreter,
    and providing their specialized commands.  Care should be used to avoid polluting
    command space with potentially generic commands. */
    class Source : public PrimaryOwnership {
        friend class Manager;
    public:
        typedef unsigned long SerialNumber;

    protected:
        enum class State {
            INITIALIZING,   ///< Nothing known yet
            VALID,          ///< Credentials valid/data exists, but still initializing or offline.
            READY,          ///< Fully initialized and ready to play music
            DEAD            ///< Waiting for disposal
        };
        State state = State::INITIALIZING;

        /// Configuration details for this source, provided on source creation.
        const SourceParameters *parameters = nullptr;

    private:
        // Some state manipulated by the media manager
        SerialNumber serialNum = 0; ///< Assigned by media manager when registered.
        State announced_state = State::INITIALIZING; ///< Used by media manager for managing callbacks
        Statistics statistics; ///< Source playback statistics.
        time_t lockout_until = 0;

        static void defaultStatusHandler (RESPONSE_CODE status, const char *detail);
        /// Hook for the status handler, allowing status messages to be rerouted.
        std::function <void (RESPONSE_CODE status, const char *detail)> statusHandler = defaultStatusHandler;
        
    public:
        Source (SourceParameters *params);
        virtual ~Source (void);
        
        void persist (void) const;
        virtual bool flush (void);
        /// Check if the source is online.
        inline bool isReady (void) const { return state == State::READY; };
        /// Get the source's unique number assigned by the media manager.
        inline SerialNumber serialNumber (void) const { return (serialNum); };
        /// Retrieve playback statistics about the source.
        inline const Statistics &getStatistics () const { return statistics; };
        /// Check if source is temporarily locked out due to repeated failures.
        inline const bool isLockedOut () const { return lockout_until > time(nullptr); };

        Parsnip::Data serializeIdentity() const;
        std::string identify() const;
        void alert (RESPONSE_CODE message) const;
        void alert (RESPONSE_CODE message, const char *detail, const char *reason = nullptr) const;
        void reportStatus (const char *detail) const;
        inline void reportStatus (const std::string &detail) const {
            reportStatus (detail.c_str());
        };

        // Identity
        /// A unique string identifying the type of source.
        virtual const char *kind (void) const = 0;
        const std::string &name (void) const;
        /// Key is a combination of kind and name.
        /// Key must be consistent between invokations, as it is
        /// user preferences related to the source are identified.
        std::string key (void) const {
            return std::string (kind()) + "#" + name();
        }
        /// Provide a filename for persisting the source's data.
        std::string filename (void) const {
            return std::string (kind()) + "-" + name() + ".json";
        }

        // Capabilities
        virtual bool canExpandToAllSongs (void) const;
        virtual bool requireNameForCreatePlaylist (void) const;

        // Playlist methods
        PianodPlaylist *getPlaylistByName (const char *name);

        /** Retrieve the source's playlists.
            @param filter Criteria specifying the subset of the playlists to retrieve.
            @return Playlists matching the filter, or all playlists if omitted. */
        virtual PlaylistList getPlaylists (const Filter &filter = Filter::All) = 0;
        virtual PianodPlaylist *getMixPlaylist (void) = 0;
        virtual PianodPlaylist *getEverythingPlaylist (void) = 0;
        virtual PianodPlaylist *getTransientPlaylist (const Filter &criteria);
        virtual PianodPlaylist *createPlaylist (const char *name,
                                              MusicThingie::Type type,
                                              MusicThingie *from);
        virtual PianodPlaylist *createPlaylist (const char *name, const Filter &filter);
        virtual MusicThingie *getAnythingById (const SplitId &id) = 0;

        // Miscellaneous 
        virtual Player *getPlayer (const AudioSettings &audio, PianodSong *song) = 0;
        virtual SongList getRandomSongs (PianodPlaylist *playlist, const UserList &users,
                                         SelectionMethod selectionMethod) = 0;
        virtual ThingieList getSuggestions (const Filter &filter, SearchRange what = SearchRange::EXHAUSTIVE) = 0;
        virtual float periodic (void);
        void playbackComplete (bool played, bool successfully);
        virtual void playbackProblem (void) { };

        // Sourcecasts
        /** Retrieve an equivalent or related album, artist, or song from a source.
            To support pianod's engine, this converts items between sources.
            The engine should not be asking for type conversions within a source.
            @param thing The item to match.
            @param type The desired type to find.
            @param where Selects the depth/exhaustiveness of search.
            @return The single, best-matching item.
            @throw CommandError with:
            - E_INVALID (inadequate criteria)
            - E_AMBIGUOUS (multiple matches)
            - E_NOTFOUND. */
        virtual MusicThingie *getSuggestion (MusicThingie *thing,
                                             MusicThingie::Type type,
                                             SearchRange where = SearchRange::SHALLOW) = 0;

        /** Retrieve an equivalent album, artist, or song from a source.
            @see `getSuggestion`, 3-parameter version. */
        inline MusicThingie *getSuggestion (MusicThingie *thing,
                                            SearchRange where = SearchRange::SHALLOW) {
            return getSuggestion (thing, thing->type(), where);
        }
    protected:
        MusicThingie *getSuggestion (MusicThingie *thing,
                                     MusicThingie::Type type,
                                     SearchRange where,
                                     bool fully_confirm);
    };
    using SourcePtr = std::unique_ptr<Source>;

    /// Mechanism to split MusicThingie IDs into source identifier, type and inner ID.
    class SplitId {
    public:
        const std::string wholeId; ///< Complete ID
        Source::SerialNumber serialNumber; ///< The source's ID number assigned by the media manager
        Source *source; ///< The source associated with the serial number.
        MusicThingie::Type type; ///< The type of music thingie that was decoded.
        std::string innerId; ///< The item ID with the source serial number and type code removed.

        explicit SplitId (const std::string &id);
    };
    
}

