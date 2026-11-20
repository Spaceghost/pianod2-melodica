///
/// Method implementations for music library.
/// @file       musiclibrary.cpp - pianod
/// @author     Perette Barella
/// @date       2014-12-09
/// @copyright  Copyright 2014-2023 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <math.h>

#include <string>
#include <unordered_map>
#include <functional>

#include <cstdio>

#include <football/football.h>
#include <parsnip/parsnip.h>

#include "musictypes.h"
#include "retainedlist.h"
#include "musiclibrary.h"
#include "musiclibraryparameters.h"
#include "fundamentals.h"
#include "utility.h"
#include "datastore.h"
#include "user.h"
#include "users.h"
#include "mediaunit.h"
#include "musickeys.h"
#include "fileio.h"
#include "musiclibraryhash.h"


namespace MusicLibrary {
    Allocator <Artist, Foundation> artist_allocate;
    Allocator <Album, Artist> album_allocate;
    Allocator <Song, Album> song_allocate;
    Allocator <Playlist, Foundation> playlist_allocate;

    // Explicitly instantiate needed containers.
    template class ThingieContainer <Artist, Foundation>;
    template class ThingieContainer <Album, Artist>;
    template class ThingieContainer <Song, Album>;
    template class ThingieContainer <Playlist, Foundation>;

    /*
     *                  Library Foundation
     */

    /** Construct a new media library.
        @param owner The source to which the library belongs.
        @param persistence If true, library indexes are written to long-term storage. */
    Foundation::Foundation (Media::Source * const owner, const bool persistence) : persist_data (persistence), source (owner) {
    }

    /** Restore persisted library data from a file.
        @return True on success, false on failure. */
    bool Foundation::load() {
        bool ok;
        try {
            ok = restoreIndexFromFile (source->filename());
        } catch (const std::ios_base::failure &) {
            ok = false;
        }
        write_time = 0;
        return ok;
    }

    /** If memory is dirty, write library data to a file.
        @return True on success, false on failure. */
    bool Foundation::flush() {
        if (write_time == 0 || !persist_data)
            return true;
        // In case of failure, try again in a while
        write_time = time (0) + 1800;
        bool status = writeIndexToFile (source->filename());
        if (status)
            write_time = 0;
        return status;
    }

    /** Do intermittent tasks, such as occasionally persisting
        the catalog to a file.
        @return Interval until function wants to be called again. */
    float Foundation::periodic() {
        if (write_time) {
            time_t now = time (nullptr);
            if (now >= write_time) {
                flush();
                return A_LONG_TIME;
            }
            return write_time - now;
        }
        return A_LONG_TIME;
    }

    using BiasType = double;

