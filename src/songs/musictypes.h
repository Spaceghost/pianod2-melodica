///
/// Playlist / Artist / Album / Song data types.
/// @file       musictypes.h - pianod
/// @author     Perette Barella
/// @date       2014-12-09
/// @copyright  Copyright (c) 2014-2023 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>
#include <stack>
#include <vector>
#include <list>
#include <unordered_map>
#include <type_traits>

#include "logging.h"
#include "ownership.h"
#include "ratings.h"
#include "ratings.h"

namespace Media {
    class Source;
    class Player;
    enum class SelectionMethod;
}

#include "fundamentals.h"
#include "ownership.h"

class Filter;
class ThingieList;
class SongList;
class PlaylistList;
class PianodConnection;
class ResponseGroup;
namespace Football {
    class Thingie;
}
namespace Parsnip {
    class Data;
}


class MusicThingie;
class PianodArtist;
class PianodAlbum;
class PianodSong;
class PianodPlaylist;

class MusicAutoReleasePool : private std::stack<MusicThingie *> {
    friend class MusicThingie;
    MusicAutoReleasePool *previousPool;
    inline void add (MusicThingie *item) {
        push (item);
    }
    void unadd (MusicThingie *item);
public:
    MusicAutoReleasePool ();
    ~MusicAutoReleasePool ();
};

/** Base class for songs, albums, artists, playlists, genres, etc.
    This class is reference counted using an autorelease pool;
    the pool must be periodically released (such as at end end of
    the main run loop) to free things up.
 
    This offers a simplistic but effective and efficient garbage
    collection scheme.  For more information, see Cocoa/NextStep.
 
    @warning This class and derived ones must be dynamically allocated.
    Static, global or automatic (stack) objects will be released
    improperly and deleted, which is inappropriate. */
class MusicThingie : public SubordinateOwnership {
    friend class MusicAutoReleasePool;
private:
    mutable short useCount = 1;
    static MusicAutoReleasePool *releasePool;
protected:
    // Non-public destructor prevents allocation outside heap (C++PL/4 525)
    virtual ~MusicThingie (void);
public:
    enum class Type: char {
        Playlist = 'p',
        Artist = 'a',
        Album = 'l',
        Song = 's',

        PlaylistSuggestion = 'e',
        ArtistSuggestion = 't',
        AlbumSuggestion = 'b',
        SongSuggestion = 'n',

        PlaylistSeed = 'y',
        ArtistSeed = 'd',
        AlbumSeed = 'u',
        SongSeed = 'g',

        SongRating = 'i'
    };
    static std::string TypeName (Type type);
    static Type TypeFromName (const std::string &name);

    MusicThingie (void);

    static inline constexpr bool isPrimary (Type t) {
        return (t == Type::Playlist ||
                t == Type::Artist ||
                t == Type::Album ||
                t == Type::Song);
    }
    inline bool isPrimary (void) const {
        return isPrimary (type());
    }
    static inline constexpr bool isSuggestion (const Type t) {
        return (t == Type::PlaylistSuggestion ||
                t == Type::ArtistSuggestion ||
                t == Type::AlbumSuggestion ||
                t == Type::SongSuggestion);
    }
    inline bool isSuggestion (void) const {
        return isSuggestion (type());
    }
    static inline constexpr bool isSeed (const Type t) {
        return (t == Type::PlaylistSeed ||
                t == Type::ArtistSeed ||
                t == Type::AlbumSeed ||
                t == Type::SongSeed ||
                t == Type::SongRating);
    }
    inline bool isSeed (void) const {
        return isSeed (type());
    }

