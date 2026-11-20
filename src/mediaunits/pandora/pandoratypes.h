///
/// Essential Pandora data types.
/// @file       mediaunits/pandora/pandoratypes.h - pianod project
/// @author     Perette Barella
/// @date       2020-03-23
/// @copyright  Copyright 2020-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>
#include <unordered_map>

#include <parsnip/parsnip.h>

#include "fundamentals.h"
#include "musictypes.h"
#include "encapmusic.h"
#include "retainer.h"

namespace Pandora {
    class Source;
    class Station;

    extern const Rating ThumbsDown;
    extern const Rating ThumbsUp;
    
    namespace Key {
        extern const char *TrackToken;
    }

    std::string pandora_to_music_id (const std::string &musicId);

    /// Class for Pandora songs.  Constructed from annotations.
    class Song : public PersistentSong {
    protected:
        inline Source * const pandora() const {
            return reinterpret_cast<Source * const > (source());
        }

    public:
        // Constructors
        Song (Source * const owner, MusicThingie::Type type, const Parsnip::Data &message);

        // API support
        virtual RatingScheme ratingScheme (void) const override;
        virtual RESPONSE_CODE rate (Rating value, User *user) override;
        virtual Rating rating (const User *user) const override;

    protected:
        Song (Source * const owner);
    };

    /// Playable song: a song with playback information.
    class PlayableSong : public Song {
        friend Source;
        friend Song;

    protected:
        inline Source * const pandora() const {
            return reinterpret_cast<Source * const > (source());
        }

    private:
        std::string audio_url;
        std::string track_token;
        double audio_gain;

    public:
        PlayableSong (Source * const owner, const Parsnip::Data &message);
        PlayableSong (Source * const owner, const Parsnip::Data &message, bool persist_flag);


        // API support
        virtual RESPONSE_CODE rateOverplayed (User *) override;
        virtual bool canSkip (time_t *whenAllowed = nullptr) override;
        virtual bool canQueue() const override;

        inline const std::string &trackToken() const {
            return track_token;
        }
    };
    
    /** A song that contains rating information.  Used to move feedback
        IDs from JSON parsing to a station, which manages it long-term.
        The field is unused after that. */
    class SongRating : public EncapsulatedSong {
        friend class Station;
        Rating song_rating{Rating::UNRATED};
    public:
        SongRating (Station *owner, const Parsnip::Data &message);
        virtual RatingScheme ratingScheme (void) const override;
        virtual RESPONSE_CODE rate (Rating value, User *user) override;
        virtual Rating rating (const User *user) const override;
    };

    /// A song contructed from station seed information.
    class SongSeed : public EncapsulatedSong {
        friend class Station;
    public:
        SongSeed (Station *owner, const Parsnip::Data &message);
    };
    
    /// A song constructed from a suggestion
    class SongSuggestion : public Song {
    public:
        SongSuggestion (Source * const owner, const Parsnip::Data &message);
    };

    class Advert : public EncapsulatedSong {
        friend class Source;
    private:
        Retainer <EncapsulatedPlaylist *> ad_station;
        double audio_gain;
        std::string audio_url;
        std::vector<std::string> ad_tokens;
    public:
        Advert (Source * const owner, const Parsnip::Data &message, Station *station);
        static Retainer <Advert *> construct (Station *station, const std::string &track_info);

        // API support
        virtual RatingScheme ratingScheme (void) const override;
        virtual RESPONSE_CODE rate (Rating value, User *user) override;
        virtual Rating rating (const User *user) const override;
        virtual RESPONSE_CODE rateOverplayed (User *) override;
        virtual bool canSkip (time_t *whenAllowed = nullptr) override;
        virtual bool mustPlay () const override;
    };

    /// Class for Pandora artists.  Constructed from annotations.
    class Artist : public PersistentArtist {
    protected:
        inline Source * const pandora() const {
            return reinterpret_cast<Source * const > (source());
        }
    public:
        Artist (Source * const owner);
        Artist (Source * const owner, MusicThingie::Type type, const Parsnip::Data &message);
   };

    /// An artist contructed from search results.
    class ArtistSuggestion : public Artist {
    public:
        ArtistSuggestion (Source * const owner, const Parsnip::Data &message);
    };

    /// A artist contructed from station seed information.
    class ArtistSeed : public EncapsulatedArtist {
        friend class Station;
    public:
        ArtistSeed (Source *const owner, const Parsnip::Data &message);
    };

    /// A genre constructed from a suggestion.
    class GenreSuggestion : public PersistentMetaPlaylist {
    public:
        GenreSuggestion (Source * const owner, const Parsnip::Data &message);
        GenreSuggestion (Source * const owner, MusicThingie::Type type, const Parsnip::Data &message);
    };
    
    /// A class for Pandora genre seeds.
    class GenreSeed : public MetaPlaylist {
        friend class Station;
    public:
        GenreSeed (Source *owner, const Parsnip::Data &message);
    };

}  // namespace Pandora

