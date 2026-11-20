///
/// PlayList / Artist / Album / Song types that are interbred
/// to form a library.
/// @file       musiclibrary.h - pianod
/// @author     Perette Barella
/// @date       2014-12-09
/// @copyright  Copyright (c) 2014-2023 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

#include <parsnip/parsnip.h>

#include "musictypes.h"
#include "retainedlist.h"
#include "musickeys.h"
#include "filter.h"

/// Memory-based index/database of music library contents.
namespace MusicLibrary {
    class Playlist;
    class Song;

    const static int BIAS_MINIMUM = 1;
    const static int BIAS_MAXIMUM = 100;
    const static int BIAS_NEUTRAL = 1;  ///< Neutral biasing factor for choosing songs

    class Foundation {
    private:
        const bool persist_data {false};
        mutable time_t write_time {0};

    protected:
        virtual bool restoreIndexFromFile (const std::string &filename) = 0;

        virtual bool writeIndexToFile (const std::string &filename) const = 0;
        virtual void persist (Parsnip::Data &into) const { };
        virtual bool restore (const Parsnip::Data &data) { return true; };
    public:
        Media::Source * const source;
        Foundation (Media::Source * const owner, const bool persistence);
        typedef enum {
            IMPORTANT = 300,
            NOMINAL = 1800,
            TRIVIAL = 60 * 60 * 6
        } IMPORTANCE;
        inline void markDirty (IMPORTANCE import = TRIVIAL) const {
            time_t when = time (nullptr) + import;
            if (write_time == 0 || when < write_time) {
                write_time = when;
            }
        }
        bool load();
        bool flush();
        float periodic();

        virtual RetainedList <Song *> getAllSongs (void) = 0;
        virtual RetainedList <Song *> getMatchingSongs (const Filter &criteria) = 0;
        virtual bool removePlaylist (Playlist *play) = 0;
        virtual ThingieList seedsForPlaylist (const Playlist *playlist) = 0;
        virtual void populatePlaylist (Playlist *play, bool aggressive = false) = 0;
        virtual RetainedList<Song *> getSongsForPlaylist (PianodPlaylist *) = 0;
        virtual RetainedList <Song *> getPlaylistSongs (const Playlist *play, bool reassess = false) = 0;
        RetainedList <Song *> getRandomSongs (PianodPlaylist *playlist,
                                 const UserList &users,
                                 Media::SelectionMethod selectionMethod,
                                 const class LibraryParameters &settings);
    };


    /// A PianodPlaylist for music libraries.
    class Playlist : public PianodPlaylist {
        friend class TransientPlaylist;
    private:
        Foundation *const _library;
        const std::string _id;
        std::string _name;
        mutable bool genres_dirty {true};
        mutable std::string _genres;

        void calculateGenres() const;
    public:
        typedef std::unordered_set<std::string> SeedSet;
        SeedSet seeds;
        bool enabled = true;
        Filter selector; ///< First selection of music from the library
    public:
        Playlist (Foundation *const library, const std::string &id, const std::string &name)
        : _library (library), _id (id), _name (name) { };
        // Default constructor is not used, but necessary for TransientPlaylist to compile (note virtual base class).
        Playlist (): Playlist (nullptr, "", "") { assert (!"Default constructor called for MusicLibrary::Playlist"); };
        inline Foundation *const library (void) const { return _library; };
        inline Foundation *const parent (void) const { return _library; };
        virtual Media::Source * const source (void) const override final { return _library->source; };
        virtual bool includedInMix (void) const override { return enabled; };
        virtual void includedInMix (bool include) override { enabled = include; };

        virtual PlaylistType playlistType (void) const override { return SINGLE; };
        virtual const std::string &playlistId (void) const override { return _id; };
        virtual const std::string &playlistName (void) const override { return _name; };
        virtual const std::string &genre (void) const override;