    static inline constexpr bool isPlaylist (const Type t) {
        return (t == Type::Playlist ||
                t == Type::PlaylistSuggestion ||
                t == Type::PlaylistSeed);
    }
    inline bool isPlaylist (void) const {
        return isPlaylist (type());
    }
    static inline constexpr bool isSong (const Type t) {
        return (t == Type::Song ||
                t == Type::SongSuggestion ||
                t == Type::SongSeed ||
                t == Type::SongRating);
    }
    inline bool isSong (void) const {
        return isSong (type());
    }
    static inline constexpr bool isAlbum (const Type t) {
        return (t == Type::Album ||
                t == Type::AlbumSuggestion ||
                t == Type::AlbumSeed);
    }
    inline bool isAlbum (void) const {
        return isAlbum (type());
    }
    static inline constexpr bool isArtist (const Type t) {
        return (t == Type::Artist ||
                t == Type::ArtistSuggestion ||
                t == Type::ArtistSeed);
    }
    inline bool isArtist (void) const {
        return isArtist (type());
    }
    static inline constexpr bool isValidType (const Type t) {
        return (isSong (t) || isAlbum (t) || isArtist (t) || isPlaylist (t));
    };
    inline bool isValidType (void) const {
        return isValidType (type());
    }
    static Type primaryType (const Type t);
    inline Type primaryType (void) const {
        return primaryType (type());
    }
    
    /// Indicate if specific item can be queued/requested.
    virtual bool canQueue() const { return false; };
    
    // Speed up and ease typecasting
    virtual PianodArtist *asArtist () { return nullptr; }
    virtual PianodAlbum *asAlbum () { return nullptr; }
    virtual PianodSong *asSong () { return nullptr; }
    virtual PianodPlaylist *asPlaylist () { return nullptr; }
    virtual const PianodArtist *asArtist () const { return nullptr; }
    virtual const PianodAlbum *asAlbum () const { return nullptr; }
    virtual const PianodSong *asSong () const { return nullptr; }
    virtual const PianodPlaylist *asPlaylist () const { return nullptr; }

    std::string operator()(void) const;

    /// Claim an instance.
#ifndef NDEBUG
    inline
#endif
    void retain (void) const {
        useCount++;
#ifndef NDEBUG
        flog (Log::ALLOCATIONS, "Retained ", (*this)(), ", useCount=", useCount);
#endif
};
    /// Abandon an instance.
#ifndef NDEBUG
    inline
#endif
    void release (void) {
        assert (useCount > 0);
#ifndef NDEBUG
        flog (Log::ALLOCATIONS, "Releasing ", (*this)(), ", useCount=", useCount - 1);
#else
        if (useCount == 1) {
            flog (Log::ALLOCATIONS, "Deleting ", (*this)());
        }
#endif
        if (--useCount == 0) delete this;
    };
    inline int getUseCount (void) const { return useCount; };

    /// Defer the ownership to the source 
    virtual Ownership *parentOwner (void) const;
    /// MediaSource from which this thingie originates. 
    virtual Media::Source * const source (void) const = 0;
    /** Get the primary id of this thingie.  If this is an artist, the artist ID;
        if an album, the album ID; etc. */
    virtual const std::string id (void) const = 0;
    /** Return the complete ID when used in a specific context */
    virtual const std::string id (MusicThingie::Type type) const = 0;
    /** Return the inner ID when used in a specific context */
    virtual const std::string &internalId (MusicThingie::Type type) const = 0;
    /** Return the most specific name of this, whatever type it is. */
    virtual const std::string &name (void) const = 0;
    /** Return the type letter for a thingie.  This is used as the first letter
        of the ID, so IDs can be routed to the appropriate dataset/handler. */
    virtual Type type (void) const = 0;
    /** Transmit the thingie's data to a connection or service. */
    virtual Football::Thingie &transmitCommon (Football::Thingie &recipient) const = 0;
    /** Transmit the thingie's user-specific data on a connection. */
    virtual PianodConnection &transmitPrivate (PianodConnection &recipient) const = 0;
    /** Assemble the thingie's data for JSON tranmission. */
    virtual Parsnip::Data serialize () const = 0;
    /** Add the thingie's user-specific data for JSON transmission. */
    virtual void serializePrivate (Parsnip::Data &, const User *user) const = 0;
    void serializeCommon (Parsnip::Data &) const;
    /** Check if a filter matches this item */
    virtual bool matches (const Filter &filter) const = 0;
    /** Check if the primary name of this thingie matches.  If this is an artist,
        compares to the artist name; if an album, the album name; etc. */
    virtual bool operator==(const std::string &compare) const = 0;
    /** Compare a thingie's name to another of the equivalent or decendent type.
        @return Whether primary names are the same.
        If compared object is not equivalent or decendent, returns false., */
    virtual bool operator==(const MusicThingie &compare) const = 0;
    inline bool operator !=(const std::string &compare) {
        return !(*this == compare);
    }
    inline bool operator !=(const MusicThingie &compare) {
        return !(*this == compare);
    }
    virtual SongList songs ();
};

