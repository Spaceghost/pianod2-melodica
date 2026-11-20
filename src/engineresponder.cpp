///
/// Response generators for audio engine.
/// @file       engineresponder.cpp - pianod
/// @author     Perette Barella
/// @date       2021-01-20
/// @copyright  Copyright 2014-2021 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <iomanip>
#include <sstream>

#include <parsnip/parsnip.h>

#include "fundamentals.h"
#include "lookup.h"
#include "utility.h"
#include "engine.h"
#include "musickeys.h"
#include "ratings.h"
#include "response.h"
#include "retainedlist.h"

/** Send the queue or history.  Allow negative indexes from either to refer to the other;
    0 refers to current track.  This makes implementing paging on clients easier because
    it won't have to use separate commands.
    @param conn The connection to which the list is being sent.
    @param options Predicate details from the command line.
    @param historical If true, looks toward history; if false, toward future. */
SongList AudioEngine::sendSongLists (PianodConnection &conn, const Parsnip::Data &options, bool historical) {
    SongList response;
    if (options.contains ("index")) {
        // Index given, send one specific song
        long index = options ["index"].asInteger();
        if (index == 0) {
            if (current_song) {
                response.push_back (current_song);
            } else {
                throw CommandError (E_WRONG_STATE);
            }
        } else {
            // Deal with negative indexes by negating the request type.
            if (index < 0) {
                historical = !historical;
                index = -index;
            }
            if (!historical && index == 1 && cueing_song.get()) {
                response.push_back (cueing_song);
            } else {
                SongList *src = &song_history;
                if (!historical) {
                    if (cueing_song)
                        index--;
                    src = &requests;
                    if (index > src->size()) {
                        index -= src->size();
                        src = &random_queue;
                    }
                }
                if (index > src->size()) {
                    throw CommandError (E_NOTFOUND);
                } else {
                    response.push_back ((*src) [index - 1]);
                }
            }
        }
    } else {
        // No index specified, send the whole list.
        if (historical) {
            return song_history;
        } else {
            if (cueing_song)
                response.push_back (cueing_song);
            response.join (requests);
            response.join (random_queue);
        }
    }
    return response;
}

/** Prepare a list of seeds for transmission.
    @param things The seeds.
    @param playlist The playlist to which the seeds belong.
    @return A data response including the seeds. */
DataResponse AudioEngine::constructSeedlist (const ThingieList &things, PianodPlaylist *playlist) {
    DataResponse response;
    ResponseGroup group;
    for (auto thing : things) {
        group.clear();
        auto json = Parsnip::Data::make_dictionary ({ { Music::Key::RatingNumeric, nullptr },
                                                      { Music::Key::ArtistSeed, false },
                                                      { Music::Key::AlbumSeed, false },
                                                      { Music::Key::SongSeed, false } });
        // Initailize group and add the song's rating
        auto song = thing->asSong();
        Rating song_rating = Rating::UNRATED;
        if (song) {
            group (thing, I_ATTACHED_SONG);
            // Only include a rating if it's tied to the station (no individual ratings).
            if (song->ratingScheme() == RatingScheme::OWNER) {
                song_rating = song->rating (nullptr);
            }
        } else {
            group (thing, I_ATTACHED_THING);
        }
        std::ostringstream rating;
        rating << std::fixed << std::setprecision (1) << ratingAsFloat (song_rating);
        Response::List list;
        list.push_back (RATINGS [song_rating]);
        list.push_back (rating.str());
        if (song_rating != Rating::UNRATED) {
            json [Music::Key::RatingNumeric] = ratingAsFloat (song_rating);
        } else {
            json [Music::Key::RatingNumeric] = nullptr;
        }
        json [Music::Key::RatingName] = RATINGS [song_rating];

        // Add seed use
        if (song) {
            if (song->type() == MusicThingie::Type::SongSeed
                || (song->type() == MusicThingie::Type::Song && playlist->canSeed (MusicThingie::Type::Song)
                    && playlist->seed (MusicThingie::Type::Song, song))) {
                json [Music::Key::SongSeed] = true;
                list.push_back ("seed");
            }
        } else {
            if (thing->asPlaylist()) {
                list.push_back ("playlistseed");
            } else if (thing->asAlbum()) {
                list.push_back ("albumseed");
                json [Music::Key::AlbumSeed] = true;
            } else {
                assert (thing->asArtist());
                list.push_back ("artistseed");
                json [Music::Key::ArtistSeed] = true;
            }
        }
        group (Response (I_RATING, std::move (list), std::move (json)));

        response.data (std::move (group));
    }
    return response;
}