        virtual bool canSeed (MusicThingie::Type seedType) const override;
        virtual bool seed (MusicThingie::Type seedType, const MusicThingie *music) const override;
        virtual ThingieList getSeeds (void) const override;
        virtual void seed (MusicThingie::Type seedType, MusicThingie *music,
                                    bool value) override;
        virtual SongList songs () override;
        virtual SongList songs (const Filter &filter) override;
        virtual bool canQueue() const override final { return true; };
        virtual void updateSelector (const Filter &new_selector) override final;
        virtual void rename (const std::string &newname) override;
        virtual void erase () override;

        bool appliesTo (const PianodSong *song) const;
        const std::string *getIdForSeed (MusicThingie::Type seedType,
                                         const MusicThingie *music) const;
        void invalidateSeeds (const MusicThingie *music);
        virtual Parsnip::Data persist () const;
        virtual void restore (const Parsnip::Data &data);

        virtual PianodConnection &transmitPrivate (PianodConnection &recipient) const override;
        virtual void serializePrivate (Parsnip::Data &song, const User *user) const override;
    };

    class TransientPlaylist : public PianodTransientPlaylist <Playlist> {
    public:
        TransientPlaylist (Foundation *const library, const Filter &criteria);
        virtual SongList songs () override;
    };

    class Album;
    class Song;

    /// A PianodArtist that contains a vector of albums.
    class Artist : public PianodArtist {
        friend class Album;
        friend class Song;
        friend class Playlist;
    private:
        Foundation *const _library;
        const std::string _id;
        const std::string _name;
        std::vector<Album *> albums;
        std::vector<Song *> compilation_songs;
    public:
        Artist (Foundation *const library, const std::string &id, const std::string &name);
        virtual ~Artist (void) override;
        inline Foundation *const parent (void) const { return _library; };
        inline Foundation *const library (void) const { return _library; };
        virtual Media::Source * const source (void) const override final { return _library->source; };

        virtual const std::string &artistId (void) const override final { return _id; };
        virtual const std::string &artist (void) const override final { return _name; };

        virtual SongList songs () override;
        virtual bool canQueue() const override { return true; };
        virtual Parsnip::Data persist () const;
        virtual void restore (const Parsnip::Data &data);
        inline bool empty() const { return albums.empty(); };
        const std::vector<Album *> &getAlbums() { return albums; };
    };

    /// A PianodAlbum that contains a vector of songs
    class Album : public PianodAlbum {
        friend class Artist;
        friend class Song;
        friend class Playlist;
        friend RetainedList <Song *> Foundation::getRandomSongs (PianodPlaylist *playlist,
                                                    const UserList &users,
                                                    Media::SelectionMethod selectionMethod,
                                                    const class LibraryParameters &settings);
    protected:
        Artist * _artist;
    private:
        const std::string _id;
        const std::string _name;
        std::vector<Song *> _songs;
        std::string _coverArt;
        mutable std::string _artists;
        mutable bool artists_dirty = false;
    public:
        Album (Artist *const parent, const std::string &id, const std::string &name);
        virtual ~Album (void) override;
        inline Artist *const parent (void) const { return _artist; };
        inline Foundation *const library (void) const { return _artist->_library; };
        virtual Media::Source * const source (void) const override final { return _artist ->_library->source; };

        virtual const std::string &artistId (void) const override final { return _artist->_id; };
        virtual const std::string &artist (void) const override final;

        virtual const std::string &albumId (void) const override final { return _id; };
        virtual const std::string &albumTitle (void) const override final { return _name; };
        virtual const std::string &coverArtUrl (void) const override final { return _coverArt; };
        virtual bool compilation () const override;

        inline void coverArtUrl (const std::string &a) { _coverArt = a; };
        void calculateArtists (void) const;
        virtual void makeCompilation (Artist *compilation_artist);

