///
/// Playlist / Artist / Album / Song datatypes that are self-implemented.
/// Each datatype / data item exists independent of the others; there
/// are no references to eachother.  (Contrast with songlibrary.)
/// @file       encapmusic.cpp - pianod
/// @author     Perette Barella
/// @date       2014-12-09
/// @copyright  Copyright 2014-2021 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <cassert>

#include <string>

#include <parsnip/parsnip.h>

#include "encapmusic.h"
#include "mediaunit.h"
#include "musickeys.h"

/*
 *              Artists
 */

/** Reconstruct a song from our persistence format. */
PersistentArtist::PersistentArtist (Media::Source * const owner, Type type, const Parsnip::Data &message)
: EncapsulatedArtist (owner, type) {
    artistId (message [Music::Key::ArtistId].asString());
    artist (message [Music::Key::ArtistName].asString());
}

/** Assemble the song details for serialization. */
Parsnip::Data PersistentArtist::persist() const {
    // clang-format off
    return Parsnip::Data { Parsnip::Data::Dictionary,
        Music::Key::ArtistId, artistId(),
        Music::Key::ArtistName, artist(),
    };
    // clang-format on
}

/*
 *              Albums
 */

/** Reconstruct a song from our persistence format. */
PersistentAlbum::PersistentAlbum (Media::Source * const owner, Type type, const Parsnip::Data &message)
: EncapsulatedAlbum (owner, type) {
    artistId (message [Music::Key::ArtistId].asString());
    artist (message [Music::Key::ArtistName].asString());
    albumTitle (message [Music::Key::AlbumName].asString());
    albumId (message [Music::Key::AlbumId].asString());
    coverArtUrl (message [Music::Key::AlbumArtUrl].asString());
}

/** Assemble the song details for serialization. */
Parsnip::Data PersistentAlbum::persist() const {
    // clang-format off
    return Parsnip::Data { Parsnip::Data::Dictionary,
        Music::Key::ArtistId, artistId(),
        Music::Key::ArtistName, artist(),
        Music::Key::AlbumId, albumId(),
        Music::Key::AlbumName, albumTitle(),
        Music::Key::AlbumArtUrl, coverArtUrl()
    };
    // clang-format on
}

/*
 *              Songs
 */

/** Reconstruct a song from our persistence format. */
PersistentSong::PersistentSong (Media::Source * const owner, Type type, const Parsnip::Data &message)
: EncapsulatedSong (owner, type) {
    artistId (message [Music::Key::ArtistId].asString());
    artist (message [Music::Key::ArtistName].asString());
    albumTitle (message [Music::Key::AlbumName].asString());
    albumId (message [Music::Key::AlbumId].asString());
    title (message [Music::Key::SongName].asString());
    songId (message [Music::Key::SongId].asString());
    coverArtUrl (message [Music::Key::AlbumArtUrl].asString());
    duration (message [Music::Key::SongDuration].asInteger());
    infoUrl (message [Music::Key::SongInfoUrl].asString());
    genre (message [Music::Key::SongGenre].asString());
    trackNumber (message [Music::Key::SongTrackNumber].asInteger());
}

/** Assemble the song details for serialization. */
Parsnip::Data PersistentSong::persist() const {
    // clang-format off
    Parsnip::Data song{ Parsnip::Data::Dictionary,
        Music::Key::ArtistId, artistId(),
        Music::Key::ArtistName, artist(),
        Music::Key::AlbumId, albumId(),
        Music::Key::AlbumName, albumTitle(),
        Music::Key::SongId, songId(),
        Music::Key::SongName, title(),
        Music::Key::SongDuration, duration(),
        Music::Key::SongInfoUrl, infoUrl(),
        Music::Key::SongTrackNumber, trackNumber(),
        Music::Key::SongGenre, genre(),
        Music::Key::AlbumArtUrl, coverArtUrl()
    };
    // clang-format on
    if (playlist()) {
        song [Music::Key::PlaylistId] = playlist()->playlistId();
    }
    return song;
}

void EncapsulatedSong::playlist (PianodPlaylist *play) {
    _playlist = play;
};

/*
 *              Playlists
 */

static void playlist_restore (EncapsulatedPlaylist &target, const Parsnip::Data &message) {
    target.playlistId (message [Music::Key::PlaylistId].asString());
    target.playlistName (message [Music::Key::PlaylistName].asString());
    target.genre (message [Music::Key::PlaylistGenre].asString());
}

static Parsnip::Data playlist_persist (const EncapsulatedPlaylist &from) {
    // clang-format off
    return Parsnip::Data { Parsnip::Data::Dictionary,
        Music::Key::PlaylistId, from.playlistId(),
        Music::Key::PlaylistName, from.playlistName(),
        Music::Key::PlaylistGenre, from.genre()
    };
    // clang-format on
}


/** Reconstruct a song from our persistence format. */
PersistentPlaylist::PersistentPlaylist (Media::Source * const owner, Type type, const Parsnip::Data &message)
: EncapsulatedPlaylist (owner, type) {
    playlist_restore (*this, message);
}

/** Assemble the song details for serialization. */
Parsnip::Data PersistentPlaylist::persist() const {
    return playlist_persist (*this);
}

/*
 *              Meta Playlists
 */

MetaPlaylist::MetaPlaylist (Media::Source * const source, PianodPlaylist::PlaylistType type)
: EncapsulatedPlaylist (source) {
    assert (type == PianodPlaylist::MIX || type == PianodPlaylist::EVERYTHING || type == PianodPlaylist::TRANSIENT);
    playlistType (type);
    const char *word = (type == PianodPlaylist::MIX          ? "mix"
                        : type == PianodPlaylist::EVERYTHING ? "bibliotheque"
                                                             : "transient");
    playlistName (std::string (source->kind()) + " " + source->name() + " " + word);
    playlistId (word);
}

bool MetaPlaylist::includedInMix (void) const {
    assert (!"Metaplaylist mix inclusion requested");
    return false;
};

void MetaPlaylist::includedInMix (bool) {
    assert (!"Metaplaylist mix inclusion changed");
};

void MetaPlaylist::rename (const std::string &) {
    assert (!"Metaplaylist rename attempted");
    throw CommandError (E_BUG);
}
void MetaPlaylist::erase() {
    assert (!"Metaplaylist delete attempted");
    throw CommandError (E_BUG);
}

/** Reconstruct a song from our persistence format. */
PersistentMetaPlaylist::PersistentMetaPlaylist (Media::Source * const owner, Type type, const Parsnip::Data &message)
: MetaPlaylist (owner, type) {
    playlist_restore (*this, message);
}

/** Assemble the song details for serialization. */
Parsnip::Data PersistentMetaPlaylist::persist() const {
    return playlist_persist (*this);
}
