///
/// Artist, album, and song extensions for filesystem source.
/// Filesystem source parameters.
/// @file       filesystemtypes.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-02-26
/// @copyright  Copyright (c) 2015-2020 Devious Fish. All rights reserved.
///

#include <config.h>

#include <string>

#include "filesystem.h"
#include "sources.h"
#include "user.h"
#include "connection.h"
#include "parsnip/parsnip.h"

const char PathSeparator = '/';

namespace Filesystem {
    /** Handle path reduction by stripping a partial path off a complete path,
        if the two match.  If not, do nothing.
        @param path A complete path.  Modified to remove the partial path, if matching.
        @param component A partial path. */
    static void remove_path (std::string &path,
                             const std::string &component) {
        if (path.compare(0, component.length(), component) == 0) {
            path.erase (0, component.length());
        }
    }



    /*
     *              Artist
     */

    /** Set the path to the artist.
        - If already set, do nothing.
        - If unset set the artist path to the path provided, with the library
        path stripped off.
        @param p The path to adopt. */
    void Artist::path (std::string p) {
        if (!_path.empty())
            return; // Already set; can't be changed.

        std::string library_path = static_cast <Library *> (library())->path();
        if (library_path.length() < p.length()) {
            assert (library_path == p.substr (0, library_path.length()));
            _path = p;
        } else {
            assert (p == library_path.substr (0, p.length()));
            _path = library_path;
        }
        remove_path (_path, library_path);
    }

    /** Retrieve the path to the artist by combining the library and
        artist-specific path.
        @return Path to directory with media for this artist. */
    std::string Artist::path () const {
        return static_cast <Library *> (library())->path() + _path;
    }

    Parsnip::Data Artist::persist () const {
        Parsnip::Data artist { MusicLibrary::Artist::persist () };
        artist [Music::Key::ArtistPath] = _path;
        return (artist);
    }

    void Artist::restore (const Parsnip::Data &data) {
        _path = data [Music::Key::ArtistPath].asString();
        MusicLibrary::Artist::restore(data);
    }

    /*
     *              Album
     */

    /** Set the path to the album.
        If already set, do nothing.
        Otherwise:
        - Set the album path to that provided.
        - Try setting the artist path to the path, with the last component removed.
        - Truncate the path by stripping the artist off the front.
        - If the artist path can't be removed, strip off the library path instead,
        leaving a leading '/' to delineate.
        @param p The path to adopt. */
    void Album::path (std::string p) {
        if (!_path.empty())
            return; // Already set; can't be changed.

        _path = p;

        // Strip the album name off
        size_t pos = (p.length() < 2 ? 0 : p.rfind (PathSeparator, p.length() - 2));
        if (pos > 0) {
            p.erase (pos + 1);

            // Try assigning the remainder to the artist path.
            static_cast <Artist *> (_artist)->path (p);

            // Remove redundant section of path
            remove_path (_path, static_cast <Artist *> (_artist)->path());
            if (!_path.empty() && _path [0] == PathSeparator) {
                remove_path (_path, static_cast <Library *> (library())->path());
            }
        }
    }

    /** Retrieve the path to the album.
        - If the path starts with '/', append it to the library path.
        - Otherwise, append it to the artist path.
        @return The full path to an album. */
    std::string Album::path () const {
        if (_path.empty() || _path [0] != PathSeparator) {
            return static_cast <Artist *> (_artist)->path() + _path;
        }
        return static_cast <Library *> (library())->path() + _path;
    }

    void Album::makeCompilation (MusicLibrary::Artist *compilation_artist) {
        std::string old_path = path();
        MusicLibrary::Album::makeCompilation (compilation_artist);
        // Set the path back to what it was originally,
        // adjusting for the artist change.
        _path.clear();
        path (old_path);
    }

    Parsnip::Data Album::persist () const {
    Parsnip::Data album { MusicLibrary::Album::persist () };
        album [Music::Key::AlbumPath] = _path;
        return (album);
    }
    void Album::restore (const Parsnip::Data &data) {
        _path = data [Music::Key::AlbumPath].asString();
        MusicLibrary::Album::restore(data);
    }

    /*
     *              Song
     */

    /** Assign the path to a song.
        - Set the song path to that provided.
        - Try setting the artist path to the path, with the filename removed.
        - Truncate the path by stripping the album off the front.
        - If the album path can't be removed, strip off the library path instead,
        leaving a leading '/' to delineate.
        @param p The path to adopt. */
    void Song::path (std::string p) {
        assert (this);
        _path = p;

        // Strip the filename off
        size_t pos = p.rfind (PathSeparator);
        if (pos > 0) {
            p.erase (pos + 1);

            // Try assigning the remainder to the album.
            static_cast <Album *> (_album)->path (p);

            // Remove redundant section of path
            remove_path (_path, static_cast <Album *> (_album)->path());
            if (!_path.empty() && _path [0] == PathSeparator) {
                remove_path (_path, static_cast <Library *> (library())->path());
            }
        }
    }

    /** Retrieve the path to the song.
        - If the path starts with '/', append it to the library path.
        - Otherwise, append it to the album path.
        @return The path to a song's media file. */
    std::string Song::path () const {
        assert (this);
        if (_path.empty() || _path [0] != PathSeparator) {
            return static_cast <Album *> (_album)->path() + _path;
        }
        return static_cast <Library *> (library())->path() + _path;
    }

    Parsnip::Data Song::persist () const {
        Parsnip::Data song { MusicLibrary::Song::persist () };
        song [Music::Key::SongPath] = _path;
        return (song);
    }
    void Song::restore (const Parsnip::Data &data) {
        _path = data [Music::Key::SongPath].asString();
        MusicLibrary::Song::restore(data);
    }

    PianodConnection &Song::transmitPrivate (PianodConnection &recipient) const {
        PianodSong::transmitPrivate (recipient);
        if (recipient.haveRank (Rank::Administrator)) {
            recipient << Response (I_PATHNAME, path());
        }
        return recipient;
    }

    void Song::serializePrivate (Parsnip::Data &data, const User *user) const {
        PianodSong::serializePrivate (data, user);
        if (user && user->haveRank (Rank::Administrator)) {
            data [SourceName::FileSystem] = Parsnip::Data::make_dictionary ({
                { Music::Key::SongPath, path() }
            });
        }
    }


    /*
     *              Parameters
     */
    Parameters::Parameters (Ownership::Type perm,
                            User *user) :
    LibraryParameters (perm, user) {
    };

    /** Create new connection parameters, retrieving values from user settings.
        @param src A user settings dictionary to retrieve settings from.
        @throw invalid_argument if values in the settings dictionary are invalid. */
    Parameters::Parameters (const UserData::JSONData &src) :
    LibraryParameters (src) {
        path = src ["path"].asString();
        if (path.empty()) {
            throw std::invalid_argument ("missing source path");
        }
    }

    /// Copy connection parameters into a user settings dictionary.
    bool Parameters::persist (UserData::JSONData &dest) const {
        dest ["path"] = path;
        return MusicLibrary::LibraryParameters::persist(dest);
    }

    /// Sanity check the parameters and make adjustments if required.
    void Parameters::sanitize (void) {
        if (path.empty())
            throw std::invalid_argument ("Path must be specified");
        if (path [0] != PathSeparator)
            throw std::invalid_argument ("Full path required");
        if (path [path.size() - 1] != PathSeparator) {
            path.append("/");
        }
    }
}