        virtual SongList songs () override;
        virtual bool canQueue() const override { return true; };
        virtual Parsnip::Data persist () const;
        virtual void restore (const Parsnip::Data &data);
        inline bool empty() const { return _songs.empty(); };
        const std::vector<Song *> &getSongs() { return _songs; };
    };

    /// A PianodSong made of inbred data structures.
    class Song : public PianodSong {
        friend class Playlist;
        friend class Album;
        friend RetainedList <Song *> Foundation::getRandomSongs (PianodPlaylist *playlist,
                                                    const UserList &users,
                                                    Media::SelectionMethod selectionMethod,
                                                    const class LibraryParameters &settings);
    protected:
        Album *const _album;
    private:
        const std::string _id;
        const std::string _name;

        Artist *_artist = nullptr;
        std::string _genre;
        int _duration = 0;
        int _year = 0;
        int _trackNumber = 0;
public: // For now
        Playlist *_playlist = nullptr;

    public:
        Song (Album *const parent, const std::string &id, const std::string &name);
        virtual ~Song (void) override;
        inline Album *const parent (void) const { return _album; };
        inline Foundation *const library (void) const { return _album->_artist->_library; };
        virtual Media::Source * const source (void) const override final { return _album->_artist->_library->source; };
        virtual const std::string &songId (void) const override { return _id; };

        virtual const std::string &artistId (void) const override final {
            return _artist ? _artist->_id : _album->_artist->_id;
        };
        virtual const std::string &artist (void) const override final;
        virtual PianodArtist *asArtist () override final { if (_artist) return _artist; else return this; };
        virtual const PianodArtist *asArtist () const override final { if (_artist) return _artist; else return this; };

        virtual const std::string &albumId (void) const override final { return _album->_id; };
        virtual const std::string &albumTitle (void) const override final { return _album->_name; };
        virtual const std::string &coverArtUrl (void) const override final { return _album->_coverArt; };
        virtual bool compilation () const override final { return _album->compilation(); };

        virtual const std::string &title (void) const override final { return _name; };

        virtual PianodPlaylist *playlist (void) const override final;
        virtual const std::string &genre (void) const override final { return _genre; }
        virtual int duration (void) const override final { return _duration; };
        virtual int year (void) const override final { return _year; };
        virtual int trackNumber (void) const override final { return _trackNumber; };
        inline int albumTrackCount () const { return _album->_songs.size(); };

        void artist (Artist *artist);
        inline void genre (const std::string &g) { _genre = g; };
        inline void duration (int d) { _duration = d; };
        inline void year (int y) { _year = y; };
        inline void trackNumber (int n) { _trackNumber = n; };

        virtual Parsnip::Data persist () const;
        virtual void restore (const Parsnip::Data &data);

        virtual bool canQueue() const override { return true; };

        // Content management
        virtual RatingScheme ratingScheme (void) const override { return RatingScheme::INDIVIDUAL; };
        virtual RESPONSE_CODE rate (Rating value, User *user) override;
        virtual Rating rating (const User *user) const override;
        virtual RESPONSE_CODE rateOverplayed (User *user) override;
    };

    /** Customized hash tables for music library.
        These add persist/restore capabilities, and provide for items
        that are cross-linked with parent objects. */
    template <class TThing, class TParent>
    class ThingieContainer : public std::unordered_map<std::string, TThing *> {
    public:
        using Allocator = std::function <TThing * (TParent *const, const std::string &, const std::string &)>;
        
    private:
        /// An allocator for making new items of derived types we could store.
        const Allocator allocate;

    public:
        ThingieContainer (const Allocator &alloc);
        ~ThingieContainer();

        void clear();
        void purge (bool pred (const TThing *));

        TThing *getById (const std::string &key) const;
        TThing *getById (const Parsnip::Data &data, const char *field);

