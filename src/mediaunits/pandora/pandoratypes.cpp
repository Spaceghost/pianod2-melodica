///
/// Datatypes to support pianod/libpiano interface for Pandora player.
/// @file       mediaunits/pandora/pandoratypes.cpp - pianod
/// @author     Perette Barella
/// @date       2014-11-30
/// @copyright  Copyright 2014-2023 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <string>
#include <exception>

#include <cassert>

#include <errno.h>

#include "musickeys.h"

#include "pandoratypes.h"
#include "pandora.h"

namespace Pandora {

    const Rating ThumbsDown = Rating::POOR;  ///< Rating value for negative Pandora feedback.
    const Rating ThumbsUp = Rating::GOOD;    ///< Rating value for positive Pandora feedback.

    const char *Key::TrackToken = "trackToken";

    /*
     *                  Pandora Song types
     */

    /** Round the value to Pandora's thumbs.
        @throw CommandError/unsupported media value if an inappropriate
        rating. */
    static Rating rounded_rating (const Rating rating) {
        if (rating <= ThumbsDown) {
            return ThumbsDown;
        } else if (rating >= ThumbsUp) {
            return ThumbsUp;
        } else if (rating == Rating::NEUTRAL) {
            return Rating::UNRATED;
        } else {
            throw CommandError (E_MEDIA_VALUE);
        }
    }

    Song::Song (Source *const owner) : PersistentSong (owner){};

    /** Restore a song from the persistence file.
        @param owner The source this song belongs to.
        @param type The MusicThingie type of the item being reconstructed.
        @param message The details from which to construct the song. */
    Song::Song (Source *const owner, MusicThingie::Type type, const Parsnip::Data &message)
    : PersistentSong (owner, type, message) {
        if (message.contains (Music::Key::PlaylistId)) {
            const std::string &plid = message [Music::Key::PlaylistId].asString();
            playlist (owner->getStationByStationId (plid));
            if (!playlist()) {
                flog (LOG_WHERE (Log::WARNING), "Unknown station: ", plid);
            }
        }
    };

    RatingScheme Song::ratingScheme (void) const {
        return RatingScheme::OWNER;
    };

    RESPONSE_CODE Song::rate (Rating value, User *user) {
        assert (isEditableBy (user));

        if (!playlist()) {
            throw CommandError (E_PLAYLIST_REQUIRED);
        }

        Rating new_rating = rounded_rating (value);
        RESPONSE_CODE result = (value == new_rating || new_rating == Rating::UNRATED ? S_OK : S_ROUNDING);

        Station *station = static_cast<Station *> (playlist());
        station->refreshSeedsAndRatings();
        SongRating *rating = station->feedback.getDetail (this);
        Rating prior_rating = (rating ? rating->rating (user) : Rating::UNRATED);

        // If the song already has the desired rating, skip the work.
        if (new_rating == prior_rating) {
            return result;
        }

        Status status;
        if (new_rating == Rating::UNRATED) {
            return rating->rate (new_rating, user);
        }
        PlayableSong *playable = dynamic_cast<PlayableSong *> (this);
        if (!playable) {
            throw CommandError (E_WRONG_STATE, "Song not recently played");
        }
        station->takePossession();
        RequestAddFeedback add_feedback (station, playable->trackToken(), new_rating == ThumbsUp);
        status = pandora()->executeRequest (add_feedback);
        if (status == Status::Ok) {
            station->feedback.storeDetail (this, add_feedback.getFeedback());
            pandora()->library.add (add_feedback.getFeedback());
            return result;
        }
        throw CommandError (E_NAK, status_strerror (status));
    }

    Rating Song::rating (const User *user) const {
        if (!playlist()) {
            return Rating::UNRATED;
        }
        Station *station = static_cast<Station *> (playlist());
        // Ratings request can be called from current-song announcements.  Don't throw.
        try {
            station->refreshSeedsAndRatings();
        } catch (CommandError &) {
            // Communication error; ignore it
        }
        SongRating *rating = station->feedback.getDetail (this);
        return (rating ? rating->rating (user) : Rating::UNRATED);
    }

    /*
     *                  Song suggestions
     */

    SongSuggestion::SongSuggestion (Source *const owner, const Parsnip::Data &message) : Song (owner) {
        songId (message ["musicToken"].asString());
        artist (message ["artistName"].asString());
        title (message ["songName"].asString());
    }