/*
 *              Artists
 */

/** Base class for artists, derived from MusicThingies. */
class PianodArtist : public MusicThingie {
public:
    virtual const std::string &artistId (void) const = 0; ///< Item's artist ID.
    virtual const std::string id (void) const override; ///< Get primary ID with media manager ID on it
    virtual const std::string id (MusicThingie::Type type) const override;
    virtual const std::string &internalId (MusicThingie::Type type) const override;
    virtual const std::string &name (void) const override { return artist(); };
    virtual Type type (void) const override { return Type::Artist; }; ///< Typecode for artist objects.
    static inline Type typetype (void) { return Type::Artist; }; ///< Typecode for artist type.
    virtual Football::Thingie &transmitCommon (Football::Thingie &recipient) const override;
    virtual PianodConnection &transmitPrivate (PianodConnection &recipient) const override;
    virtual Parsnip::Data serialize () const override;
    virtual void serializePrivate (Parsnip::Data &, const User *user) const override;
    virtual bool matches (const Filter &filter) const override;
    virtual PianodArtist *asArtist () override { return this; }
    virtual const PianodArtist *asArtist () const override { return this; }

    virtual const std::string &artist (void) const = 0; ///< Get artist name.

    /** See if the artist has a certain name.
     @return True/false.  Uses fuzzy logic, skipping 'a'/'an'/'the' and adjusting
     for /first last/ to /last, first/ */
    virtual bool operator==(const std::string &compare) const override;
    /** Compare artist's name to that of an artist/album/song.
     @return True/false.  Uses fuzzy logic, skipping 'a'/'an'/'the' and adjusting
     for /first last/ to /last, first/ */
    virtual bool operator==(const MusicThingie &compare) const override;
};


/*
 *              Albums
 */

/** Base class for albums, these are also MusicThingies and artists. */
class PianodAlbum : public PianodArtist {
public:
    virtual const std::string &albumId (void) const = 0; ///< Item's album ID
    virtual const std::string id (void) const override; ///< Primary ID is album ID.
    virtual const std::string id (MusicThingie::Type type) const override;
    virtual const std::string &internalId (MusicThingie::Type type) const override;
    virtual const std::string &name (void) const override { return albumTitle(); };
    virtual Type type (void) const override { return Type::Album; }; ///< Typecode for album objects.
    static inline Type typetype (void) { return Type::Album; }; ///< Typecode for album type.
    virtual Football::Thingie &transmitCommon (Football::Thingie &recipient) const override;
    virtual PianodConnection &transmitPrivate (PianodConnection &recipient) const override;
    virtual Parsnip::Data serialize () const override;
    virtual void serializePrivate (Parsnip::Data &, const User *user) const override;
    virtual bool matches (const Filter &filter) const override;
    virtual PianodArtist *asArtist () override { return compilation() ? nullptr : this; }
    virtual const PianodArtist *asArtist () const override { return compilation() ? nullptr : this; }
    virtual PianodAlbum *asAlbum () override final { return this; }
    virtual const PianodAlbum *asAlbum () const override final { return this; }

