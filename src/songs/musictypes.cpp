///
/// Method implementations for Playlist / Artist / Album / Song data types.
/// @file       musictypes.cpp - pianod
/// @author     Perette Barella
/// @date       2014-12-09
/// @copyright  Copyright 2014-2023 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <ctime>

#include <typeinfo>
#include <string>
#include <sstream>
#include <vector>
#include <iomanip>

#include <football/football.h>
#include <parsnip/parsnip.h>

#include "connection.h"
#include "response.h"

#include "musictypes.h"
#include "retainedlist.h"
#include "musickeys.h"
#include "mediaunit.h"
#include "mediamanager.h"
#include "filter.h"
#include "utility.h"
#include "ownership.h"
#include "user.h"
#include "users.h"
#include "datastore.h"

/*
 *       Auto Release Pools & Reference Counting
 */
MusicAutoReleasePool::MusicAutoReleasePool() : previousPool (MusicThingie::releasePool) {
    MusicThingie::releasePool = this;
}

MusicAutoReleasePool::~MusicAutoReleasePool() {
    MusicThingie::releasePool = previousPool;
    while (!empty()) {
        top()->release();
        pop();
    }
}

/** Remove an item from the release pool when it fails subsequent construction. */
void MusicAutoReleasePool::unadd (MusicThingie *item) {
    assert (!empty());
    assert (top() == item);
    assert (item->useCount == 1);
    pop();
}

/** The current autorelease pool to put newly created music thingies into */
MusicAutoReleasePool *MusicThingie::releasePool = nullptr;

/** When allocated, use count starts at 1 and the object
    is put in the release pool, justifying its existence.
    If anyone wants it, they must retain() it before
    autorelease.  One MusicAutoReleasePool is in the main run
    loop; others may be created to shorten temporary lives.
    Retained objects are later deleted instantly when released(). */
MusicThingie::MusicThingie (void) {
    assert (releasePool);
    releasePool->add (this);
}

MusicThingie::~MusicThingie (void) {
    if (useCount != 0) {
        // The only way this "should" happen is if descendent constructor fails,
        // in which case we need to remove this from the autorelease pool.
        releasePool->unadd (this);
    }
}

bool MusicThingie::operator== (const std::string &compare) const {
    return compare_titles (name(), compare);
}

/** Insert common items into the serialization dictionary. */
void MusicThingie::serializeCommon (Parsnip::Data &serialdata) const {
    serialdata [Music::Key::PrimaryId] = id();
    serialdata [Music::Key::PrimaryName] = name();
    serialdata [Media::Key::Source] = source()->serializeIdentity();
}

/*
 *              Music Thingie
 */

using MusicTypeToTypeName = std::unordered_map<MusicThingie::Type, std::string>;
using MusicTypeNameToType = std::unordered_map<std::string, MusicThingie::Type>;

static const MusicTypeToTypeName music_type_to_type_name{
    { MusicThingie::Type::Playlist, Music::Key::Type::Playlist },
    { MusicThingie::Type::Artist, Music::Key::Type::Artist },
    { MusicThingie::Type::Album, Music::Key::Type::Album },
    { MusicThingie::Type::Song, Music::Key::Type::Song },
    { MusicThingie::Type::PlaylistSuggestion, Music::Key::Type::PlaylistSuggestion },
    { MusicThingie::Type::ArtistSuggestion, Music::Key::Type::ArtistSuggestion },
    { MusicThingie::Type::AlbumSuggestion, Music::Key::Type::AlbumSuggestion },
    { MusicThingie::Type::SongSuggestion, Music::Key::Type::SongSuggestion },

    { MusicThingie::Type::PlaylistSeed, Music::Key::Type::PlaylistSeed },
    { MusicThingie::Type::ArtistSeed, Music::Key::Type::ArtistSeed },
    { MusicThingie::Type::AlbumSeed, Music::Key::Type::AlbumSeed },
    { MusicThingie::Type::SongSeed, Music::Key::Type::SongSeed },

    { MusicThingie::Type::SongRating, Music::Key::Type::SongRating }
};

