///
/// Method implementations for music library types.
/// @file       musiclibrarytypes.cpp - pianod
/// @author     Perette Barella
/// @date       2014-12-09
/// @copyright  Copyright 2014-2023 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <string>
#include <set>

#include <parsnip/parsnip.h>
#include <football/football.h>

#include "fundamentals.h"
#include "musictypes.h"
#include "retainedlist.h"
#include "musiclibrary.h"
#include "musiclibraryparameters.h"
#include "utility.h"
#include "datastore.h"
#include "user.h"
#include "mediaunit.h"
#include "musickeys.h"
#include "connection.h"


/// Element and attribute keys for ratings.
namespace MusicLibrary {
    static const std::string noPlaylistName = "Bibliotheque";
    

    /** Make a list of unique strings.  Used for artists on compilation albums
        and genres on playlists. */
    class NameAggregator: public std::set<std::string> {
    public:
        /** Return the results of aggregation.
            @param joiner The string with which to connect members.
            @param limit Approximate maximum length of the returned string.
            @param tail A string to append if members are dropped. */
        std::string str(const std::string &joiner, std::string::size_type limit = 0xffff, const std::string &tail = "...") const {
            std::string result;
            bool dropped = false;
            for (auto &item : *this) {
                if (!item.empty()) {
                    if (result.empty() || (result.size() + joiner.size() + item.size() < limit)) {
                        if (!result.empty())
                            result += joiner;
                        result += item;
                    } else {
                        dropped = true;
                    }
                }
            }
            if (dropped) {
                result += tail;
            }
            return result;
        }
    };
    
    
    /*
     *                  Playlists
     */
    bool Playlist::appliesTo (const PianodSong *song) const {
        if (selector.matches (song)) return true;
        if (seeds.find (song->songId()) != seeds.end()) return true;
        if (seeds.find (song->albumId()) != seeds.end()) return true;
        if (seeds.find (song->artistId()) != seeds.end()) return true;
        return false;
    };

    bool Playlist::canSeed (MusicThingie::Type) const {
        return true;
    }

    bool Playlist::seed (MusicThingie::Type seed_type,
                         const MusicThingie *music) const {
        assert (music->source() == this->source());
        if (seed_type == MusicThingie::Type::Playlist)
            throw CommandError (E_MEDIA_VALUE);
        const std::string &seed_id = music->internalId (seed_type);
        if (seed_id.empty()) return false;
        return (seeds.find (seed_id) != seeds.end());
    }

    void Playlist::seed (MusicThingie::Type seed_type,
                         MusicThingie *music,
                         bool value) {
        assert (music->source() == this->source());
        if (seed_type == MusicThingie::Type::Playlist)
            throw CommandError (E_MEDIA_VALUE);
        const std::string &seed_id = music->internalId (seed_type);
        if (seed_id.empty())
            throw CommandError (E_INVALID);
        if (seed_type == Type::Artist && value) {
            PianodArtist *artist = music->asArtist();
            assert (artist);
            if (artist->artist().empty()) {
                throw CommandError (E_WRONGTYPE, "Artist unidentified");
            }
        } else if (seed_type == Type::Album && value) {
            PianodAlbum *album = music->asAlbum();
            assert (album);
            // asArtist() returns actual artist for compilation albums.
            if (album->albumTitle().empty() && album->asArtist()->artist().empty()) {
                throw CommandError (E_WRONGTYPE, "Album unidentified");
            }
        }
        assert (static_cast <MusicThingie::Type> (seed_id [0]) == seed_type);
        SeedSet::const_iterator it = seeds.find (seed_id);
        if ((value && it != seeds.end()) || (!value && it == seeds.end())) {
            // Already in correct state.
            return;
        }
        if (value) {
            std::pair<SeedSet::iterator, bool> result = seeds.insert (seed_id);
            if (!result.second)
                throw CommandError (E_NAK);
        } else {
            seeds.erase (it);
            invalidateSeeds (music);
        }
        genres_dirty = true;
        _library->populatePlaylist (this);
        _library->markDirty (Foundation::NOMINAL);
    }

    ThingieList Playlist::getSeeds (void) const {
        return _library->seedsForPlaylist (this);
    };

    SongList Playlist::songs () {
        return _library->getPlaylistSongs (this, true);
    }

    SongList Playlist::songs (const Filter &filter) {
        SongList candidates = this->songs();
        SongList results;
        results.reserve (candidates.size());
        for (auto song : candidates) {
            if (filter.matches (song)) {
                results.push_back (song);
            }
        }
        return results;
    }