    virtual const std::string &albumTitle (void) const = 0;
    virtual const std::string &coverArtUrl(void) const = 0;
    virtual bool compilation () const { return false; }; ///< Is album a compilation?
    virtual bool operator==(const std::string &compare) const override;
    virtual bool operator==(const MusicThingie &compare) const override;
};


/*
 *              Songs
 */

/** Base class for songs, these are also MusicThingies, artists and albums. */
class PianodSong : public PianodAlbum {
protected:
    time_t expiration = 0; ///< Time at which a queued item becomes invalid, or 0 if good forever
    time_t last_played = 0; ///< Time song was last played, 0 if never or unknown.
public:
    virtual const std::string &songId (void) const = 0;
    virtual const std::string id (void) const override;
    virtual const std::string id (MusicThingie::Type type) const override final;
    virtual const std::string &internalId (MusicThingie::Type type) const override final;
    virtual const std::string &name (void) const override final { return title(); };
    virtual Type type (void) const override { return Type::Song; };
    static inline Type typetype (void) { return Type::Song; };
    virtual Football::Thingie &transmitCommon (Football::Thingie &recipient) const override final;
    virtual PianodConnection &transmitPrivate (PianodConnection &recipient) const override;
    virtual Parsnip::Data serialize () const override final;
    virtual void serializePrivate (Parsnip::Data &, const User *user) const override;
    ResponseGroup assembleRatings (const User *user, const PianodPlaylist *playlist, bool include_json) const;
    ResponseGroup assembleCapabilities (const User *user, const PianodPlaylist *playlist) const;
    Parsnip::Data serializeRatings (const User *user, const PianodPlaylist *playlist) const;
    Parsnip::Data serializeCapabilities (const User *user, const PianodPlaylist *playlist) const;
    virtual bool matches (const Filter &filter) const override final;
    virtual PianodSong *asSong () override final { return this; }
    virtual const PianodSong *asSong () const override final { return this; }

    inline bool expires (void) { return expiration != 0; };
    inline bool expired (void) { return expires() && expiration < time (NULL); };
    /** Get time a song last played.
        @return Timestamp, or 0 if it has never played/last play is unknown. */
    inline time_t lastPlayed (void) const { return last_played; };
    inline void lastPlayed (time_t t) { last_played = t; };

    virtual bool isMelodicaTrack() const { return false; }
    virtual const std::string &title (void) const = 0; ///< Get the song's title
    virtual PianodPlaylist *playlist (void) const = 0; ///< Get a playlist instance
    virtual const std::string &playlistName (void) const;
    virtual const std::string &genre (void) const = 0; ///< Get genre of this song
    virtual const std::string &infoUrl (void) const;
    /** Get track number (sequence number within an album/original media).
        @return Track number, or 0 if unknown. */
    virtual int trackNumber (void) const = 0;
    /** Duration of song in seconds.
        @return Play time of song, or 0 if unknown */
    virtual int duration (void) const = 0;
    /** Year of release of song.
        @return Year, or 0 if unknown. */
    virtual int year (void) const = 0;

    virtual RatingScheme ratingScheme (void) const { return RatingScheme::NOBODY; };
    /// Rate a song.
    /// @param value The rating to assign.
    /// @param user The user making the rating; use depends on source.
    virtual RESPONSE_CODE rate (Rating value, User *user) = 0;
    /// Get a song's rating.
    virtual Rating rating (const User *user) const = 0;
    virtual RESPONSE_CODE rateOverplayed (User *) { return E_NOT_IMPLEMENTED; };
    float averageRating () const;

    virtual bool operator==(const std::string &compare) const override;
    virtual bool operator==(const MusicThingie &compare) const override;

    // Actions and action checks
    virtual SongList songs () override;
    virtual bool canSkip (time_t *whenAllowed);
    virtual bool mustPlay () const;
    Media::Player *play (const AudioSettings &audio);
};