/** Get the type name of a music thingie.
    @param type The type whose name to retrieve.
    @return A string with the type name. */
std::string MusicThingie::TypeName (Type type) {
    auto it = music_type_to_type_name.find (type);
    return (it == music_type_to_type_name.end() ? "Unknown" : it->second);
}

static const MusicTypeNameToType flip_map (const MusicTypeToTypeName &map) {
    MusicTypeNameToType flipped_map;
    for (const auto &value : map) {
        flipped_map [value.second] = value.first;
    }
    return flipped_map;
}
static const std::unordered_map<std::string, MusicThingie::Type> music_type_name_to_type{ flip_map (music_type_to_type_name) };

MusicThingie::Type MusicThingie::TypeFromName (const std::string &name) {
    auto it = music_type_name_to_type.find (name);
    if (it == music_type_name_to_type.end()) {
        throw std::invalid_argument (name);
    }
    return it->second;
}


std::string MusicThingie::operator() (void) const {
    return TypeName (type()) + " \"" + name() + "\" (ID " + id() + ")";
}

MusicThingie::Type MusicThingie::primaryType (Type t) {
    if (isSong (t))
        return Type::Song;
    if (isAlbum (t))
        return Type::Album;
    if (isArtist (t))
        return Type::Artist;
    if (isPlaylist (t))
        return Type::Playlist;
    assert (!"primaryType of unknown type");
    throw std::invalid_argument ("Unknown MusicThingie type");
}

/** Retrieve a list of requestable songs applicable to this thingie. */
SongList MusicThingie::songs() {
    return SongList{};
};

Ownership *MusicThingie::parentOwner (void) const {
    return (source());
};

ThingieTypesLookup THINGIETYPES ({ { "artist", MusicThingie::Type::Artist },
                                   { "album", MusicThingie::Type::Album },
                                   { "song", MusicThingie::Type::Song },
                                   { "playlist", MusicThingie::Type::Playlist } });

/*
 *                  Artists
 */

const std::string PianodArtist::id (void) const {
    return std::to_string (source()->serialNumber()) + (char) type() + artistId();
};

const std::string PianodArtist::id (MusicThingie::Type type) const {
    const static std::string empty;
    assert (isPrimary (type));
    switch (type) {
        case MusicThingie::Type::Artist:
            return id();
        default:
            return empty;
    }
};

const std::string &PianodArtist::internalId (MusicThingie::Type type) const {
    const static std::string empty;
    assert (isPrimary (type));
    switch (type) {
        case MusicThingie::Type::Artist:
            return artistId();
        default:
            return empty;
            ;
    }
};

Football::Thingie &PianodArtist::transmitCommon (Football::Thingie &recipient) const {
    recipient << Response (I_ID, id()) << Response (I_ARTIST, artist()) << Response (I_SOURCE, source()->kind())
              << Response (I_NAME, source()->name());
    return recipient;
}

PianodConnection &PianodArtist::transmitPrivate (PianodConnection &recipient) const {
    if (isUsableBy (recipient.user) && isPrimary() && canQueue()) {
        recipient << Response (I_ACTIONS, Response::List{ "request" }, std::move (Response::NoJsonData));
    }
    return recipient;
}

Parsnip::Data PianodArtist::serialize() const {
    // clang-format off
    Parsnip::Data artist {Parsnip::Data::Dictionary,
        Music::Key::ArtistId, id ()
    };
    // clang-format on
    serializeCommon (artist);
    return artist;
}

void PianodArtist::serializePrivate (Parsnip::Data &artist, const User *user) const {
    // clang-format off
    artist [Music::Key::Actions] = Parsnip::Data {Parsnip::Data::Dictionary,
        Music::Key::CanRequest, isUsableBy (user) && isPrimary() && canQueue()
    };
    // clang-format on
}

bool PianodArtist::matches (const Filter &filter) const {
    return filter.matches (this);
}

bool PianodArtist::operator== (const std::string &compare) const {
    return compare_person_or_title (artist(), compare);
}

