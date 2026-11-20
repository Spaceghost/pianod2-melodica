///
/// Pandora messages.
/// @file       mediaunits/pandora/pandoramessages.h - pianod project
/// @author     Perette Barella
/// @date       2020-03-24
/// @copyright  Copyright 2020-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <string>
#include <functional>

#include "logging.h"
#include "fundamentals.h"
#include "musictypes.h"
#include "encapmusic.h"

#include "pandoratypes.h"
#include "pandoramessages.h"
#include "pandora.h"

namespace Pandora {
    Request::Request (Source * const src, const char *ep, Option options) : source (src), flags (options), endpoint (ep){};

    /** Retrieve the Parsnip Data object with the request message.
        It would be better off returned const, but efficiency.
        @return Data containing serializable form of request. */
    Parsnip::Data &Request::retrieveRequestMessage() {
        assert (!request_message.isNull());
        return request_message;
    }

    /*
     *              Requests with no added data
     */
    RequestWithNoData::RequestWithNoData (Source * const src, const char *ep, Option flags) : Request (src, ep, flags) {
        request_message = Parsnip::Data::make_dictionary ({});
    };

    /*
     *              Notifications
     */

    Notification::Notification (const char *ep, Option flags) : Request (nullptr, ep, flags){};

    void Notification::extractResponse (const Parsnip::Data &message) {
        // Response is empty or unimportant.
    }

    /*
     *              Retrieve Station List
     */

    RequestStationList::RequestStationList (Source * const src) : RequestWithNoData (src, "user.getStationList"){};

    void RequestStationList::extractResponse (const Parsnip::Data &message) {
        checksum = message ["checksum"].asString();
        MusicAutoReleasePool pool;
        for (const auto &station_info : message ["stations"]) {
            try {
                if (station_info ["isQuickMix"].asBoolean()) {
                    mix_stations = station_info ["quickMixStationIds"].as<std::vector<std::string>>();
                    quickmix = new QuickMixStation (source, station_info);
                    everything = new MixEverythingStation (source, station_info);
                } else {
                    stations.push_back (new Station (source, station_info));
                }
            } catch (const Parsnip::NoSuchKey &ex) {
                flog (LOG_WHERE (Log::ERROR), "Unable to decode musicthingie: ", ex.what());
                station_info.dumpJson ("Problem station");
            }
        }
    }

    /*
     *              Retrieve Station List Checksum
     */

    RequestStationListChecksum::RequestStationListChecksum (bool genre_stations)
    : RequestWithNoData (nullptr,
                         genre_stations ? "station.getGenreStationsChecksum" : "user.getStationListChecksum"){};

    void RequestStationListChecksum::extractResponse (const Parsnip::Data &message) {
        checksum = message ["checksum"].asString();
    }

    /*
     *              Retrieve Genre Station List
     */

    RequestGenreStationList::RequestGenreStationList (Source * const src)
    : RequestWithNoData (src, "station.getGenreStations"){};

    void RequestGenreStationList::extractResponse (const Parsnip::Data &message) {
        MusicAutoReleasePool pool;
        stations.reset (new PlaylistList());

        for (const Parsnip::Data &category : message ["categories"]) {
            std::string genre = category ["categoryName"].asString();
            for (const Parsnip::Data &station : category ["stations"]) {
                try {
                    GenreSuggestion *suggestion = new GenreSuggestion (this->source, station);
                    suggestion->genre (genre);
                    this->stations->push_back (suggestion);
                } catch (const Parsnip::NoSuchKey &ex) {
                    flog (LOG_WHERE (Log::ERROR), "Unable to decode genre station: ", ex.what());
                    station.dumpJson ("Problem station");
                }
            }
        }
    }

    /*
     *              Adjust the quick mix
     */
    SetQuickMixStations::SetQuickMixStations (const StationList &mix_stations) : Notification ("user.setQuickMix") {
        Parsnip::Data ids{ Parsnip::Data::List };
        for (const Station *station : mix_stations) {
            ids.push_back (station->playlistId());
        }
        request_message = Parsnip::Data{ Parsnip::Data::Dictionary, "quickMixStationIds", std::move (ids) };
    };

    /*
     *              Create a station
     */

    RequestCreateStation::RequestCreateStation (Source * const src, MusicThingie::Type type, const std::string &music_id)
    : Request (src, "station.createStation") {
        // clang-format off
        request_message = Parsnip::Data::make_dictionary ({
            { "musicType", type == MusicThingie::Type::Artist ? "artist" : "song" },
            { "musicToken", music_id }
        });
        // clang-format on
    }

    void RequestCreateStation::extractResponse (const Parsnip::Data &message) {
        new_station = new Station (source, message);
    }

    /*
     *              Remove a station
     */

    RequestRemoveStation::RequestRemoveStation (Station *station) : Notification ("station.deleteStation") {
        assert (!station->getToken().empty());
        assert (station->playlistType() == PianodPlaylist::PlaylistType::SINGLE);
        request_message = Parsnip::Data::make_dictionary ({ { "stationToken", station->getToken() } });
    }