        /** Search the things looking for a name and parent match. */
        TThing *getByName (const std::string &name, TParent *parent) const;
        std::string getNewId (MusicThingie::Type item_type) const;
        TThing *addItem (const std::string &name, std::string id, TParent *parent);
        TThing *addOrGetItem (const std::string &name, std::string id, TParent *parent);
        inline TThing *addOrGetItem (const std::string &name, TParent *parent) {
            return addOrGetItem (name, "", parent);
        }
        TThing *addOrGetItem (const Parsnip::Data &data,
                              TParent *parent,
                              const std::string &namefield,
                              const std::string &idfield);
    };

    static inline bool isCompilationAlbum (const Parsnip::Data &album) {
        return (album.contains (Music::Key::AlbumIsCompilation)
                && album[Music::Key::AlbumIsCompilation].asBoolean());
    }

    template <typename AllocType, typename ParentType>
    class Allocator {
    public:
        AllocType *operator () (ParentType *const parent, const std::string &id, const std::string &name) {
            return new AllocType (parent, id, name);
        }
    };
    
    extern Allocator <Artist, Foundation> artist_allocate;
    extern Allocator <Album, Artist> album_allocate;
    extern Allocator <Song, Album> song_allocate;
    extern Allocator <Playlist, Foundation> playlist_allocate;
    
    class Library : public Foundation {
        using ArtistContainer = ThingieContainer <Artist, Foundation>;
        using AlbumContainer = ThingieContainer <Album, Artist>;
        using SongContainer = ThingieContainer <Song, Album>;
        using PlaylistContainer = ThingieContainer <Playlist, Foundation>;
    public:
        using ArtistAllocator = ArtistContainer::Allocator;
        using AlbumAllocator = AlbumContainer::Allocator;
        using SongAllocator = SongContainer::Allocator;
        using PlaylistAllocator = PlaylistContainer::Allocator;

    protected:
        ArtistContainer artists;
        AlbumContainer albums;
        SongContainer songs;
        PlaylistContainer playlists;

        Library (Media::Source * const owner,
                 const bool persistence,
                 const SongAllocator &song_allocator = SongAllocator {song_allocate},
                 const AlbumAllocator &album_allocator = AlbumAllocator {album_allocate},
                 const ArtistAllocator &artist_allocator = ArtistAllocator {artist_allocate},
                 const PlaylistAllocator &playlist_allocator = PlaylistAllocator {playlist_allocate});

        void purge (void);

    public:
        virtual bool removePlaylist (Playlist *play) override;
        virtual ThingieList seedsForPlaylist (const Playlist *playlist) override;
        virtual RetainedList <Song *> getAllSongs (void) override;
        virtual RetainedList <Song *> getMatchingSongs (const Filter &criteria) override;
        ThingieList getSuggestions (const Filter &criteria, SearchRange what);
        RetainedList <Song *> getMixSongs (void);
        virtual RetainedList <Song *> getPlaylistSongs (const Playlist *play, bool reassess = false) override;
        Playlist *findPlaylistForSong (Song *song, bool enabled = true);
        virtual void populatePlaylist (Playlist *play, bool aggressive = false) override;
        void unpopulatePlaylist (Playlist *play);
        /** Iterate over every song and replace its playlist assignment.
            Applicable on initialization. */
        void mixRecalculate (void) {
            for (auto item : songs) {
                item.second->_playlist = findPlaylistForSong (item.second);
            }
        }
        MusicThingie *getById (MusicThingie::Type type, const std::string &id);
        virtual RetainedList <Song *> getSongsForPlaylist (PianodPlaylist *playlist) override;
        PianodPlaylist *createPlaylist (const std::string &name,
                                        MusicThingie::Type type,
                                        MusicThingie *from);
        PianodPlaylist *createPlaylist (const std::string &name, const Filter &filter);
        PianodPlaylist *formTransientPlaylist (const Filter &criteria);

        virtual bool writeIndexToFile (const std::string &filename) const override;
        virtual bool restoreIndexFromFile (const std::string &filename) override;
    };
};  // namespace MusicLibrary