bool PianodArtist::operator== (const MusicThingie &compare) const {
    if (type() != compare.type())
        return false;

    auto other = dynamic_cast<const PianodArtist *> (&compare);
    assert (other);
    if (!other)
        return false;
    return compare_person_or_title (artist(), other->artist());
}

/*
 *                  Albums
 */

const std::string PianodAlbum::id (void) const {
    return std::to_string (source()->serialNumber()) + (char) type() + albumId();
};

const std::string PianodAlbum::id (MusicThingie::Type type) const {
    const static std::string empty;
    assert (isPrimary (type));
    switch (type) {
        case MusicThingie::Type::Artist:
            return PianodArtist::id();
        case MusicThingie::Type::Album:
            return id();
        default:
            return empty;
    }
};

const std::string &PianodAlbum::internalId (MusicThingie::Type type) const {
    const static std::string empty;
    assert (isPrimary (type));
    switch (type) {
        case MusicThingie::Type::Artist:
            return artistId();
        case MusicThingie::Type::Album:
            return albumId();
        default:
            return empty;
            ;
    }
};
Football::Thingie &PianodAlbum::transmitCommon (Football::Thingie &recipient) const {
    recipient << Response (I_ID, id());
    if (!artist().empty())
        recipient << Response (I_ARTIST, artist());
    recipient << Response (I_ALBUM, albumTitle()) << Response (I_SOURCE, source()->kind())
              << Response (I_NAME, source()->name());
    return recipient;
}

PianodConnection &PianodAlbum::transmitPrivate (PianodConnection &recipient) const {
    if (isUsableBy (recipient.user) && isPrimary() && canQueue()) {
        recipient << Response (I_ACTIONS, Response::List{ "request" }, std::move (Response::NoJsonData));
    }
    return recipient;
}

Parsnip::Data PianodAlbum::serialize() const {
    // clang-format off
    Parsnip::Data album {Parsnip::Data::Dictionary,
        Music::Key::AlbumId, id (),
        Music::Key::AlbumName, albumTitle(),
        Music::Key::AlbumIsCompilation, compilation(),
        Music::Key::AlbumArtUrl, nullptr
    };
    // clang-format on
    serializeCommon (album);
    if (compilation()) {
        album [Music::Key::ArtistId] = nullptr;
        album [Music::Key::ArtistName] = nullptr;
    } else {
        album [Music::Key::ArtistId] = id (Type::Artist);
        album [Music::Key::ArtistName] = artist();
    }
    return album;
}

void PianodAlbum::serializePrivate (Parsnip::Data &album, const User *user) const {
    // clang-format off
    album [Music::Key::Actions] = Parsnip::Data {Parsnip::Data::Dictionary,
        Music::Key::CanRequest, isUsableBy (user) && isPrimary() && canQueue()
    };
    // clang-format on
}

bool PianodAlbum::matches (const Filter &filter) const {
    return filter.matches (this);
}

bool PianodAlbum::operator== (const std::string &compare) const {
    return compare_titles (albumTitle(), compare);
}

bool PianodAlbum::operator== (const MusicThingie &compare) const {
    if (type() != compare.type())
        return false;

    auto other = dynamic_cast<const PianodAlbum *> (&compare);
    assert (other);
    if (!other)
        return false;
    return compare_titles (albumTitle(), other->albumTitle()) && compare_person_or_title (artist(), other->artist());
}

/*
 *              Songs
 */

const std::string PianodSong::id (void) const {
    return std::to_string (source()->serialNumber()) + (char) type() + songId();
};

const std::string PianodSong::id (MusicThingie::Type type) const {
    const static std::string empty;
    assert (isPrimary (type));
    switch (type) {
        case MusicThingie::Type::Artist:
            return PianodArtist::id();
        case MusicThingie::Type::Album:
            return PianodAlbum::id();
        case MusicThingie::Type::Song:
            return id();
        case MusicThingie::Type::Playlist:
            return (playlist() ? playlist()->id() : empty);
        default:
            return empty;
    }
};

