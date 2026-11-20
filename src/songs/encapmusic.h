///
/// Playlist / Artist / Album / Song datatypes that are self-implemented.
/// Each datatype / data item exists independent of the others; there
/// are no references to eachother.  (Contrast with songlibrary.)
/// @file       encapmusic.h - pianod
/// @author     Perette Barella
/// @date       2014-12-09
/// @copyright  Copyright (c) 2014-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>

#include "musictypes.h"
#include "retainer.h"

/** A PianodArtist that is fully-contained in itself, unrelated to
    albums, songs, or playlists. */
class EncapsulatedArtist : public PianodArtist {
private:
    Media::Source * const _source;
    const Type _type;
    std::string _artistId;
    std::string _artist;
public:
    inline EncapsulatedArtist (Media::Source * const owner,
                               Type type = Type::Artist)
    : _source (owner), _type (type) { };

    virtual Type type (void) const override { return _type; };
    virtual Media::Source * const source (void) const override { return _source; };

    virtual const std::string &artistId (void) const override { return _artistId; };
    virtual const std::string &artist (void) const override { return _artist; };

    inline void artistId (const std::string &a) { _artistId = a; };
    inline void artistId (const char *a) { if (a) _artistId = a; };
    inline void artist (const std::string &a) { _artist = a; };
    inline void artist (const char *a) { if (a) _artist = a; };
};

/** A PianodArtist that is fully-contained in itself, unrelated to
    albums, songs, or playlists. */
class PersistentArtist : public EncapsulatedArtist {
public:
    using EncapsulatedArtist::EncapsulatedArtist;
    PersistentArtist (Media::Source * const owner,
                        Type type,
                        const Parsnip::Data &data);
    virtual Parsnip::Data persist() const;
};


/// A PianodAlbum that is self-contained, unrelated to artists, songs, or playlists
/// Encapsulated albums may not be needed; may not occur with services
class EncapsulatedAlbum : public PianodAlbum {
private:
    Media::Source * const _source;
    const Type _type;
    std::string _artistId;
    std::string _artist;

    std::string _albumId;
    std::string _albumTitle;
    std::string _coverArt;
public:
    inline EncapsulatedAlbum (Media::Source * const owner,
                       Type item_type = Type::Album)
    : _source (owner), _type (item_type) { };

    virtual Type type (void) const override { return _type; };
    virtual Media::Source * const source (void) const override { return _source; };

    virtual const std::string &artistId (void) const override { return _artistId; };
    virtual const std::string &artist (void) const override { return _artist; };

    virtual const std::string &albumId (void) const override { return _albumId; };
    virtual const std::string &albumTitle (void) const override { return _albumTitle; };
    virtual const std::string &coverArtUrl (void) const override { return _coverArt; };

    inline void artistId (const std::string &a) { _artistId = a; };
    inline void artistId (const char *a) { if (a) _artistId = a; };
    inline void artist (const std::string &a) { _artist = a; };
    inline void artist (const char *a) { if (a) _artist = a; };

    inline void albumId (const std::string &a) { _albumId = a; };
    inline void albumId (const char *a) { if (a) _albumId = a; };
    inline void albumTitle (const std::string &t) { _albumTitle = t; };
    inline void albumTitle (const char *t) { if (t) _albumTitle = t; };
    inline void coverArtUrl (const std::string &u) { _coverArt = u; };
    inline void coverArtUrl (const char *u) { if (u) _coverArt = u; };
};

/// A PianodAlbum that is self-contained, unrelated to artists, songs, or playlists
/// Encapsulated albums may not be needed; may not occur with services
class PersistentAlbum : public EncapsulatedAlbum {
public:
    using EncapsulatedAlbum::EncapsulatedAlbum;
    PersistentAlbum (Media::Source * const owner,
                       Type type,
                       const Parsnip::Data &data);
    virtual Parsnip::Data persist() const;

};

/// A PianodSong that is self-contained, unrelated to albums or artists.
class EncapsulatedSong : public PianodSong {
private:
    Media::Source * const _source;
    const Type _type;
    std::string _artistId;
    std::string _artist;

    std::string _albumId;
    std::string _albumTitle;
    std::string _coverArt;

    std::string _songId;
    std::string _title;
    std::string _genre;
    std::string _infoUrl;
    int _duration = 0;
    int _year = 0;
    int _trackNumber = 0;
    Retainer <PianodPlaylist *> _playlist;

public:
    inline EncapsulatedSong (Media::Source * const owner, Type type = Type::Song)
    : _source (owner), _type (type) { };

    virtual Type type (void) const override { return _type; };
    virtual Media::Source * const source (void) const override { return _source; };

    virtual const std::string &songId (void) const override { return _songId; };

    virtual const std::string &artistId (void) const override { return _artistId; };
    virtual const std::string &artist (void) const override { return _artist; };