    void Playlist::invalidateSeeds(const MusicThingie *music) {
        auto artist = dynamic_cast<const Artist *>(music);
        if (artist) {
            for (auto album : artist->albums) {
                for (auto song : album->_songs) {
                    if (song->_playlist == this) song->_playlist = nullptr;
                }
            }
            return;
        }
        // To ensure we invalidate all related seeds,
        // do it from our parent artist.
        auto album = dynamic_cast<const Album *>(music);
        if (album) {
            invalidateSeeds (album->_artist);
        } else {
            auto song = dynamic_cast<const Song *>(music);
            assert (song);
            invalidateSeeds (song->_album->_artist);
        }
    }

    void Playlist::updateSelector (const Filter &new_selector) {
        selector = new_selector;
    }

    void Playlist::rename (const std::string &newname) {
        _name = newname;
        return;
    }

    void Playlist::erase () {
        if (!library()->removePlaylist (this)) {
            throw CommandError (E_NAK);
        }
    }


    PianodPlaylist *Song::playlist (void) const {
        return _playlist;
    };

    Parsnip::Data Playlist::persist () const {
        Parsnip::Data seed_list {Parsnip::Data::List};
        for (const auto &seed : seeds) {
            seed_list.push_back (seed);
        }
        
        return Parsnip::Data { Parsnip::Data::Dictionary,
            Music::Key::PlaylistId, _id,
            Music::Key::PlaylistName, _name,
            Music::Key::PlaylistEnabled, enabled,
            Music::Key::PlaylistSelector, selector.toString(),
            Music::Key::PlaylistSeeds, std::move (seed_list)
        };
    };

    void Playlist::restore (const Parsnip::Data &data) {
        enabled = data [Music::Key::PlaylistEnabled].asBoolean();
        try {
            selector = data [Music::Key::PlaylistSelector].asString();
        } catch (const CommandError &err) {
            flog (LOG_WHERE (Log::WARNING),
                  "Playlist ", playlistName(), " has an invalid filter expression: ", err.reason());
            selector = Filter::None;
        } catch (const Parsnip::Exception &err) {
            flog (LOG_WHERE (Log::WARNING),
                  "Playlist ", playlistName(), ": ", err.what());
            selector = Filter::None;
        }
        for (const auto &seed : data [Music::Key::PlaylistSeeds]) {
            seeds.insert (seed.asString());
        }
        genres_dirty = true;
    }

    void Playlist::calculateGenres () const {
        NameAggregator genres;
        for (auto song : _library->getPlaylistSongs (this)) {
            const char *gen = song->genre().c_str();
            while (isspace (*gen))
                gen++;
            while (*gen) {
                // Grab everything up to one of the genre separators
                std::string::size_type len = strcspn (gen, "/,+");
                if (len) {
                    genres.insert (trim (std::string {gen, len}));
                }
                // Skip past the genre and any separators
                gen += len;
                gen += strspn (gen, "/,+ \t");
            }
        }

        // Assemble the list into a string
        _genres = genres.str (", ", 50);
        genres_dirty = false;
    }
    
    /** (Re)calculate the current genres list for a playlist. */
    const std::string &Playlist::genre (void) const {
        if (genres_dirty) {
            calculateGenres();
        }
        return _genres;
    }

    PianodConnection &Playlist::transmitPrivate (PianodConnection &recipient) const {
        PianodPlaylist::transmitPrivate (recipient);
        if (this->isOwnedBy (recipient.user)) {
            recipient << Response (I_SELECTION_ALGORITHM, selector.toString());
        }
        return (recipient);
    }
    
    void Playlist::serializePrivate (Parsnip::Data &data, const User *user) const {
        PianodPlaylist::serializePrivate (data, user);
        if (this->isOwnedBy (user)) {
            data ["library"] = Parsnip::Data::make_dictionary ({
                { Music::Key::PlaylistSelector, selector.toString() }
            });
        }
    }


    /*
     *                  Transient Playlist
     */

    TransientPlaylist::TransientPlaylist (Foundation *const library, const Filter &criteria) :
    Playlist (library, "transient", "Transient") {
        selector = criteria;
    }

    SongList TransientPlaylist::songs () {
        return _library->getMatchingSongs (selector);
    }





    /*
     *                  Artists
     */