const std::string &PianodSong::internalId (MusicThingie::Type type) const {
    const static std::string empty;
    assert (isPrimary (type));
    switch (type) {
        case MusicThingie::Type::Artist:
            return artistId();
        case MusicThingie::Type::Album:
            return albumId();
        case MusicThingie::Type::Song:
            return songId();
        case MusicThingie::Type::Playlist:
            return (playlist() ? playlist()->playlistId() : empty);
        default:
            return empty;
            ;
    }
};

Football::Thingie &PianodSong::transmitCommon (Football::Thingie &recipient) const {
    recipient << Response (I_ID, id());
    if (!albumTitle().empty())
        recipient << Response (I_ALBUM, albumTitle());
    if (!artist().empty())
        recipient << Response (I_ARTIST, artist());
    recipient << Response (I_SONG, title());
    if (!coverArtUrl().empty())
        recipient << Response (I_COVERART, coverArtUrl());
    PianodPlaylist *pl = playlist();
    if (pl)
        recipient << Response (I_PLAYLIST, pl->playlistName());
    if (!genre().empty())
        recipient << Response (I_GENRE, genre());
    if (!infoUrl().empty())
        recipient << Response (I_INFO_URL, infoUrl());
    if (year())
        recipient << Response (I_YEAR, year());
    if (duration())
        recipient << Response (I_DURATION, format_duration (duration()));
    recipient << Response (I_SOURCE, source()->kind()) << Response (I_NAME, source()->name());
    return recipient;
}

PianodConnection &PianodSong::transmitPrivate (PianodConnection &recipient) const {
    const PianodPlaylist *pl = playlist();
    assembleRatings (recipient.user, pl, false).transmitLine (recipient);
    assembleCapabilities (recipient.user, pl).transmitLine (recipient);
    if (pl) {
        pl->assembleRatings (recipient.user).transmitLine (recipient);
    }
    return recipient;
}

ResponseGroup PianodSong::assembleRatings (const User *user, const PianodPlaylist *playlist, bool include_json) const {
    ResponseGroup ratings;
    // Suggestions never have ratings.
    if (isSuggestion()) {
        return ratings;
    }

    // Primaries, seeds and ratings have some kind of rating.
    // (Except primaries don't have a rating for unauthenticated users.)
    // Announce seeds as unrated 0.0, so seed flags stay in place.
    Rating song_rating = Rating::UNRATED;
    if (ratingScheme() == RatingScheme::OWNER || (ratingScheme() == RatingScheme::INDIVIDUAL && user)) {
        song_rating = rating (user);
    }

    std::ostringstream rating;
    rating << std::setprecision (1) << std::fixed << ratingAsFloat (song_rating);

    Response::List rating_pieces{ RATINGS [song_rating], rating.str() };

    // Include seed flags if they apply.
    if (playlist) {
        if (playlist->canSeed (MusicThingie::Type::Song) && playlist->seed (MusicThingie::Type::Song, this))
            rating_pieces.push_back ("seed");
        if (playlist->canSeed (MusicThingie::Type::Album) && playlist->seed (MusicThingie::Type::Album, this))
            rating_pieces.push_back ("albumseed");
        if (playlist->canSeed (MusicThingie::Type::Artist) && playlist->seed (MusicThingie::Type::Artist, this))
            rating_pieces.push_back ("artistseed");
    }
    ratings (I_RATING,
             std::move (rating_pieces),
             include_json ? serializeRatings (user, playlist) : std::move (Response::NoJsonData));
    return ratings;
}