/*
 *              Playlists
 */

/** Base class for playlists, but still a MusicThingie. */
class PianodPlaylist : public MusicThingie {
public:
    enum PlaylistType {
        SINGLE,
        MIX,
        EVERYTHING,
        TRANSIENT
    };

    virtual const std::string &playlistId (void) const = 0; ///< Item's playlist ID.
    virtual const std::string id (void) const override; ///< Primary ID is playlist ID.
    virtual const std::string id (MusicThingie::Type type) const override final;
    virtual const std::string &internalId (MusicThingie::Type type) const override final;
    virtual const std::string &name (void) const override final { return playlistName(); };
    virtual Type type (void) const override { return Type::Playlist; }; ///< Typecode for playlist objects.
    static inline Type typetype (void) { return Type::Playlist; }; ///< Typecode for playlist type.
    virtual Football::Thingie &transmitCommon (Football::Thingie &recipient) const override final;
    virtual PianodConnection &transmitPrivate (PianodConnection &recipient) const override;
    class Response assembleRatings (const User *user) const;
    virtual Parsnip::Data serialize () const override final;
    virtual void serializePrivate (Parsnip::Data &, const User *user) const override;
    Parsnip::Data serializeRatings (const User *user) const;
    virtual bool matches (const Filter &filter) const override final;
    virtual PianodPlaylist *asPlaylist () override final { return this; }
    virtual const PianodPlaylist *asPlaylist () const override final { return this; }

    virtual PlaylistType playlistType (void) const = 0; ///< Mix, everything, transient or single list
    virtual const std::string &playlistName (void) const = 0; ///< Name of the playlist
    virtual const std::string &genre (void) const = 0; ///< Get genre of this playlist

    virtual SongList getRandomSongs (const UserList &users,
                                     Media::SelectionMethod selectionMethod);
    virtual bool operator==(const std::string &compare) const override;
    virtual bool operator==(const MusicThingie &compare) const override;
    virtual bool includedInMix (void) const = 0;
    virtual void includedInMix (bool include) = 0;
    RESPONSE_CODE rate (Rating value, User *user);
    Rating rating (const User *user) const;
    float averageRating () const;

    virtual bool canSeed (MusicThingie::Type seedType) const;
    virtual bool seed (MusicThingie::Type seedType, const MusicThingie *music) const;
    virtual void seed (MusicThingie::Type seedType, MusicThingie *music, bool value);
    virtual ThingieList getSeeds (void) const;
    virtual SongList songs () override;
    virtual SongList songs (const Filter &filter);

    virtual void updateSelector (const Filter &new_selector);
    virtual void rename (const std::string &newname) = 0;
    virtual void erase () = 0;
};


/** Template to make a corresponding transient playlist. */
template<class BasePlaylist>
class PianodTransientPlaylist : virtual public BasePlaylist {
public:
    virtual PianodPlaylist::PlaylistType playlistType (void) const override { return PianodPlaylist::TRANSIENT; };

    virtual bool includedInMix (void) const override { return false; };
    virtual void includedInMix (bool) override { throw CommandError (E_MEDIA_TRANSIENT); };

    virtual bool canSeed (MusicThingie::Type) const override { return false; };
    virtual bool seed (MusicThingie::Type, const MusicThingie *) const override {
        assert (!"Queried seed for transient playlist.");
        return false;
    }
    virtual void seed (MusicThingie::Type, MusicThingie *, bool) override {
        assert (!"Set seed for transient playlist");
    }

    virtual void rename (const std::string &) override { throw CommandError (E_MEDIA_TRANSIENT); };
    virtual void erase () override { throw CommandError (E_MEDIA_TRANSIENT); };
};



/*
 *              Thingie Management
 */

/// Look up table for thingie type names and type codes.
typedef const LookupTable<MusicThingie::Type> ThingieTypesLookup;
extern ThingieTypesLookup THINGIETYPES;