    virtual const std::string &albumId (void) const override { return _albumId; };
    virtual const std::string &albumTitle (void) const override { return _albumTitle; };
    virtual const std::string &coverArtUrl (void) const override { return _coverArt; };

    virtual const std::string &title (void) const override { return _title; };
    virtual const std::string &genre (void) const override { return _genre; };
    virtual const std::string &infoUrl (void) const override { return _infoUrl; };
    virtual int duration (void) const override { return _duration; };
    virtual int year (void) const override { return _year; };
    virtual time_t lastPlayed (void) const { return 0; };
    virtual int trackNumber (void) const override { return _trackNumber; };
    virtual PianodPlaylist *playlist (void) const override { return _playlist.get(); };

    void playlist (PianodPlaylist *play);

    inline void artistId (const std::string &a) { _artistId = a; };
    inline void artistId (const char *a) { if (a) _artistId = a; };
    inline void artist (const std::string &a) { _artist = a; };
    inline void artist (const char *a) { if (a) _artist = a; };

    inline void albumId (const std::string &a) { _albumId = a; };
    inline void albumId (const char *a) { if (a) _albumId = a; };
    inline void albumTitle (const std::string &t) { _albumTitle = t; };
    inline void albumTitle (const char *t) { if (t) _albumTitle = t; };
    inline void coverArtUrl (const std::string &u) { _coverArt = u; };
    inline void coverArtUrl (const char *u) { if (u) _coverArt = u; };

    inline void songId (const std::string &i) { _songId = i; };
    inline void songId (const char *i) { if (i) _songId = i; };
    inline void title (const std::string &t) { _title = t; };
    inline void title (const char *t) { if (t) _title = t; };
    inline void genre (const std::string &g) { _genre = g; };
    inline void genre (const char *g) { if (g) _genre = g; };
    inline void infoUrl (const std::string &i) { _infoUrl = i; };
    inline void infoUrl (const char *i) { if (i) _infoUrl = i; };
    inline void duration (int d) { _duration = d; };
    inline void year (int y) { _year = y; };
    inline void trackNumber (int n) { _trackNumber = n; };

    virtual RESPONSE_CODE rate (Rating , User *) override {
        throw CommandError (E_INVALID);
    };
    virtual Rating rating (const User *) const override {
        return Rating::UNRATED;
    }

};

/// A PianodSong that is self-contained, unrelated to albums or artists.
class PersistentSong : public EncapsulatedSong {
public:
    using EncapsulatedSong::EncapsulatedSong;
    PersistentSong (Media::Source * const owner,
                    Type type,
                    const Parsnip::Data &data);
    virtual Parsnip::Data persist() const;
};

/// A PianodPlaylist that is self-contained.
class EncapsulatedPlaylist : public PianodPlaylist {
private:
    Media::Source * const _source;
    const Type _type;
    PlaylistType _playlistType = SINGLE;
    std::string _playlistId;
    std::string _playlistName;
    std::string _genre;
public:
    EncapsulatedPlaylist (Media::Source * const owner,
                          Type type = Type::Playlist)
    : _source (owner), _type (type) { };
    virtual Type type (void) const override { return _type; };
    virtual Media::Source * const source (void) const override { return _source; };

    virtual PlaylistType playlistType (void) const override { return _playlistType; };
    virtual const std::string &playlistId (void) const override { return _playlistId; };
    virtual const std::string &playlistName (void) const override { return _playlistName; };
    virtual const std::string &genre (void) const override { return _genre; };


    inline void playlistType (PlaylistType p) { _playlistType = p; };
    inline void playlistId (const std::string &p) { _playlistId = p; };
    inline void playlistId (const char *p) { if (p) _playlistId = p; };
    inline void playlistName (const std::string &n) { _playlistName = n; };
    inline void playlistName (const char * &n) { if (n) _playlistName = n; };
    inline void genre (const std::string &g) { _genre = g; };
    inline void genre (const char * &g) { if (g) _genre = g; };
};

class PersistentPlaylist : public EncapsulatedPlaylist {
public:
    using EncapsulatedPlaylist::EncapsulatedPlaylist;
    PersistentPlaylist (Media::Source * const owner,
                        Type type,
                        const Parsnip::Data &data);
    virtual Parsnip::Data persist() const;
};

/// A faux playlist suitable for mix or everything playlists.
class MetaPlaylist : public EncapsulatedPlaylist {
    virtual bool includedInMix (void) const override;
    virtual void includedInMix (bool include) override;
    virtual void rename (const std::string &newname) override;
    virtual void erase () override;
public:
    using EncapsulatedPlaylist::EncapsulatedPlaylist;
    MetaPlaylist (Media::Source * const owner,
                  PianodPlaylist::PlaylistType type);
};


class PersistentMetaPlaylist : public MetaPlaylist {
public:
    using MetaPlaylist::MetaPlaylist;
    PersistentMetaPlaylist (Media::Source * const owner,
                            Type type,
                            const Parsnip::Data &data);
    virtual Parsnip::Data persist() const;
};