ResponseGroup PianodSong::assembleCapabilities (const User *user, const PianodPlaylist *playlist) const {
    ResponseGroup caps;
    // Suggestions never have ratings.
    if (isSuggestion()) {
        return caps;
    }

    // Provide a list of actions current user can take on this song
    Response::List capabilities;
    if ((ratingScheme() == RatingScheme::INDIVIDUAL && user)
        || (ratingScheme() == RatingScheme::OWNER && isEditableBy (user))) {
        capabilities.push_back ("rate");
        if (playlist && playlist->isEditableBy (user)) {
            // Include seed flags if they apply.
            if (playlist->canSeed (MusicThingie::Type::Song))
                capabilities.push_back ("seed");
            if (playlist->canSeed (MusicThingie::Type::Album))
                capabilities.push_back ("albumseed");
            if (playlist->canSeed (MusicThingie::Type::Artist))
                capabilities.push_back ("artistseed");
        }
    }
    if (isUsableBy (user) && canQueue() && isPrimary())
        capabilities.push_back ("request");
    caps (I_ACTIONS, std::move (capabilities), std::move (Response::NoJsonData));
    return caps;
}

Parsnip::Data PianodSong::serialize() const {
    // clang-format off
    Parsnip::Data song {Parsnip::Data::Dictionary,
        Music::Key::ArtistId, id (Type::Artist),
        Music::Key::ArtistName, artist(),
        Music::Key::AlbumId, id (Type::Album),
        Music::Key::AlbumName, albumTitle(),
        Music::Key::AlbumIsCompilation, compilation(),
        Music::Key::AlbumArtUrl, nullptr,
        Music::Key::SongId, id(),
        Music::Key::SongName, title(),
        Music::Key::SongGenre, nullptr,
        Music::Key::SongYear, nullptr,
        Music::Key::SongDuration, nullptr,
        Music::Key::SongInfoUrl, nullptr,
        Music::Key::PlaylistId, nullptr,
        Music::Key::PlaylistName, nullptr
    };
    // clang-format on
    serializeCommon (song);
    if (!coverArtUrl().empty())
        song [Music::Key::AlbumArtUrl] = coverArtUrl();
    PianodPlaylist *pl = playlist();
    if (pl) {
        song [Music::Key::PlaylistId] = pl->id();
        song [Music::Key::PlaylistName] = pl->playlistName();
    }
    if (!genre().empty())
        song [Music::Key::SongGenre] = genre();
    if (!infoUrl().empty())
        song [Music::Key::SongInfoUrl] = infoUrl();
    if (year())
        song [Music::Key::SongYear] = year();
    if (duration())
        song [Music::Key::SongDuration] = duration();
    return song;
}

void PianodSong::serializePrivate (Parsnip::Data &song, const User *user) const {
    song [Music::Key::SongRatings] = serializeRatings (user, playlist());
    song [Music::Key::Actions] = serializeCapabilities (user, playlist());
}

Parsnip::Data PianodSong::serializeRatings (const User *user, const PianodPlaylist *playlist) const {
    // Suggestions never have ratings.
    if (isSuggestion()) {
        return Parsnip::Data{};
    }

    // Primaries, seeds and ratings have some kind of rating.
    // (Except primaries don't have a rating for unauthenticated users.)
    // Announce seeds as unrated 0.0, so seed flags stay in place.
    Rating song_rating = Rating::UNRATED;
    if (ratingScheme() == RatingScheme::OWNER || (ratingScheme() == RatingScheme::INDIVIDUAL && user)) {
        song_rating = rating (user);
    }

    // clang-format off
    Parsnip::Data rating {Parsnip::Data::Dictionary,
        Music::Key::RatingNumeric, ratingAsFloat (song_rating),
        Music::Key::RatingName, RATINGS [song_rating],
        Music::Key::ArtistSeed, nullptr,
        Music::Key::AlbumSeed, nullptr,
        Music::Key::SongSeed, nullptr
    };
    // clang-format on
    if (playlist) {
        rating [Music::Key::ArtistSeed]
                = playlist->canSeed (MusicThingie::Type::Artist) && playlist->seed (MusicThingie::Type::Artist, this);
        rating [Music::Key::AlbumSeed]
                = playlist->canSeed (MusicThingie::Type::Album) && playlist->seed (MusicThingie::Type::Album, this);
        rating [Music::Key::SongSeed]
                = playlist->canSeed (MusicThingie::Type::Song) && playlist->seed (MusicThingie::Type::Song, this);
    };
    if (song_rating == Rating::UNRATED) {
        rating [Music::Key::RatingNumeric] = nullptr;
    }
    return rating;
}