    /** Calculate bias values for some songs.
        @param candidates The list of songs for which to compute biases.
        @param [out] biases On return, the computed biases.
        @param users_ratings Users' song ratings to consider.
        @param overplay_ratings Users' overplay ratings to consider.
        @param settings Library parameters, for biasing levels.
        @param album_mode If true, compute/adjust probabilities for album
        selection, and avoid biasing in favor of albums with more tracks.
        @return The amount of bias in the biases table. */
    static BiasType compute_biases (const RetainedList <Song *> &candidates,
                                    std::vector<BiasType> &biases,
                                    const std::vector<UserData::Ratings *> &users_ratings,
                                    const std::vector<UserData::OverplayedList *> &overplay_ratings,
                                    const LibraryParameters &settings,
                                    const bool album_mode) {
        BiasType rating_scale = (BiasType (settings.rating_bias) * 0.09 + 1);
        BiasType recent_scale = (BiasType (settings.recent_bias) + 10) / 11;

        // Compute average age.  Matters for small numbers of songs;
        // If there are a large number number, assuming a week is good enough.
        time_t now = time (nullptr);
        BiasType average_age = 86400 * 7;  // One week
        if (candidates.size() < 2880) {
            // Small enough set to be worth doing the calculation.
            int count = 0;
            BiasType sum = 0;
            for (const auto song : candidates) {
                if (song->lastPlayed()) {
                    sum += (now - song->lastPlayed());
                    count++;
                }
            }
            if (count) {
                average_age = sum / BiasType (count);
            }
            // Don't use less than a minute for average song age
            if (average_age < 60)
                average_age = 60;
        }

        flog (LOG_FUNCTION (Log::BIASING), "Computed biases follow:");

        biases.clear();
        biases.reserve (candidates.size());
        BiasType total_bias = 0;
        for (const auto song : candidates) {
            // Calculate the average rating for this song.
            RatingAverager average;
            for (auto ratings : users_ratings) {
                average.add (ratings->get (song->songId(), Rating::UNRATED));
            }
            BiasType average_rating = average.rating (Rating::NEUTRAL);
            if (average_rating < 0.000001)
                average_rating = 0.000001;
            time_t age = (song->lastPlayed() ? now - song->lastPlayed() : FAR_FUTURE);
            BiasType relative_age = BiasType (age) / average_age;
            if (relative_age > 10)
                relative_age = 10;
            else if (relative_age < 0.000001)
                relative_age = 0.000001;
            BiasType bias = (powf (rating_scale, (average_rating - static_cast<double> (Rating::NEUTRAL)) / 4)
                             * powf (recent_scale, log10f (relative_age)));
            assert (bias >= 0);
            // When choosing by album, divide by number of tracks so albums
            // are equally likely to be chosen, instead of more tracks=more likely.
            if (album_mode) {
                assert (song->albumTrackCount());
                bias /= song->albumTrackCount();
            }

            // If anyone has rated this overplayed, refuse to play song.
            bool overplayed = false;
            for (auto ratings : overplay_ratings) {
                time_t until = ratings->get (song->songId(), 0);
                if (until > now) {
                    overplayed = true;
                    break;
                }
            }

            if (logging_enabled (Log::BIASING)) {
                fprintf (stderr,
                         "%-40.40s rate %5f age %5f bias %5f %s\n",
                         song->title().c_str(),
                         average_rating,
                         relative_age,
                         bias,
                         overplayed ? "(overplayed)" : "");
            }
            
            if (overplayed)
                bias = 0.0;
            // First song has range [0, biases [0])
            // Remaining have range [biases [n-1], biases [n])
            total_bias += bias;
            biases.push_back (total_bias);
        }
        return total_bias;
    }

    /** Pick some songs from a biased list at random.
        - A random number is chosen such that 0 <= `n` < `total_bias`.
        - We then search the bias values to find the index with range containing `n`.
        - The corresponding song is the next contestant.
        @param candidates The songs to pick from.
        @param biases The biases, in index locations matching `candidates`.
        @param total_bias The total range of bias from which to choose.
        @param requested_count The number of songs to pick.
        @return The songs picked.  At least 1 song will be returned.
	@warning May return fewer songs than requested, especially if
	`request_count ≅ candidates.size()`. */
    static RetainedList <Song *> get_biased_selections (const RetainedList <Song *> &candidates,
                                           const std::vector<BiasType> &biases,
                                           const BiasType total_bias,
                                           const SongList::size_type requested_count) {
        SongList::size_type retrieve = candidates.size();
        if (retrieve > requested_count)
            retrieve = requested_count;
        // Spin limit: Imagine 4 songs in the queue, 3 normal and one with a horrible bias.
        RetainedList <Song *> selections;
        for (SongList::size_type i = 0, spin_limit = retrieve + 4; i < retrieve && spin_limit > 0; i++, spin_limit--) {
            BiasType choice = fmod (random(), total_bias);
            // Binary search for the song with the chosen bias.
            SongList::size_type lower = 0;
            SongList::size_type upper = candidates.size() - 1;
            while (lower < upper) {
                unsigned long index = (lower + upper) >> 1;  // Middle
                assert (index >= 0 && index < candidates.size());
                if (choice >= biases[index]) {
                    lower = index + 1;
                } else {
                    upper = index;
                }
            }
            // Check for & reject duplicate choices
            bool duplicate = false;
            for (auto item : selections) {
                if (item == candidates[lower]) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                selections.push_back (candidates[lower]);
            }
        } // For the requested count (with spin limit)
        return selections;
    }