    /*
     *              Transform a shared station to a personal station
     */

    RequestTransformStation::RequestTransformStation (Station *sta)
    : Request (sta->pandora(), "station.transformSharedStation"),
      station (sta) {
        assert (!station->getToken().empty());
        assert (station->playlistType() == PianodPlaylist::PlaylistType::SINGLE);
        request_message = Parsnip::Data::make_dictionary ({ { "stationToken", station->getToken() } });
    }

    void RequestTransformStation::extractResponse (const Parsnip::Data &message) {
        station->is_shared = false;
    }

    /*
     *              Rename a station
     */

    RequestRenameStation::RequestRenameStation (Station *sta, const std::string &new_name)
    : Request (sta->pandora(), "station.renameStation"),
      station (sta) {
        assert (!station->getToken().empty());
        assert (!new_name.empty());
        assert (station->playlistType() == PianodPlaylist::PlaylistType::SINGLE);
        request_message = Parsnip::Data::make_dictionary (
                { { "stationToken", station->getToken() }, { "stationName", new_name } });
    }

    void RequestRenameStation::extractResponse (const Parsnip::Data &message) {
        station->playlistName (message ["stationName"].asString());
    }

    /*
     *              Retrieve Station Details (seeds and a few other tidbits)
     */

    RequestStationDetails::RequestStationDetails (Station *sta)
    : Request (sta->pandora(), "station.getStation"),
      station (sta) {
        // clang-format off
        request_message = Parsnip::Data::make_dictionary ({
            {"stationToken", station->getToken()},
            {"includeExtendedAttributes", true}
        });
        // clang-format on
    }

    RetainedList <SongRating *> RequestStationDetails::extractFeedback (const Parsnip::Data &list) {
        RetainedList <SongRating *> ratings;
        for (const auto &thumb_rating : list) {
            try {
                ratings.push_back (new SongRating (station.get(), thumb_rating));
            } catch (const Parsnip::Exception &ex) {
                flog (LOG_WHERE (Log::ERROR), "Unable to decode feedback: ", ex.what());
                thumb_rating.dumpJson ("Problem feedback");
            }
        }
        return ratings;
    }

    void RequestStationDetails::extractResponse (const Parsnip::Data &message) {
        negative_feedback = extractFeedback (message ["feedback"]["thumbsDown"]);
        positive_feedback = extractFeedback (message ["feedback"]["thumbsUp"]);

        for (const auto &song_seed : message ["music"]["songs"]) {
            try {
                song_seeds.push_back (new SongSeed (station.get(), song_seed));
            } catch (const Parsnip::Exception &ex) {
                flog (LOG_WHERE (Log::ERROR), "Unable to decode seed: ", ex.what());
                song_seed.dumpJson ("Problem seed");
            }
        }

        for (const auto &artist_seed : message ["music"]["artists"]) {
            try {
                artist_seeds.push_back (new ArtistSeed (station->pandora(), artist_seed));
            } catch (const Parsnip::Exception &ex) {
                flog (LOG_WHERE (Log::ERROR), "Unable to decode seed: ", ex.what());
                artist_seed.dumpJson ("Problem seed");
            }
        }

        for (const auto &genre_seed : message ["music"]["genres"]) {
            try {
                genre_seeds.push_back (new GenreSeed (station->pandora(), genre_seed));
            } catch (const Parsnip::Exception &ex) {
                flog (LOG_WHERE (Log::ERROR), "Unable to decode seed: ", ex.what());
                genre_seed.dumpJson ("Problem seed");
            }
        }
    }

    /*
     *              Add a station seeds
     */

    RequestAddStationSeed::RequestAddStationSeed (Station *sta, const std::string &music_id)
    : Request (sta->pandora(), "station.addMusic"),
      station (sta) {
        // clang-format off
        request_message = Parsnip::Data::make_dictionary ( {
            { "stationToken", station->getToken() } ,
            { "musicToken", music_id }
        });
        // clang-format on
    }

    void RequestAddStationSeed::extractResponse (const Parsnip::Data &message) {
        seed_id = message ["seedId"].asString();
        if (message.contains ("songName")) {
            song_seed = new SongSeed (station.get(), message);
        } else if (message.contains ("artistName")) {
            artist_seed = new ArtistSeed (station->pandora(), message);
        } else {
            genre_seed = new GenreSeed (station->pandora(), message);
        }
    }

    /*
     *              Remove a station seed
     */

    RequestRemoveStationSeed::RequestRemoveStationSeed (Station *station, const std::string &seed_id)
    : Notification ("station.deleteMusic") {
        request_message = Parsnip::Data::make_dictionary ({ { "seedId", seed_id } });
    }

    /*
     *              Mark a song as overplayed/do not play for awhile.
     */