    /** Create a new artist.
        @param library The library in which the artist will reside.
        @param id A unique ID for the artist.
        @param name The name of the artist. */
    Artist::Artist (Foundation *const library,
                    const std::string &id, const std::string &name)
    : _library (library), _id (id), _name (name) {
    };
    Artist::~Artist() {
        assert (albums.empty());
    }

    /** Get all songs belonging to all albums by this artist.
        @return All songs on albums belonging to this artist. */
    SongList Artist::songs () {
        int song_count = compilation_songs.size();
        for (auto album : albums) {
            song_count += album->_songs.size();
        }
        SongList songs;
        songs.reserve (song_count);
        for (auto *song : compilation_songs)
            songs.push_back (song);
        for (auto album : albums) {
            songs.join (album->songs());
        }
        return songs;
    };

    Parsnip::Data Artist::persist () const {
        Parsnip::Data all_albums { Parsnip::Data::List };
        
        for (auto album : albums) {
            all_albums.push_back (album->persist ());
        }
        return Parsnip::Data { Parsnip::Data::Dictionary,
            Music::Key::ArtistId, _id,
            Music::Key::ArtistName, _name,
            Music::Key::ArtistAlbums, std::move (all_albums)
        };
    };
    void Artist::restore (const Parsnip::Data &data) {
        // Nothing to do in this base class.
    }




    /*
     *                  Albums
     */

    /** Create a new album, and register itself with its parent.
        @param parent The album's artist.
        @param id A unique ID for the album.
        @param name The name of the album. */
    Album::Album (Artist *const parent, const std::string &id, const std::string &name)
    : _artist (parent), _id (id), _name (name) {
        _artist->albums.push_back (this);
        _artist->retain();
    };

    /** Destroy an album by unregistering from its parent artist. */
    Album::~Album (void) {
        assert (_songs.empty());
        auto it = find (_artist->albums.begin(), _artist->albums.end(), this);
        assert (it != _artist->albums.end());
        if (it != _artist->albums.end())
            _artist->albums.erase (it);
        _artist->release();
    }


    /** Get the current artist, or list of artists if a compilation. */
    const std::string &Album::artist (void) const {
        if (compilation() && artists_dirty)
            calculateArtists();
        return (compilation() ? _artists : _artist->_name);
    };

    /** (Re)calculate the current artists list for a compilation. */
    void Album::calculateArtists (void) const {
        assert (compilation()); // Harmless but wasteful on non-compilation
        NameAggregator artists;
        for (auto song : _songs) {
            artists.insert (trim (song->artist()));
        }
        _artists = artists.str("; ");
        artists_dirty = false;
    }

    bool Album::compilation () const {
        return (_songs.empty() ? false :
                _songs.front()->_artist != nullptr);
    };


    /** Mark an album as a compilation.
        Set all song artists to the current artist.
        Detach the item from its artist and stick it into a special compilation
        artist.
        @param compilation_artist The compilation artist to move the album to. */
    void Album::makeCompilation (Artist *compilation_artist) {
        assert (compilation_artist);

        // If already a compilation, do nothing.
        if (compilation()) return;

        // Move artist name into existing songs, add song to artist's compilation songs.
        for (auto song : _songs) {
            assert (!song->_artist);
            if (song->_artist) {
                flog (Log::WARNING, operator()(), " was ambiguously a compilation");
                if (song->_artist != compilation_artist) {
                    flog (Log::WARNING, operator()(), " is not the compilation artist ",
                          (*compilation_artist)());
                }
            }
            song->artist (_artist);
        }

        // Remove album from existing artist
        auto it = find (_artist->albums.begin(), _artist->albums.end(), this);
        assert (it != _artist->albums.end());
        if (it != _artist->albums.end())
            _artist->albums.erase (it);
        _artist->release();

        // Add to new artist
        _artist = compilation_artist;
        _artist->albums.push_back (this);
        compilation_artist->retain();
    };

    /** Get all songs belonging to the album.
        @return The songs belonging to the album. */
    SongList Album::songs () {
        sort (_songs.begin(), _songs.end(), [] (Song *a, Song *b) {
            int result = a->trackNumber() - b->trackNumber();
            if (result == 0) {
                result = compare_title_order(a->title(), b->title());
            }
            return result < 0;
        });
        SongList list;
        list.reserve (_songs.size());
        for (auto song : _songs) {
            list.push_back (song);
        }
        return list;
    };


