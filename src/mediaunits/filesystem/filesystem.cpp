///
/// Filesystem media library.
/// Scan media and maintain catalog/index.
/// @file       filesystem.cpp - pianod
/// @author     Perette Barella
/// @date       2015-02-25
/// @copyright  Copyright 2015-2023 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <cerrno>
#include <cstring>

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <exception>
#include <stdexcept>

#include "fundamentals.h"
#include "logging.h"
#include "musiclibrary.h"
#include "musiclibraryhash.h"
#include "filesystem.h"
#include "metadata/metadata.h"

namespace Filesystem {

    static MusicLibrary::Allocator <Artist, MusicLibrary::Foundation> artist_allocate;
    static MusicLibrary::Allocator <Album, MusicLibrary::Artist> album_allocate;
    static MusicLibrary::Allocator <Song, MusicLibrary::Album> song_allocate;

    /** Construct a library for filesystem media.
        @param owner The source that owns the library.
        @param persistence If true, library contents should be persisted.
        @param directory The path to media files.
        @param behavior Indicates when to search for new media. */
    Library::Library (Media::Source * const owner,
                      const bool persistence,
                      const std::string directory,
                      const ScanFrequency behavior) :
    MusicLibrary::Library (owner, persistence, song_allocate, album_allocate, artist_allocate),
    _path (directory.substr (0, directory.length() - 1)),
    scan_behavior (behavior) {
        // Sanity check that a fully qualified path was provided.
        assert (!directory.empty());
        assert (directory [0] == '/');
        assert (directory [directory.length() - 1] == '/');

        bool existing = load();
        if (!existing && behavior == ScanFrequency::NEVER)
            scan_behavior = ScanFrequency::NEXTSTARTUP;
        if (checkValidity() && existing) {
            mixRecalculate();
            ready = true;
        }
    };

    /** Validate that a filesystem souce is available.
        If the filesystem is missing, make the source invalid; when
        it becomes available, make it valid and start indexing it. */
    bool Library::checkValidity() {
        struct stat file_details;
        bool valid_now = false;
        if (stat (_path.c_str(), &file_details) != 0) {
            flog (LOG_WHERE (Log::ERROR),
                  _path, ": stat() failed: ", strerror (errno));
        } else if (S_ISDIR (file_details.st_mode)) {
            valid_now = true;
        } else {
            flog (LOG_WHERE (Log::ERROR),
                  _path, ": Not a directory.");
        }

        if (valid_now && !valid) {
            if (scan_behavior == ScanFrequency::EVERYSTARTUP ||
                scan_behavior == ScanFrequency::NEXTSTARTUP) {
                startScan();
            }
            valid = true;
        } else if (!valid_now && valid) {
            searches.clear();
            valid = false;
        }
        next_validity_check = time (nullptr) + (valid_now ? 301 : 31);
        return valid_now;
    }

    /** Initiate a scan to update the catalog with new/changed media. */
    void Library::startScan () {
        if (!searches.empty()) {
            throw std::invalid_argument ("Scan already in progress");
        }
        for (auto song : songs) {
            static_cast <Song *> (song.second)->present = false;
        }
        if (!scanDirectory (_path + "/")) {
            throw std::invalid_argument ("Cannot read directory");
        }
        if (searches.empty()) {
            throw std::invalid_argument ("No audio files in specified location");
        }
    }

    /** Remove all indexed contents from the library. */
    void Library::clear () {
        songs.clear();
        albums.clear();
        artists.clear();
    }

    void Library::persist (Parsnip::Data &params) const {
        params [Music::Key::MediaPath] = _path;
        params [Music::Key::MediaLastScan] = last_scan;
    };
    
    bool Library::restore (const Parsnip::Data &data) {
        _path = data [Music::Key::MediaPath].asString();
        last_scan = data [Music::Key::MediaLastScan].as<time_t> ();
        return true;
    };

    /** Remove missing songs from the library.  Invoked after a scan,
        removes all songs not present during the scan. */
    void Library::purge () {
        songs.purge ([] (const MusicLibrary::Song *song)->bool {
                return (song->getUseCount() == 1 && !static_cast <const Song *> (song)->present);
            });
        base_library::purge();
    }