    /** Retrieve some random selections from the library for queue/playback.
        @param playlist The playlist selections should be from.
        @param users Users to consider when making selections.
        @param selectionMethod Manner in which to pick selections.
        @param settings Biasing parameters. */
    RetainedList <Song *> Foundation::getRandomSongs (PianodPlaylist *playlist,
                                         const UserList &users,
                                         Media::SelectionMethod selectionMethod,
                                         const LibraryParameters &settings) {
        assert (settings.rating_bias >= 1 && settings.rating_bias <= 100);
        assert (settings.recent_bias >= 1 && settings.recent_bias <= 100);
        assert (selectionMethod == Media::SelectionMethod::Song ||
                selectionMethod == Media::SelectionMethod::Artist ||
                selectionMethod == Media::SelectionMethod::Album);

        // Collect ratings from relevant users
        std::vector<UserData::Ratings *> users_ratings;
        std::vector<UserData::OverplayedList *> overplayed_ratings;
        users_ratings.reserve (users.size());
        overplayed_ratings.reserve (users.size());
        for (auto user : users) {
            auto ratings = UserData::Ratings::retrieve (user,
                                                        UserData::Key::TrackRatings,
                                                        playlist->source()->key());
            if (ratings) {
                users_ratings.push_back (ratings);
            }
            auto overplays = UserData::OverplayedList::retrieve (user,
                                                                 playlist->source()->key());
            if (overplays) {
                overplayed_ratings.push_back (overplays);
            }
        }

        RetainedList <Song *> candidates (getSongsForPlaylist (playlist));

        // Compute biases.
        std::vector<BiasType> biases;
        BiasType total_bias = compute_biases (candidates,
                                              biases,
                                              users_ratings,
                                              overplayed_ratings,
                                              settings,
                                              selectionMethod == Media::SelectionMethod::Album);

        RetainedList <Song *> response = get_biased_selections (candidates,
                                                   biases,
                                                   total_bias,
                                                   selectionMethod == Media::SelectionMethod::Song ? 4 : 1);
        if (!response.empty() && selectionMethod != Media::SelectionMethod::Song) {
            Song *song = response.front();
            assert (song);
            if (selectionMethod == Media::SelectionMethod::Album) {
                return retained_list_cast <Song *> (song->_album->songs());
            }
            assert (selectionMethod == Media::SelectionMethod::Artist);
            candidates = retained_list_cast <Song *> (song->_album->_artist->songs());
            total_bias = compute_biases (candidates, biases,
                                         users_ratings, overplayed_ratings,
                                         settings, false);
            response = get_biased_selections (candidates, biases, total_bias, 4);
        }
        return response;
    };


    /*
     *              Library Implementation
     */

    /** Construct a library.
        @param owner The source to which the media belongs.
        @param persistence If true, library indexes are written to long-term storage.
        @param song_allocator A function constructing songs of the source's type.
        @param album_allocator A function constructing albums of the source's type.
        @param artist_allocator A function constructing artists of the source's type.
        @param playlist_allocator A function constructing playlists of the source's type. */
    Library::Library (Media::Source * const owner,
                      const bool persistence,
                      const SongAllocator &song_allocator,
                      const AlbumAllocator &album_allocator,
                      const ArtistAllocator &artist_allocator,
                      const PlaylistAllocator &playlist_allocator)
    : Foundation (owner, persistence),
      artists (artist_allocator),
      albums (album_allocator),
      songs (song_allocator),
      playlists (playlist_allocator){
              // Derived classes should invoke restoreIndexFromFile;
              // object is not fully constituted here.
      };

    /** Remove albums & artists from the library that don't have any
        songs & albums respectively. */
    void Library::purge (void) {
        albums.purge ([] (const Album *album) -> bool { return (album->getUseCount() == 1 && album->empty()); });
        artists.purge ([] (const Artist *artist) -> bool { return (artist->getUseCount() == 1 && artist->empty()); });
    }

