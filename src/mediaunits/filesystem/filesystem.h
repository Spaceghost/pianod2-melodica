///
/// Filesystem audio source.
/// Locates and plays audio files from a mounted filesystem.
/// @file       filesystem.h - pianod
/// @author     Perette Barella
/// @date       2015-02-25
/// @copyright  Copyright (c) 2015-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <cstdio>
#include <ctime>

#include <string>
#include <list>

#include "datastore.h"
#include "mediaunit.h"
#include "mediaplayer.h"
#include "audio/audiooutput.h"
#include "fundamentals.h"
#include "musiclibrary.h"
#include "musiclibraryparameters.h"

/** Classes for playing local media files stored within a directory
    hierarchy. */
namespace Filesystem {

    class Source;
    class Player;
    class Library;
    class Album;
    class Song;

    /// Artist customization for filesystem library & source.
    class Artist : public MusicLibrary::Artist {
        friend Album;  // Partial path from library base to artist directory.
        friend Song;

    private:
        std::string _path;  ///< Path from library root to this artist.  Always starts with '/'.
    public:
        using MusicLibrary::Artist::Artist;
        void path (std::string p);
        std::string path() const;
        virtual Parsnip::Data persist() const override;
        virtual void restore (const Parsnip::Data &data) override;
    };

    /// Album customization for filesystem library & source.
    class Album : public MusicLibrary::Album {
        friend Song;

    private:
        std::string _path;  ///< If starting with `/`, path from library root; otherwise appends to artist path.
    public:
        using MusicLibrary::Album::Album;
        void path (std::string p);
        std::string path() const;
        virtual void makeCompilation (MusicLibrary::Artist *compilation_artist) override;
        virtual Parsnip::Data persist() const override;
        virtual void restore (const Parsnip::Data &data) override;
    };

    /// Song customization for filesystem library & source.
    class Song : public MusicLibrary::Song {
        friend Source;
        friend Player;
        friend Library;

    private:
        std::string _path;     ///< If starting with `/`, path from library root; otherwise appends to album path.
        bool present = false;  ///< Set to false at start of a scan, true when found.
    public:
        using MusicLibrary::Song::Song;
        void path (std::string p);
        std::string path() const;
        virtual Parsnip::Data persist() const override final;
        virtual void restore (const Parsnip::Data &data) override final;
        virtual PianodConnection &transmitPrivate (PianodConnection &recipient) const override final;
        virtual void serializePrivate (Parsnip::Data &, const User *user) const override final;
    };

    /// Source parameters for filesystem source.
    struct Parameters : public MusicLibrary::LibraryParameters {
        std::string path;

        Parameters (Ownership::Type perm, User *user = nullptr);
        Parameters (const UserData::JSONData &src);
        virtual bool persist (UserData::JSONData &dest) const override final;
        void sanitize (void);
    };

    /// Filesystem-specific media library.
    class Library : public MusicLibrary::Library {
        friend Source;
        friend Song;

    private:
        using base_library = MusicLibrary::Library;
        using ScanFrequency = MusicLibrary::ScanFrequency;
        std::string _path;
        time_t last_scan = 0;
        bool links_warned = false;
        bool permissions_warned = false;
        time_t next_validity_check = 0;
        bool valid = false;
        bool ready = false;
        ScanFrequency scan_behavior;

        /// Types of items to scan when indexing media on a filesystem.
        enum class ScanItemType { DIRECTORY, FILE };

        /// Structure for storing lists of pending file scanning.
        struct ScanWorkItem {
            ScanItemType type = ScanItemType::FILE;
            std::string path;
            ScanWorkItem (const std::string &newpath) {
                path = newpath;
            }
        };
        /// Pending files to be scanned.  Items are processed in LIFO (stack) order.
        typedef std::list<ScanWorkItem> SearchList;
        SearchList searches;

    protected:
        bool isReady() { return ready && valid; }
        void purge();
        virtual void persist (Parsnip::Data &into) const override;
        virtual bool restore (const Parsnip::Data &data) override;

    public:
        Library (Media::Source *const owner,
                 const bool persistence,
                 const std::string directory,
                 const MusicLibrary::ScanFrequency behavior);
        inline const std::string &path() {
            return _path;
        };
        float periodic();
        bool checkValidity();
        void startScan();
        void clear();
        bool scanDirectory (const std::string &path);
        void indexFile (const std::string &path);
    };

    /// Source for filesystem media.
    class Source : public Media::Source {
        friend class Playlist;

    private:
        Library library;
        Retainer <PianodPlaylist *> mix_playlist;
        Retainer <PianodPlaylist *> everything_playlist;

    public:
        Source (const Parameters &params);

        // Identity
        virtual const char *kind (void) const override;

        // Capability check methods
        virtual bool canExpandToAllSongs (void) const override;

        // General source stuff
        virtual bool flush (void) override;
        virtual float periodic (void) override;

        // Playlist methods
        virtual PlaylistList getPlaylists (const Filter &filter = Filter::All) override;
        virtual MusicThingie *getAnythingById (const Media::SplitId &id) override;
        virtual PianodPlaylist *getMixPlaylist (void) override;
        virtual PianodPlaylist *getEverythingPlaylist (void) override;

        // Creating playlists
        virtual PianodPlaylist *createPlaylist (const char *name, MusicThingie::Type type, MusicThingie *from) override;
        virtual PianodPlaylist *createPlaylist (const char *name, const Filter &filter) override;
        virtual PianodPlaylist *getTransientPlaylist (const Filter &criteria) override;

        // Miscellaneous
        virtual Media::Player *getPlayer (const AudioSettings &audio, PianodSong *song) override;
        virtual SongList getRandomSongs (PianodPlaylist *playlist,
                                         const UserList &users,
                                         Media::SelectionMethod selectionMethod) override;
        virtual ThingieList getSuggestions (const Filter &filter, SearchRange where) override;
        virtual void playbackProblem (void) override;

        // Typecasts
        virtual MusicThingie *getSuggestion (MusicThingie *thing, MusicThingie::Type type, SearchRange where) override;

        // Utilities
        void rescan (bool reset);
    };
}  // namespace Filesystem