    /** Background tasks:
        - Flush dirty data files periodically.
        - If scanning, process one file or directory.
        @return Number of seconds until next service is requested. */
    float Library::periodic () {
        float next_request = MusicLibrary::Foundation::periodic();
        if (time (nullptr) >= next_validity_check) {
            checkValidity();
        }
        if (!valid && next_request > 30) {
            next_request = 30;
        }
        if (!searches.empty()) {
            ScanWorkItem work = searches.back();
            searches.pop_back();
            if (work.type == ScanItemType::DIRECTORY) {
                scanDirectory (work.path);
            } else {
                indexFile (work.path);
            }
            if (searches.empty()) {
                ready = true;
                purge();
                flog (LOG_WHERE (Log::FILESYSTEM),
                      "Filesystem ", _path, ": ",
                      artists.size(), " artists, ",
                      albums.size(), " albums, ",
                      songs.size(), " songs");
                source->reportStatus((std::to_string (songs.size()) + " songs, " +
                                std::to_string (albums.size()) + " albums, " +
                                std::to_string (artists.size()) + " artists"));
                markDirty();
                last_scan = time (nullptr);
                if (scan_behavior == ScanFrequency::NEXTSTARTUP)
                    scan_behavior = ScanFrequency::NEVER;
                flush();
                mixRecalculate();
            }
        }
        return (searches.empty() ? next_request : 0);
    };

    /** Queue a directory for addition to the library catalog.
        - Files are added to the scan list.
        - Subdirectories (not . and ..) are added to the scan list.
        - Symbolic links, generate a single warning that they are ignored.
        - Devices, fifos, sockets and other weirdness are silently ignored.
        Future work is *pushed* onto the work list, rather than *queued*;
        maximum work list size is filesystem depth + longest directory whereas
        queueing could result in a much larger list.
        @param path The directory to process. */
    bool Library::scanDirectory(const std::string &path) {
        DIR *dir = opendir (path.c_str());
        if (!dir) {
            flog (LOG_WHERE (Log::ERROR),
                  path, ": opendir() failed: ", strerror (errno));
            if (!permissions_warned) {
                source->alert (F_PERMISSION, "Cannot index some directories");
                permissions_warned = true;
            }
            return false;
        }
        struct dirent *file_entry;
        while ((file_entry = readdir (dir))) {
            ScanWorkItem work (path + file_entry->d_name);
            struct stat file_details;
            if (lstat (work.path.c_str(), &file_details) != 0) {
                flog (LOG_WHERE (Log::ERROR),
                      work.path, ": stat() failed: ", strerror (errno));
            } else if (S_ISDIR (file_details.st_mode)) {
                work.type = ScanItemType::DIRECTORY;
                work.path.append ("/");
                if (strcmp (file_entry->d_name, ".") != 0 &&
                    strcmp (file_entry->d_name, "..") != 0) {
                    searches.push_back (work);
                }
            } else if (S_ISREG (file_details.st_mode)) {
                searches.push_back (work);
            } else if (S_ISLNK (file_details.st_mode)) {
                if (!links_warned) {
                    flog (LOG_WHERE (Log::FILESYSTEM|Log::WARNING), work.path, ": symlink ignored");
                    source->reportStatus ("Symbolic links ignored");
                    links_warned = false;
                }
            }
        }
        closedir (dir);
        return true;
    }