    /*
     *                  Playable Song type
     */

    /** Construct a Song from a Pandora annotation message.
        @param owner The source this song belongs to.
        @param message The details from which to construct the song. */
    PlayableSong::PlayableSong (Source *const owner, const Parsnip::Data &message) : Song (owner) {
        songId (message ["musicId"].asString());
        artist (message ["artistName"].asString());
        albumTitle (message ["albumName"].asString());
        title (message ["songName"].asString());
        coverArtUrl (message ["albumArtUrl"].asString());
        duration (message ["trackLength"].asInteger());
        infoUrl (message ["songDetailUrl"].asString());
        audio_url = message ["audioUrlMap"][owner->userFeatures().hifi_audio_encoding ? "highQuality" : "mediumQuality"]
                            ["audioUrl"]
                                    .asString();
        track_token = message [Key::TrackToken].asString();
        // Gain is stored in a string for some reason...
        audio_gain = message ["trackGain"].makeFlexible().asDouble();

        Station *station = owner->getStationByStationId (message ["stationId"].asString());
        if (station) {
            playlist (station);
        } else {
            flog (LOG_WHERE (Log::WARNING), "Unknown station: ", message ["stationId"].asString());
        }

        // Set expiration time, managed way up in a base class.
        expiration = time (nullptr) + owner->connectionParams()->playlist_expiration;
    }

    RESPONSE_CODE PlayableSong::rateOverplayed (User *) {
        RequestAddTiredSong mark_overplayed (pandora(), track_token);
        Status status = pandora()->executeRequest (mark_overplayed);
        if (status != Status::Ok) {
            throw CommandError (E_NAK, status_strerror (status));
        }
        return S_OK;
    }

    /** Determine if a skip is allowed, and if so, record one.
        A skip requires both the station and the source allow skips.
        @param when_allowed If not allowed, set to the time when one will be.
        @return True if allowed, false otherwise. */
    bool PlayableSong::canSkip (time_t *when_allowed) {
        time_t source_when;
        time_t station_when{ 0 };
        bool source_can;
        bool station_can = true;
        source_can = pandora()->skips.canSkip (&source_when);
        Station *station = static_cast<Station *> (playlist());
        if (station) {
            station_can = station->skips.canSkip (&station_when);
        }
        if (source_can && station_can) {
            pandora()->skips.skip();
            if (station) {
                station->skips.skip();
            }
            return true;
        }
        // We need both to skip, so pick the further-out time.
        *when_allowed = (source_when > station_when ? source_when : station_when);
        return false;
    }

    bool PlayableSong::canQueue() const {
        return pandora()->userFeatures().replays;
    };

    /*
     *                  Pandora Song Type Variations
     */

    /** Construct a Song from a station seed list.
        @param owner The source this song belongs to.
        @param message The details from which to construct the song. */
    SongSeed::SongSeed (Station *owner, const Parsnip::Data &message)
    : EncapsulatedSong (owner->pandora(), MusicThingie::Type::SongSeed) {
        songId (message ["seedId"].asString());
        artist (message ["artistName"].asString());
        title (message ["songName"].asString());
        coverArtUrl (message ["artUrl"].asString());
        playlist (owner);
    };

    /** Construct a Song from a list of station feedback.
        @param owner The source this song belongs to.
        @param message The details from which to construct the song. */
    SongRating::SongRating (Station *owner, const Parsnip::Data &message) : EncapsulatedSong (owner->pandora()) {
        songId (message ["feedbackId"].asString());
        artist (message ["artistName"].asString());
        title (message ["songName"].asString());
        coverArtUrl (message ["albumArtUrl"].asString());
        song_rating = (message ["isPositive"].asBoolean() ? ThumbsUp : ThumbsDown);
        playlist (owner);
    }

    RatingScheme SongRating::ratingScheme (void) const {
        return RatingScheme::OWNER;
    };

    RESPONSE_CODE SongRating::rate (Rating value, User *user) {
        assert (isEditableBy (user));
        assert (playlist());

        Rating new_rating = rounded_rating (value);
        if (new_rating == song_rating) {
            return S_OK;
        } else if (new_rating != Rating::UNRATED) {
            throw CommandError (E_WRONGTYPE, "Cannot rate rating");
        }

        Station *station = static_cast<Station *> (playlist());
        RequestDeleteFeedback delete_feedback (station, songId());
        Source *pandora = static_cast <Source *> (source());
        Status status = pandora->executeRequest (delete_feedback);
        if (status == Status::Ok) {
            station->feedback.erase (this);
            pandora->library.erase (this);
            return S_OK;
        }
        throw CommandError (E_NAK, status_strerror (status));
    }