    /** Remove a playlist from the library.
        @param play The playlist to remove. */
    bool Library::removePlaylist (Playlist *play) {
        auto it = playlists.find (play->playlistId());
        assert (it != playlists.end());
        playlists.erase (it);
        unpopulatePlaylist (play);
        play->release();
        return true;
    };

    /** Retrieve a list of seeds for a playlist.
        @param playlist The playlist for which to return data.
        @return A list containing song, album, and artist seeds. */
    ThingieList Library::seedsForPlaylist (const Playlist *playlist) {
        ThingieList results;
        for (auto &item : playlist->seeds) {
            assert (!item.empty());
            // Type code is the first character of the ID.
            const MusicThingie::Type type = (MusicThingie::Type) item.at (0);
            MusicThingie *thing = nullptr;
            switch (type) {
                case MusicThingie::Type::Song:
                    thing = songs.getById (item);
                    break;
                case MusicThingie::Type::Album:
                    thing = albums.getById (item);
                    break;
                case MusicThingie::Type::Artist:
                    thing = artists.getById (item);
                    break;
                default:
                    assert (0);
                    break;
            }
            if (thing) {
                results.push_back (thing);
            }
        }
        return results;
    }

    /// Get a list of all songs in the library.
    RetainedList <Song *> Library::getAllSongs (void) {
        RetainedList <Song *> list;
        list.reserve (songs.size());
        for (auto item : songs) {
            list.push_back (item.second);
        }
        return list;
    }

    /// Get a list of all songs matching a filter.
    RetainedList <Song *> Library::getMatchingSongs (const Filter &criteria) {
        RetainedList <Song *> list;
        list.reserve (songs.size());
        for (auto &item : songs) {
            if (criteria.matches (item.second)) {
                list.push_back (item.second);
            }
        }
        return list;
    }

    /** Retrieve suggestions from the library.  This returns a mix
        of artists, albums, and songs.
        @param criteria Specifies the criteria for which to select
        matching data.
        @param what Describes a search manner.
        - For deep searches, all matching items will be returned.
        - For shallow searches, matches on an artist or album
        will skip their respective albums or songs. */
    ThingieList Library::getSuggestions (const Filter &criteria, SearchRange what) {
        bool exhaustive = deepSearch (what);
        ThingieList list;
        for (auto &artist : artists) {
            bool matches_artist = criteria.matches (artist.second);
            if (matches_artist)
                list.push_back (artist.second);
            if (!matches_artist || exhaustive) {
                for (auto album : artist.second->getAlbums()) {
                    bool matches_album = criteria.matches (album);
                    if (matches_album)
                        list.push_back (album);
                    if (!matches_album || exhaustive) {
                        for (auto song : album->getSongs()) {
                            if (criteria.matches (song)) {
                                list.push_back (song);
                            }
                        }
                    }
                }
            }
        }
        return list;
    }

    /// Get a list of all songs belonging to enabled playlists.
    RetainedList <Song *> Library::getMixSongs (void) {
        RetainedList <Song *> list;
        list.reserve (songs.size());
        for (auto item : songs) {
            if (item.second->_playlist && item.second->_playlist->enabled) {
                list.push_back (item.second);
            }
        }
        return list;
    }

    /** Get a list of all songs assigned to a playlist.
        @param play The playlist for which to retrieve songs.
        @param reassess If false, only assigned songs are returned.
        If true, songs in other playlists are also considered.
        @return A list of songs belonging to the playlist. */
    RetainedList <Song *> Library::getPlaylistSongs (const Playlist *play, bool reassess) {
        RetainedList <Song *> list;
        list.reserve (songs.size());
        for (auto item : songs) {
            if ((item.second->_playlist == play)
                || (reassess && item.second->_playlist && play->appliesTo (item.second))) {
                list.push_back (item.second);
            }
        }
        return list;
    }

