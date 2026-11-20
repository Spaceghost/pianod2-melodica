///
/// Pandora messages.
/// @file       mediaunits/pandora/pandoramessages.cpp - pianod project
/// @author     Perette Barella
/// @date       2020-03-24
/// @copyright  Copyright 2020-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>
#include <memory>

#include <parsnip/parsnip.h>

#include "musictypes.h"
#include "pandoratypes.h"
#include "pandorastation.h"
#include "pandoraparameters.h"

namespace Pandora {
    class Communication;
    class Song;
    class Artist;
    class Station;

    enum class RequestOption { NONE = 0, DEBUG = 1, TLS = 2, UNENCRYPTED = 4 };
    static inline constexpr RequestOption operator& (const RequestOption a, const RequestOption b) {
        return RequestOption (int (a) & int (b));
    }
    static inline constexpr RequestOption operator| (const RequestOption a, const RequestOption b) {
        return RequestOption (int (a) | int (b));
    }

    /** Pandora request class.  This provides a framework for assembling
        requests and decoding responses. */
    class Request {
    public:
        using Option = RequestOption;
    protected:
        Source * const source;
        Parsnip::Data request_message;

    private:
        Option flags;
        const std::string endpoint;  /// RPC method name or REST API endpoint.

    protected:
        Request (Source * const src, const char *ep, Option options = Option::NONE);

    public:
        inline const std::string &url() const { return endpoint; };
        inline bool debug () const { return (flags & Option::DEBUG) == Option::DEBUG; };
        inline bool tlsEncrypt () const { return (flags & Option::TLS) == Option::TLS; };
        inline bool blowfishEncrypt () const { return (flags & Option::UNENCRYPTED) != Option::UNENCRYPTED; };

        Parsnip::Data &retrieveRequestMessage();

        /** Decode a response message, already converted to Parsnip Data,
            into some internal representation.
            @param message The message to be decoded. */
        virtual void extractResponse (const Parsnip::Data &message) = 0;
        
    };

    /// Pandora request class.  A request whose response is empty/unimportant.
    class RequestWithNoData : public Request {
    protected:
        RequestWithNoData (Source * const src, const char *ep, Option flags = Option::NONE);
    };

    /// Pandora request class.  A request whose response is empty/unimportant.
    class Notification : public Request {
    protected:
        Notification (const char *ep, Option flags = Option::NONE);
        virtual void extractResponse (const Parsnip::Data &message) override;
    };

    /// Pandora message: Retrieve station list
    class RequestStationList : public RequestWithNoData {
        using MixIdList = std::vector<std::string>;
        StationList stations;
        std::string checksum;
        Retainer<Station *> quickmix;
        Retainer<Station *> everything;
        MixIdList mix_stations;

    public:
        RequestStationList (Source * const src);
        virtual void extractResponse (const Parsnip::Data &message) override;
        inline const StationList &getStations() const {
            return stations;
        }
        inline Retainer<Station *> getQuickMix() const {
            return quickmix;
        }
        inline Retainer<Station *> getEverythingStation() const {
            return everything;
        }
        inline MixIdList getMixStationList() const {
            return mix_stations;
        }
        inline std::string getChecksum() const {
            return checksum;
        }
    };

    /// Pandora message: Retrieve station list
    class RequestStationListChecksum : public RequestWithNoData {
        std::string checksum;

    public:
        static constexpr bool GenreStations = true;
        RequestStationListChecksum(bool genre_stations = false);
        virtual void extractResponse (const Parsnip::Data &message) override;
        inline std::string getChecksum() const {
            return checksum;
        }
    };

    /// Pandora message: Retrieve station list
    class RequestGenreStationList : public RequestWithNoData {
        std::unique_ptr<PlaylistList> stations;

    public:
        RequestGenreStationList (Source * const src);
        virtual void extractResponse (const Parsnip::Data &message) override;
        inline const PlaylistList &getStations() const {
            assert (stations);
            return *stations;
        }
    };

    /// Pandora message: set quickmix stations.
    class SetQuickMixStations : public Notification {
        PlaylistList mix_stations;

    public:
        /// Request shuffle station list and shuffle station info.
        SetQuickMixStations (const StationList &mix_stations);
    };

    /// Pandora message: create a station
    class RequestCreateStation : public Request {
        Retainer <Station *> new_station;

    public:
        RequestCreateStation (Source * const src, MusicThingie::Type type, const std::string &music_id);
        virtual void extractResponse (const Parsnip::Data &message) override;
        inline Station *getStation () const {
            assert (new_station);
            return new_station.get();
        }
    };

    /// Pandora message: delete a station
    class RequestRemoveStation : public Notification {
    public:
        RequestRemoveStation (Station *station);
    };