    RequestAddTiredSong::RequestAddTiredSong (Source * const , const std::string &track_token)
    : Notification ("user.sleepSong") {
        request_message = Parsnip::Data::make_dictionary ({ { "trackToken", track_token } });
    }

    /*
     *              Add feedback to a song that has played.
     */

    RequestAddFeedback::RequestAddFeedback (Station *sta, const std::string &track_token, bool positive)
    : Request (sta->pandora(), "station.addFeedback"), station (sta) {
        // clang-format off
        request_message = Parsnip::Data::make_dictionary ({
            { "stationToken", station->getToken() },
            { "trackToken", track_token },
            { "isPositive", positive }
        });
        // clang-format on
    }

    void RequestAddFeedback::extractResponse (const Parsnip::Data &message) {
        feedback = new SongRating (station.get(), message);
    }

    /*
     *              Delete feedback for a song.
     */

    RequestDeleteFeedback::RequestDeleteFeedback (Station *, const std::string &feedback_id)
    : Notification ("station.deleteFeedback") {
        request_message = Parsnip::Data::make_dictionary ({ { "feedbackId", feedback_id } });
    }

    /*
     *              Retrieve a list of items for the playback queue
     */

    RequestQueueTracks::RequestQueueTracks (Station *sta)
    : Request (sta->pandora(), "station.getPlaylist", Option::TLS),
      station (sta) {
        // clang-format off
        request_message = Parsnip::Data::make_dictionary ({
            { "stationToken", station->getToken() },
            { "includeTrackLength", true }
        });
        // clang-format on
    }

    void RequestQueueTracks::extractResponse (const Parsnip::Data &message) {
        MusicAutoReleasePool pool;
        results.reset (new SongList());
        for (const auto &track_info : message ["items"]) {
            try {
                if (track_info.contains ("adToken")) {
                    Retainer <Advert *> ad (Advert::construct (station.get(), track_info ["adToken"].asString()));
                    if (ad) {
                        results->push_back (ad);
                    }
                } else {
                    results->push_back (new PlayableSong (source, track_info));
                }
            } catch (const Parsnip::Exception &ex) {
                flog (LOG_WHERE (Log::ERROR), "Unable to decode track: ", ex.what());
                track_info.dumpJson ("Problem queue track");
            }
        }
    }

    /*
     *              Search Request
     */

    SearchRequest::SearchRequest (Source * const src, const std::string &query, const bool exact)
    : Request (src, "music.search") {
        // clang-format off
        request_message = Parsnip::Data::make_dictionary ({
            { "searchText", query },
            { "includeNearMatches", !exact },
            { "includeGenreStations", true }
        });
        // clang-format on
    }

    void SearchRequest::extractResponse (const Parsnip::Data &message) {
        results.clear();
        for (const auto &song : message ["songs"]) {
            try {
                results.push_back (new SongSuggestion (source, song));
            } catch (const Parsnip::Exception &ex) {
                flog (LOG_WHERE (Log::ERROR), "Unable to decode song: ", ex.what());
                song.dumpJson ("Problem song");
            }
        }

        for (const auto &artist : message ["artists"]) {
            try {
                results.push_back (new ArtistSuggestion (source, artist));
            } catch (const Parsnip::Exception &ex) {
                flog (LOG_WHERE (Log::ERROR), "Unable to decode artist: ", ex.what());
                artist.dumpJson ("Problem artist");
            }
        }

        for (const auto &genre : message ["genreStations"]) {
            try {
                results.push_back (new GenreSuggestion (source, genre));
            } catch (const Parsnip::Exception &ex) {
                flog (LOG_WHERE (Log::ERROR), "Unable to decode genre: ", ex.what());
                genre.dumpJson ("Problem seed");
            }
        }
    }

    /*
     *              Retrieve advertisements from server
     */

    RetrieveAdvert::RetrieveAdvert (Station *sta, const std::string &ad_token)
    : Request (sta->pandora(), "ad.getAdMetadata"),
      station (sta) {
        // clang-format off
        request_message = Parsnip::Data::make_dictionary ({
            { "adToken", ad_token },
            { "returnAdTrackingTokens", true },
            { "supportAudioAds", true }
        });
        // clang-format on
    }

    void RetrieveAdvert::extractResponse (const Parsnip::Data &message) {
        advert = new Advert (source, message, station.get());
    }

    /*
     *              Notify servers of ad playback
     */
    AdvertPlayedNotification::AdvertPlayedNotification (PianodPlaylist *station,
                                                        const std::vector<std::string> &ad_tokens)
    : Notification ("ad.registerAd") {
        Parsnip::Data tokens{ Parsnip::Data::List };
        for (const auto &ad : ad_tokens) {
            tokens.push_back (ad);
        }
        // clang-format off
        request_message = Parsnip::Data {Parsnip::Data::Dictionary,
            "stationId", station->playlistId(),
            "adTrackingTokens", std::move (tokens)
        };
        // clang-format on
    }

}  // namespace Pandora