/// Provide a list of actions a user can take on this song
Parsnip::Data PianodSong::serializeCapabilities (const User *user, const PianodPlaylist *playlist) const {
    // Suggestions never have ratings.
    if (isSuggestion()) {
        return Parsnip::Data{};
    }

    bool rating_capable = ((ratingScheme() == RatingScheme::INDIVIDUAL && user)
                           || (ratingScheme() == RatingScheme::OWNER && isEditableBy (user)));
    bool seeding_capable = (playlist && playlist->isEditableBy (user));

    // clang-format off
    return Parsnip::Data {Parsnip::Data::Dictionary,
        Music::Key::CanRequest, isUsableBy (user) && canQueue() && isPrimary(),
        Music::Key::CanRate, rating_capable,
        Music::Key::ArtistSeed, seeding_capable && playlist->canSeed (MusicThingie::Type::Artist),
        Music::Key::AlbumSeed, seeding_capable && playlist->canSeed (MusicThingie::Type::Album),
        Music::Key::SongSeed, seeding_capable && playlist->canSeed (MusicThingie::Type::Song)
    };
    // clang-format on
}

bool PianodSong::matches (const Filter &filter) const {
    return filter.matches (this);
}

bool PianodSong::operator== (const std::string &compare) const {
    return compare_titles (name(), compare);
}

bool PianodSong::operator== (const MusicThingie &compare) const {
    if (type() != compare.type())
        return false;

    auto other = dynamic_cast<const PianodSong *> (&compare);
    assert (other);
    if (!other)
        return false;
    return compare_titles (name(), other->name()) && compare_titles (albumTitle(), other->albumTitle())
           && compare_person_or_title (artist(), other->artist());
}

/** Return the average rating of a song, considering all ratings. */
float PianodSong::averageRating() const {
    if (ratingScheme() == RatingScheme::NOBODY)
        return ratingAsFloat (Rating::UNRATED);
    if (ratingScheme() == RatingScheme::OWNER)
        return ratingAsFloat (rating (nullptr));
    RatingAverager average;
    const UserList all_users{ user_manager->allUsers() };
    for (const auto user : all_users) {
        average.add (rating (user));
    }
    return (average());
}

/** Check for permission to skip a song.
    Some sources, such as Pandora, limit skipping for non-paying bastards.
    If skipping is permitted, it should be counted.
    @param whenAllowed On return, contains time at which skip will be allowed.
    @return True if skipping is allowed. */
bool PianodSong::canSkip (time_t *whenAllowed) {
    (void) whenAllowed;
    return true;
}

/** Mark songs (err, adverts) as must-play; they can't be skipped over
    even when playlist selections are changed or whatnot. */
bool PianodSong::mustPlay() const {
    return false;
}

/** Play the song. */
Media::Player *PianodSong::play (const AudioSettings &audio) {
    Media::Player *player = media_manager->getPlayer (audio, this);
    if (player) {
        last_played = time (nullptr);
    };
    return player;
};

SongList PianodSong::songs() {
    assert (canQueue());
    SongList songs;
    if (isPrimary())
        songs.push_back (this);
    return songs;
};

/** Provide a URL with additional info.
    @return URL or empty string if unknown. */
const std::string &PianodSong::infoUrl (void) const {
    const static std::string empty;
    return empty;
}

/** Get name of a playlist to which this belongs.
 @return Name, or an empty string. */
const std::string &PianodSong::playlistName (void) const {
    const static std::string empty;
    PianodPlaylist *pl = playlist();
    return pl ? pl->playlistName() : empty;
}

SongList PianodPlaylist::songs() {
    return (songs (Filter::All));
};

/** Get songs belonging to a playlist.
    @param filter Match only matsongs matching this.
    If omitted, match all.
    @return A list of matching songs from the playlist. */
SongList PianodPlaylist::songs (const Filter &filter) {
    assert (!"songs invoked on non-request playlist.");
    (void) filter;
    return SongList();
}