    /// Pandora message: transform a shared station into a personal station
    class RequestTransformStation : public Request {
        Retainer <Station *> station;
    public:
        RequestTransformStation (Station *sta);
        virtual void extractResponse (const Parsnip::Data &message) override;
    };

    /// Pandora message: rename a station
    class RequestRenameStation : public Request {
        Retainer <Station *> station;
    public:
        RequestRenameStation (Station *sta, const std::string &name);
        virtual void extractResponse (const Parsnip::Data &message) override;
    };

    /// Pandora message: Retrieve station seeds
    class RequestStationDetails : public Request {
    protected:
        Retainer <Station *> station;
        RetainedList <SongSeed *> song_seeds;
        RetainedList <ArtistSeed *> artist_seeds;
        RetainedList <GenreSeed *> genre_seeds;
        RetainedList <SongRating *> positive_feedback;
        RetainedList <SongRating *> negative_feedback;
        RetainedList <SongRating *> extractFeedback (const Parsnip::Data &list);

    public:
        RequestStationDetails (Station *sta);
        virtual void extractResponse (const Parsnip::Data &message) override;
        
        inline RetainedList<SongSeed *> getSongSeeds() {
            return std::move (song_seeds);
        }
        inline RetainedList <ArtistSeed *> getArtistSeeds() {
            return std::move (artist_seeds);
        }
        inline RetainedList <GenreSeed *> getGenreSeeds() {
            return std::move (genre_seeds);
        }
        inline RetainedList <SongRating *> negativeFeedback() {
            return std::move (negative_feedback);
        }
        inline RetainedList <SongRating *> positiveFeedback() {
            return std::move (positive_feedback);
        }
    };

    /// Pandora message: Add or remove station seeds
    class RequestAddStationSeed : public Request {
        Retainer <Station *> station;
        std::string seed_id;
        Retainer <GenreSeed *> genre_seed;
        Retainer <ArtistSeed *> artist_seed;
        Retainer <SongSeed *> song_seed;
    public:
        RequestAddStationSeed (Station *sta, const std::string &music_id);
        virtual void extractResponse (const Parsnip::Data &message) override;
        inline const std::string &getSeedId () const {
            assert (!seed_id.empty());
            return seed_id;
        }
        inline GenreSeed *getGenreSeed () const {
            assert (genre_seed);
            return genre_seed.get();
        }
        inline ArtistSeed *getArtistSeed () const {
            assert (artist_seed);
            return artist_seed.get();
        }
        inline SongSeed *getSongSeed () const {
            assert (song_seed);
            return song_seed.get();
        }
    };

    /// Pandora message: Add or remove station seeds
    class RequestRemoveStationSeed : public Notification {
    public:
        RequestRemoveStationSeed (Station *station, const std::string &seed_id);
    };


    /// Pandora message: Provide feedback for a song on a station.
    class RequestAddTiredSong : public Notification {
    public:
        RequestAddTiredSong (Source * const , const std::string &track_token);
    };

    /// Pandora message: Provide feedback for a song that has played on a station.
    class RequestAddFeedback : public Request {
        Retainer <Station *> station;
        Retainer <SongRating *> feedback;

    public:
        RequestAddFeedback (Station *sta, const std::string &track_token, bool positive);
        virtual void extractResponse (const Parsnip::Data &message)  override;
        inline SongRating *const getFeedback() const {
            assert (feedback);
            return feedback.get();
        }
    };

    /// Pandora message: Remove feedback for a song on a station.
    class RequestDeleteFeedback : public Notification {
    public:
        RequestDeleteFeedback (Station *station, const std::string &feedback_id);
    };

    /// Pandora message: Retrieve list of music to play from a station.
    class RequestQueueTracks : public Request {
        Retainer <Station *> station;
        std::unique_ptr<SongList> results;
    public:
        RequestQueueTracks (Station *sta);
        virtual void extractResponse (const Parsnip::Data &message) override;
        inline const SongList &getResponse() {
            assert (results);
            return *results;
        }
    };

    // Pandora message: Search request.
    class SearchRequest : public Request {
        ThingieList results;

    public:
        SearchRequest (Source * const src, const std::string &query, const bool exact);
        virtual void extractResponse (const Parsnip::Data &message) override;
        inline const ThingieList &getResponse() {
            return results;
        }
    };


    // Pandora message: retrieve advertisements.
    class RetrieveAdvert : public Request {
        Retainer <Station *> station;
        Retainer<Advert *> advert;
    public:
        RetrieveAdvert (Station *sta, const std::string &ad_token);
        virtual void extractResponse (const Parsnip::Data &message) override;
        inline Advert *getAdvert() {
            return advert.get();
        }
    };

    // Pandora message: Advert playing
    class AdvertPlayedNotification : public Notification {
    public:
        AdvertPlayedNotification (PianodPlaylist *station, const std::vector<std::string> &ad_tokens);
    };
}  // namespace Pandora