    /** Add a file to the library catalog, generating IDs based on data so
        they remain persistent in case the index is destroyed.
        - Album:
            1.  use unique album ID, such as CDDB ID.
            2.  ifnotfound or collision, use hash of album name.
            3.  if collision, use randomly assigned.
        - Artist:
            1.  if album is compilation*, use dedicated compilation artist (acomp)
                and store artist name with track data.
            2.  otherwise, ID is hash of artist name.
            3.  Collisions do not apply to artists.
        - Song:
            1.  if track has a number, use album ID + track #.
            2.  otherwise, hash the album name and track name.
            3.  if collision, use randomly assigned ID.
        Albums are always non-compilations when their first track is added.
        When the additional tracks are added, if the artist differs, then the
        album is converted to a compilation.
        @param path The pathname of the file to index.
     */
    void Library::indexFile (const std::string &path) {
        Media::Metadata track;
        try {
            track = Media::Metadata::getMetadata (path);
        } catch (const Media::MediaException &e) {
            return;
        }

        // Get a candidate album ID and make sure it's the same album
        std::string album_id;
        if (!track.cddb_id.empty()) {
            album_id = MusicLibrary::persistentId (MusicThingie::Type::Album, track.cddb_id);
        }
        Album *album = nullptr;
        if (!album_id.empty()) {
            album = static_cast <Album *> (albums.getById (album_id));
            if (album && *album != track.album) {
                // CDDB ID collision; use alternative
                flog (LOG_WHERE (Log::FILESYSTEM), "Album ID Collision: Album ",
                      album->albumTitle(), " vs ", track.album);
                album = nullptr;
                album_id = "";
            }
        }

        // If Album ID didn't work, hash the album title and try that.
        // But if the album title is empty, hash the artist's name.
        if (!album && album_id.empty()) {
            album_id = MusicLibrary::persistentId (MusicThingie::Type::Album,
                                                   track.album.empty() ? track.artist : track.album);
            album = static_cast <Album *> (albums.getById (album_id));
            if (album) {
                std::string album_path = album->path();
                if (*album != track.album) {
                    flog (LOG_WHERE (Log::FILESYSTEM), "Hash collision: Album ",
                          album->albumTitle(), " vs ", track.album);
                    album = nullptr;
                    album_id = "";
                }
            }
        }

        // Set up the artist and attach the album
        Artist *artist;
        bool compilation = (album && !compare_person_or_title (album->artist(), track.artist));
        if (compilation) {
            artist = static_cast <Artist *> (artists.addOrGetItem ("", "acomp", this));
            album->makeCompilation (artist);
        } else {
            // If there is no artist name, use a special id to avoid mixing with the compilations.
            artist = static_cast <Artist *> (artists.addOrGetItem (track.artist, track.artist.empty() ? "anoname" : "", this));
            if (!album) {
                album = static_cast <Album *> (albums.addOrGetItem (track.album, album_id, artist));
            }
        }

        // Candidate ID: if track has a number, create song id:
        //      song_id = album_id + disc number (if present and > 1) + track #.
        // Otherwise, use hash of album title + track title.
        std::string candidate_id;
        if (track.track_number) {
            candidate_id = album->albumId();
            candidate_id [0] = static_cast <char> (MusicThingie::Type::Song);
            if (track.disc_number > 1) {
                candidate_id.append ("-").append (std::to_string (track.disc_number));
            }
            candidate_id.append ("-").append (std::to_string (track.track_number));
        } else {
            candidate_id = MusicLibrary::persistentId (MusicThingie::Type::Song,
                                                       track.album + track.title);
        }
        Song *song = static_cast <Song *> (songs.getById (candidate_id));
        if (song && song->path() != path) {
            // Is the file renamed, or is it a naming collision?
            std::string alternate_id = MusicLibrary::persistentId (MusicThingie::Type::Song,
                                                              path);
            Song *alternate = static_cast <Song *> (songs.getById (alternate_id));
            if (alternate && alternate->path() == path) {
                // The alternate already exists, so go with that.
                song = alternate;
            } else if (!song->present && song->artist() == track.artist &&
                       song->albumTitle() == track.album &&
                       song->trackNumber() == track.track_number) {
                // The alternate doesn't exist and details match, so
                // it appears the file was renamed.  Use it.
            } else if (song->present && song->artist() == track.artist &&
                       song->albumTitle() == track.album) {
                // Two tracks on the album use the same name.
                flog (LOG_WHERE (Log::FILESYSTEM), "Duplicate naming: ",
                      song->path(), " vs ", path,
                      ", using ", alternate ? "random" : "alternate");
                song = nullptr;
                candidate_id = alternate ? "" : alternate_id;
            } else {
                flog (LOG_WHERE (Log::FILESYSTEM), "Hash collision: ",
                      song->path(), " vs ", path,
                      ", using ", alternate ? "random" : "alternate");
                song = nullptr;
                candidate_id = alternate ? "" : alternate_id;
            }
        }
        if (!song) {
            song = static_cast <Song *> (songs.addItem (track.title, candidate_id, album));
            assert (song);
        }
        song->year (track.year);
        song->genre (track.genre);
        song->path (path);
        song->trackNumber (track.track_number);
        song->present = true;
        song->duration (track.duration);
        if (compilation) {
            song->artist (artists.addOrGetItem (track.artist, this));
        }
    }
}