/** Choose some random songs for queueing.
    Because different sources do this differently, it can't be
    a baseline algorithm; thus it is up to the sources. */
SongList PianodPlaylist::getRandomSongs (const UserList &users, Media::SelectionMethod selectionMethod) {
    return source()->getRandomSongs (this, users, selectionMethod);
};

/*
 *                  Playlists
 */

const std::string PianodPlaylist::id (void) const {
    return std::to_string (source()->serialNumber()) + (char) type() + playlistId();
};

const std::string PianodPlaylist::id (MusicThingie::Type type) const {
    static std::string empty;
    assert (isPrimary (type));
    switch (type) {
        case MusicThingie::Type::Playlist:
            return id();
        default:
            return empty;
            ;
    }
};

const std::string &PianodPlaylist::internalId (MusicThingie::Type type) const {
    static std::string empty;
    assert (isPrimary (type));
    switch (type) {
        case MusicThingie::Type::Playlist:
            return playlistId();
        default:
            return empty;
            ;
    }
};

Football::Thingie &PianodPlaylist::transmitCommon (Football::Thingie &recipient) const {
    recipient << Response (I_ID, id()) << Response (I_PLAYLIST, playlistName());
    if (!genre().empty()) {
        recipient << Response (I_GENRE, genre());
    }
    recipient << Response (I_SOURCE, source()->kind()) << Response (I_NAME, source()->name());
    return recipient;
}

PianodConnection &PianodPlaylist::transmitPrivate (PianodConnection &recipient) const {
    if (recipient.user) {
        recipient << assembleRatings (recipient.user);
    }

    // Provide the list of actions the user can take on this playlist
    Response::List actions;
    if (recipient.user) {
        actions.push_back ("rate");
    }
    if (isUsableBy (recipient.user) && canQueue() && isPrimary())
        actions.push_back ("request");
    if (isEditableBy (recipient.user)) {
        // Include seed flags if they apply.
        if (canSeed (MusicThingie::Type::Song))
            actions.push_back ("seed");
        if (canSeed (MusicThingie::Type::Album))
            actions.push_back ("albumseed");
        if (canSeed (MusicThingie::Type::Artist))
            actions.push_back ("artistseed");
    }
    if (!actions.empty()) {
        recipient << Response (I_ACTIONS, std::move (actions), std::move (Response::NoJsonData));
    }
    return recipient;
}

Response PianodPlaylist::assembleRatings (const User *user) const {
    // If this song can have a playlist rating, announce that.
    Rating playlist_rating = rating (user);
    std::ostringstream rating;
    rating << std::fixed << std::setprecision (1) << ratingAsFloat (playlist_rating);
    Response::List rating_pieces = { RATINGS [playlist_rating], rating.str() };
    return Response (I_PLAYLISTRATING, std::move (rating_pieces), std::move (Response::NoJsonData));
}

Parsnip::Data PianodPlaylist::serialize() const {
    // clang-format off
    Parsnip::Data playlist {Parsnip::Data::Dictionary,
        Music::Key::PlaylistId, id(),
        Music::Key::PlaylistName, playlistName(),
        Music::Key::SongGenre, genre()
    };
    // clang-format on
    serializeCommon (playlist);
    return playlist;
}

void PianodPlaylist::serializePrivate (Parsnip::Data &playlist, const User *user) const {
    // clang-format off
    playlist [Music::Key::Actions] = Parsnip::Data {Parsnip::Data::Dictionary,
        Music::Key::CanRequest, isUsableBy (user),
        Music::Key::CanRate, user != nullptr,
        Music::Key::ArtistSeed, isEditableBy (user) && canSeed (MusicThingie::Type::Artist),
        Music::Key::AlbumSeed, isEditableBy (user) && canSeed (MusicThingie::Type::Album),
        Music::Key::SongSeed, isEditableBy (user) && canSeed (MusicThingie::Type::Song)
    };
    playlist  [Music::Key::PlaylistRatings] = serializeRatings (user);
    // clang-format on
}