    /** Find a playlist for a song, preferring enabled playlists.
        First, aim for enabled playlists;
        if not found then recurse and aim for disabled playlists. */
    Playlist *Library::findPlaylistForSong (Song *song, bool enabled) {
        for (auto playlist : playlists) {
            if (playlist.second->enabled == enabled) {
                if (playlist.second->appliesTo (song)) {
                    return playlist.second;
                }
            }
        }
        if (enabled == false)
            return nullptr;
        return findPlaylistForSong (song, false);
    }

    /** Review songs and assign them to a new candidate if they match. This is
        applicable when a playlist has been created or just been added to the mix.
        @param play The playlist to populate with songs.
        @param aggressive If true, songs with enabled playlists are  considered for reassignment.
        If false, only songs without a playlist or with a disabled playlist are considered. */
    void Library::populatePlaylist (Playlist *play, bool aggressive) {
        for (auto item : songs) {
            // Only grab songs that aren't assigned or whose playlist isn't enabled, unless we're aggressive
            if (aggressive || !item.second->_playlist || !item.second->_playlist->enabled) {
                // Don't waste time if we already own it
                if (item.second->_playlist != play) {
                    if (play->appliesTo (item.second)) {
                        item.second->_playlist = play;
                    }
                }
            }
        }
    }

    /** Reassign all a playlists' songs to some other playlist.
        Applicable when song has been removed from the mix, or
        playlist is about to be deleted.
        @warning May reassign current playlists, if no better ones are found
        and the current playlist is still in the playlist set. */
    void Library::unpopulatePlaylist (Playlist *play) {
        for (auto &item : songs) {
            // Reassign only if it belongs to this playlist
            if (item.second->_playlist == play) {
                item.second->_playlist = findPlaylistForSong (item.second);
            }
        }
    }
    /** Retrieve anything stored in the library by its ID.
        @param type The type of the thing.
        @param id The ID of the thing to retrieve.
        @return The thing, or a nullptr if not found. */
    MusicThingie *Library::getById (MusicThingie::Type type, const std::string &id) {
        MusicThingie *thing = nullptr;
        switch (type) {
            case MusicThingie::Type::Playlist: {
                MusicLibrary::Playlist *play = playlists.getById (id);
                if (!play)
                    return nullptr;
                populatePlaylist (play, true);
                thing = play;
                break;
            }
            case MusicThingie::Type::Artist:
                thing = artists.getById (id);
                break;
            case MusicThingie::Type::Album:
                thing = albums.getById (id);
                break;
            case MusicThingie::Type::Song:
                thing = songs.getById (id);
                break;
            default:
                // Asked for a suggestion, seed, etc.
                // Not used by this source, may nevertheless be asked for one.
                break;
        }
        return thing;
    }

    /** Retrieve all songs for a playlist, which may be a meta playlist */
    RetainedList <Song *> Library::getSongsForPlaylist (PianodPlaylist *playlist) {
        switch (playlist->playlistType()) {
            case PianodPlaylist::SINGLE:
            case PianodPlaylist::TRANSIENT:
                return retained_list_cast <Song *> (playlist->songs());
            case PianodPlaylist::MIX:
                return getMixSongs();
            case PianodPlaylist::EVERYTHING:
                return getAllSongs();
        }
        assert (!"Unreachable");
        return RetainedList <Song *>{};
    }

    /** Construct a playlist with an initial seed.
        @param name The name for the new playlist.
        @param type The manner in which to interpret the initial seed.
        @param from An initial seed.
        @return The new playlist. */
    PianodPlaylist *Library::createPlaylist (const std::string &name, MusicThingie::Type type, MusicThingie *from) {
        assert (from);
        assert (from->source() == source);
        assert (!playlists.getByName (name, this));
        if (playlists.getByName (name, this))
            throw CommandError (E_DUPLICATE);
        Playlist *play = playlists.addOrGetItem (name, this);
        play->selector = Filter::None;
        populatePlaylist (play);
        markDirty (IMPORTANT);
        try {
            play->seed (type, from, true);
        } catch (...) {
            removePlaylist (play);
            throw;
        }
        return play;
    }