    Parsnip::Data Album::persist () const {
        Parsnip::Data all_songs { Parsnip::Data::List };
        for (auto song : _songs) {
            all_songs.push_back (song->persist());
        }
        Parsnip::Data album { Parsnip::Data::Dictionary,
            Music::Key::AlbumId, _id,
            Music::Key::AlbumName, _name,
            Music::Key::AlbumSongs, std::move (all_songs)
        };

        if (compilation())
            album [Music::Key::AlbumIsCompilation] = true;
            
        return album;
    };
    
    void Album::restore (const Parsnip::Data &data) {
    }



    /*
     *                  Songs
     */

    /** Add a song, and register itself with its parent album.
        @param parent The song's album.
        @param id A unique ID for the song.
        @param name The name of the song. */
    Song::Song (Album *const parent, const std::string &id, const std::string &name)
    : _album (parent), _id (id), _name (name) {
        _album->_songs.push_back (this);
        _album->retain();
        _album->artists_dirty = true;
    };
    const std::string &Song::artist (void) const {
        return (_artist ? _artist->_name : _album->_artist->_name);
    };

    Song::~Song (void) {
        // If song is part of a compilation, detach it from its artist.
        if (_artist) {
            auto it = find (_artist->compilation_songs.begin(), _artist->compilation_songs.end(), this);
            assert (it != _artist->compilation_songs.end());
            if (it != _artist->compilation_songs.end())
                _artist->compilation_songs.erase (it);
            _artist->release();
        }
        auto it = find (_album->_songs.begin(), _album->_songs.end(), this);
        assert (it != _album->_songs.end());
        if (it != _album->_songs.end())
            _album->_songs.erase (it);
        _album->artists_dirty = true;
        _album->release();
    };

    /** Set the artist for this song.  This applies only for compilation albums.
        @param artist The artist of the song. */
    void Song::artist (Artist *artist) {
        if (_artist)
            _artist->release();
        _artist = artist;
        _artist->retain();
        _artist->compilation_songs.push_back (this);
    };

    RESPONSE_CODE Song::rate (Rating value, User *user) {
        assert (user);
        auto ratings = UserData::Ratings::retrieve (user, UserData::Key::TrackRatings, source()->key());
        if (!ratings) {
            return E_RESOURCE;
        }
        try {
            (*ratings) [songId()] = value;
            user->updateData();
            return S_OK;
        } catch (const std::bad_alloc &) {
            return E_RESOURCE;
        }
    }

    Rating Song::rating (const User *user) const {
        assert (user);
        auto ratings = UserData::Ratings::retrieve (user, UserData::Key::TrackRatings, source()->key());
        if (!ratings) return Rating::UNRATED;
        return ratings->get (songId(), Rating::UNRATED);
    }

    RESPONSE_CODE Song::rateOverplayed (User *user) {
        if (!user) return E_LOGINREQUIRED;
        auto overplays = UserData::OverplayedList::retrieve (user, source()->key());
        if (!overplays) {
            return E_RESOURCE;
        }
        try {
            // 30 days in seconds
            (*overplays) [songId()] = time(nullptr) + 86400 * 30;
            user->updateData();
            return S_OK;
        } catch (const std::bad_alloc &) {
            return E_RESOURCE;
        }
    }

    Parsnip::Data Song::persist () const {
        Parsnip::Data song { Parsnip::Data::Dictionary,
            Music::Key::SongId, _id,
            Music::Key::SongName, _name,
            Music::Key::SongGenre, _genre
        };

        if (_artist) {
            song [Music::Key::ArtistId] = _artist->_id;
            song [Music::Key::ArtistName] = _artist->_name;
        }
        if (_duration)
            song [Music::Key::SongDuration] = _duration;
        if (_year)
            song [Music::Key::SongYear] = _year;
        if (_trackNumber)
            song [Music::Key::SongTrackNumber] = _trackNumber;
        if (last_played)
            song [Music::Key::SongLastPlayed] = last_played;
        return (song);
    };

    void Song::restore(const Parsnip::Data &data) {
        _genre = data [Music::Key::SongGenre].asString();
        if (data.contains (Music::Key::SongDuration))
            _duration = data [Music::Key::SongDuration].asInteger();
        if (data.contains (Music::Key::SongYear))
            _year = data [Music::Key::SongYear].asInteger();
        if (data.contains (Music::Key::SongTrackNumber))
            _trackNumber = data [Music::Key::SongTrackNumber].asInteger();
        if (data.contains (Music::Key::SongLastPlayed))
            last_played = data [Music::Key::SongLastPlayed].as<time_t> ();
    }
}