Parsnip::Data PianodPlaylist::serializeRatings (const User *user) const {
    // If this song can have a playlist rating, announce that.
    Rating playlist_rating = rating (user);
    // clang-format off
    Parsnip::Data ratings {Parsnip::Data::Dictionary,
        Music::Key::RatingNumeric, ratingAsFloat (playlist_rating),
        Music::Key::RatingName, RATINGS [playlist_rating]
    };
    // clang-format on
    if (playlist_rating == Rating::UNRATED) {
        ratings [Music::Key::RatingNumeric] = nullptr;
    }
    return ratings;
}

bool PianodPlaylist::matches (const Filter &filter) const {
    return filter.matches (this);
}

bool PianodPlaylist::operator== (const std::string &compare) const {
    return compare_titles (playlistName(), compare);
}

bool PianodPlaylist::operator== (const MusicThingie &compare) const {
    if (type() != compare.type())
        return false;

    auto other = dynamic_cast<const PianodPlaylist *> (&compare);
    assert (other);
    if (!other)
        return false;
    return compare_titles (playlistName(), other->playlistName());
}

/** Set a user's playlist's rating.  This is handled internally by pianod.
    @param value The rating to assign.
    @param user The user rating the playlist.
    @return A status code. */
RESPONSE_CODE PianodPlaylist::rate (Rating value, User *user) {
    if (!user)
        throw CommandError (E_LOGINREQUIRED);
    if (playlistType() != PlaylistType::SINGLE)
        throw CommandError (E_WRONGTYPE);
    UserData::Ratings *ratings = UserData::Ratings::retrieve (user, UserData::Key::PlaylistRatings, source()->key());
    if (!ratings) {
        throw CommandError (E_RESOURCE);
    }
    try {
        (*ratings) [playlistId()] = value;
        user->updateData();
        return S_OK;
    } catch (const std::bad_alloc &) {
        throw CommandError (E_RESOURCE);
    }
};

/** Retrieve a user's playlist's rating.
 @param user The user to get the rating for.
 @return A rating code, or Rating::UNRATED. */
Rating PianodPlaylist::rating (const User *user) const {
    if (!user)
        return Rating::UNRATED;
    UserData::Ratings *ratings = UserData::Ratings::retrieve (user, UserData::Key::PlaylistRatings, source()->key());
    if (!ratings)
        return Rating::UNRATED;
    return ratings->get (playlistId(), Rating::UNRATED);
};

/** Return the average rating of a playlist, considering all ratings. */
float PianodPlaylist::averageRating() const {
    RatingAverager average;
    const UserList all_users{ user_manager->allUsers() };
    for (const auto user : all_users) {
        average.add (rating (user));
    }
    return (average());
    ;
}

/** Determine if a particular type of seeding is possible.
    @param seedType The type of seeding to check for.
    @return True if that kind of seeding is allowed. */
bool PianodPlaylist::canSeed (MusicThingie::Type seedType) const {
    (void) seedType;
    return false;
}

/** Check if there is a seed of a particular type for this thingie.
    @param seedType Indicates artist, album or song seed.
    @param music The artist, album or song to use as a seed. */
bool PianodPlaylist::seed (MusicThingie::Type seedType, const MusicThingie *music) const {
    assert (0);
    (void) seedType;
    (void) music;
    return false;
}

/** Make a particular seed type for this thingie.
    @param seedType Indicates artist, album or song seed.
    @param music The artist, album or song to use as a seed.
    @param value Whether to set or remove a seed. */
void PianodPlaylist::seed (MusicThingie::Type seedType, MusicThingie *music, bool value) {
    assert (0);
    (void) seedType;
    (void) music;
    (void) value;
    throw CommandError (E_UNSUPPORTED);
}

/** Get the seed list for a playlist. */
ThingieList PianodPlaylist::getSeeds (void) const {
    assert (!"getSeeds not implemented.");
    return ThingieList();
}

/** Update a playlist's selector algorithm.
    @param new_selector The new algorithm. */
void PianodPlaylist::updateSelector (const Filter &new_selector) {
    throw CommandError (E_UNSUPPORTED);
}