    /** Construct a new smart playlist.
        @param name A name for the new playlist.
        @param filter Search criteria with which to construct the playlist.
        @return The newly constructed playlist. */
    PianodPlaylist *Library::createPlaylist (const std::string &name, const Filter &filter) {
        assert (filter.canPersist());
        assert (!playlists.getByName (name, this));
        Playlist *play = playlists.addOrGetItem (name, this);
        play->selector = filter;
        populatePlaylist (play);
        markDirty (IMPORTANT);
        return play;
    }

    /** Construct a temporary playlist.
        @param criteria Search criteria for the temporary playlist.
        @return The temporary playlist. */
    PianodPlaylist *Library::formTransientPlaylist (const Filter &criteria) {
        return new TransientPlaylist (this, criteria);
    }

    /** Persist the library's index into a file.
        This includes playlists and their seeds and match criteria.
        @param filename The name of the file. */
    bool Library::writeIndexToFile (const std::string &filename) const {
        try {
            Parsnip::Data all_artists{Parsnip::Data::List};
            for (const auto &artist : artists) {
                all_artists.push_back (artist.second->persist());
            }

            Parsnip::Data all_playlists{Parsnip::Data::List};
            for (const auto &playlist : playlists) {
                assert (playlist.second->selector.canPersist());
                all_playlists.push_back (playlist.second->persist());
            }

            Parsnip::Data media
                    = {Parsnip::Data::Dictionary, Music::Key::MediaArtists, std::move (all_artists)};
            persist (media);

            Parsnip::Data doc{Parsnip::Data::Dictionary,
                                    Music::Key::LibraryMedia,
                                    std::move (media),
                                    Music::Key::LibraryPlaylists,
                                    std::move (all_playlists)};
            return carefullyWriteFile (filename, doc);
        } catch (const std::exception &e) {
            flog (LOG_WHERE (Log::ERROR), "Could not serialize user data: ", e.what());
            return false;
        }
    }

    /** Restore the library's index, playlists, seeds and match
        criteria from a file.
        @param filename The name of the file from which to restore.
        @return True, or throws an exception. */
    bool Library::restoreIndexFromFile (const std::string &filename) {
        const Parsnip::Data index = retrieveJsonFile (filename);

        // Restore the regular tree
        MusicAutoReleasePool pool;
        for (const auto &artist : index[Music::Key::LibraryMedia][Music::Key::MediaArtists]) {
            auto newartist = artists.addOrGetItem (artist, this, Music::Key::ArtistName, Music::Key::ArtistId);
            if (!newartist)
                continue;
            for (const auto &album : artist[Music::Key::ArtistAlbums]) {
                auto newalbum = albums.addOrGetItem (album, newartist, Music::Key::AlbumName, Music::Key::AlbumId);
                if (!newalbum)
                    continue;
                for (const auto &track : album[Music::Key::AlbumSongs]) {
                    songs.addOrGetItem (track, newalbum, Music::Key::SongName, Music::Key::SongId);
                }
            }
        }
        // Restore compilations
        for (const auto &artist : index[Music::Key::LibraryMedia][Music::Key::MediaArtists]) {
            for (const auto &album : artist[Music::Key::ArtistAlbums]) {
                if (isCompilationAlbum (album)) {
                    for (const auto &track : album[Music::Key::AlbumSongs]) {
                        auto thesong = songs.getById (track, Music::Key::SongId);
                        assert (thesong);
                        // Check if compilation song has an artist; if so, set it up.
                        if (track.contains (Music::Key::ArtistId)) {
                            auto trackartist = artists.getById (track, Music::Key::ArtistId);
                            if (!trackartist)
                                flog (LOG_WHERE (Log::WARNING), "Could not find song artist for ", (*thesong)());
                            else
                                thesong->artist (trackartist);
                        }
                    }
                }
            }
        }
        // Restore the playlists
        for (const auto &playlist : index[Music::Key::LibraryPlaylists]) {
            playlists.addOrGetItem (playlist, this, Music::Key::PlaylistName, Music::Key::PlaylistId);
        }
        return true;
    }

}  // namespace MusicLibrary