    Rating SongRating::rating (const User *) const {
        return song_rating;
    }

    /*
     *                  Pandora Adverts
     */

    Retainer <Advert *> Advert::construct (Station *station, const std::string &track_info) {
        RetrieveAdvert retriever (station, track_info);
        Status status = station->pandora()->executeRequest (retriever);
        return (status == Status::Ok ? retriever.getAdvert() : nullptr);
    }

    Advert::Advert (Source *const owner, const Parsnip::Data &message, Station *station)
    : EncapsulatedSong (owner, Type::Song) {
        songId ("ADVERT");
        artist (message ["companyName"].asString());
        albumTitle ("Buying Frenzy!!!");
        title (message ["title"].asString());
        coverArtUrl (message ["imageUrl"].asString());
        message ["trackGain"].makeFlexible();
        audio_gain = message ["trackGain"].asDouble();
        audio_url = message ["audioUrlMap"][owner->userFeatures().hifi_audio_encoding ? "highQuality" : "mediumQuality"]
                            ["audioUrl"]
                                    .asString();
        infoUrl (message ["clickThroughUrl"].asString());
        ad_tokens = message ["adTrackingTokens"].as<std::vector<std::string>>();
        ad_station = station;
    }

    RatingScheme Advert::ratingScheme (void) const {
        return RatingScheme::NOBODY;
    };

    RESPONSE_CODE Advert::rate (Rating value, User *user) {
        assert (!"Attempt to rate advertisement");
        return E_NAK;
    }

    Rating Advert::rating (const User *user) const {
        assert (!"Attempt to get rating of advertisement");
        return Rating::UNRATED;
    }

    RESPONSE_CODE Advert::rateOverplayed (User *) {
        return E_NAK;
    }

    bool Advert::canSkip (time_t *whenAllowed) {
        return false;
    }

    bool Advert::mustPlay() const {
        return true;
    }

    /*
     *                  Pandora Artist types
     */

    Artist::Artist (Source *const owner) : PersistentArtist (owner, Type::Artist){};

    Artist::Artist (Source *const owner, MusicThingie::Type type, const Parsnip::Data &message)
    : PersistentArtist (owner, type, message) {
    }

    /** Construct an artist from a Pandora annotation message.
        @param owner The source this song belongs to.
        @param message The details from which to construct the song. */
    ArtistSeed::ArtistSeed (Source *const owner, const Parsnip::Data &message) : EncapsulatedArtist (owner, Type::Artist) {
        artistId (message ["seedId"].asString());
        artist (message ["artistName"].asString());
    }

    ArtistSuggestion::ArtistSuggestion (Source *const owner, const Parsnip::Data &message) : Artist (owner) {
        artistId (message ["musicToken"].asString());
        artist (message ["artistName"].asString());
    }

    /*
     *                  Pandora Genre Station and Seed Types
     */

    /** Construct a genre suggestion from search/annotation result.
        @param owner The source the genre will belong to.
        @param message The annotation record for the genre. */
    GenreSuggestion::GenreSuggestion (Source *const owner, const Parsnip::Data &message)
    : PersistentMetaPlaylist (owner, Type::PlaylistSuggestion) {
        if (message.contains ("stationId")) {
            // Genre station list uses "stationId"
            playlistId (message ["stationId"].asString());
        } else {
            // Search results use "musicToken"
            playlistId (message ["musicToken"].asString());
        }
        playlistName (message ["stationName"].asString());
    }

    GenreSuggestion::GenreSuggestion (Source *const owner, MusicThingie::Type type, const Parsnip::Data &message)
    : PersistentMetaPlaylist (owner, type, message) {
    }

    /** Construct a genre seed from seed information.
        @param owner The source the genre will belong to.
        @param message The seed record for the genre. */
    GenreSeed::GenreSeed (Source *owner, const Parsnip::Data &message)
    : MetaPlaylist (owner, Type::PlaylistSeed) {
        playlistId (message ["seedId"].asString());
        playlistName (message ["genreName"].asString());
    }

}  // namespace Pandora
