///
/// Send response messages over the JSON protocol.
/// @file       responsejson.cpp - pianod project
/// @author     Perette Barella
/// @date       2020-02-06
/// @copyright  Copyright 2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cstdarg>
#include <cassert>

#include <sstream>
#include <iomanip>

#include <football/fb_public.h>
#include <parsnip/parsnip.h>

#include "logging.h"
#include "fundamentals.h"
#include "musictypes.h"
#include "musickeys.h"
#include "response.h"
#include "connection.h"
#include "user.h"
#include "mediaunit.h"

namespace JSON {
    namespace Key {
        static const char *Events = "events";
        static const char *State = "state";
        static const char *CurrentSong = "currentSong";
        static const char *Errors = "errors";

        static const char *Successes = "successes";
        static const char *Diagnostics = "failures";
        static const char *StatusText = "status";
        static const char *StatusNumeric = "code";
        static const char *Details = "details";

        static const char *Data = "data";
        static const char *RelatedId = "id";
        static const char *RelatedName = "name";
        static const char *PlaybackState = "playbackState";
        static const char *QueueMode = "queueMode";

        const char *PlayDuration = Music::Key::SongDuration;
        const char *PlayPoint = "timeIndex";
        const char *PlayRemaining = "timeRemaining";
    }  // namespace Key
}  // namespace JSON

/** Retrieve the JSON text for a response.
    @param code The number of the response.
    For status or error text use ResponseText.
    @return The name of the field or related text. */
const char *JSONFieldName (RESPONSE_CODE code) {
    assert (isStatusChange (code) || isDataField (code));
    // clang-format off
    const static std::unordered_map<RESPONSE_CODE, const char *> field_names {
        // For playback states, these are state names instead of field names.
        { V_PLAYING, "playing" },
        { V_STALLED, "stalled" },
        { V_PAUSED, "paused" },
        { V_IDLE, "idle" },
        { V_BETWEEN_TRACKS, "intertrack" },

        // For queueMode, these are state names instead of field names.
        { V_QUEUE_STOPPED, "stopped" },
        { V_QUEUE_REQUEST, "request" },
        { V_QUEUE_RANDOM, "random" },

        { V_SELECTEDSOURCE, "selectedSource" },
        { V_SELECTEDPLAYLIST, "selectedPlaylist" },
        { V_SELECTIONMETHOD, "queueRandomize" },

        { I_WELCOME, "software" },
        { I_ID, Music::Key::PrimaryId },
        { I_CHOICEEXPLANATION, "explanation" },
        { I_OWNER, "owner" },
        { I_SOURCE, Media::Key::Source },
        { I_NAME, Music::Key::PrimaryName },
        { I_ACTIONS, Music::Key::Actions },
        { I_RATING, Music::Key::SongRatings },
        { I_ROOM, "room" },

        { I_VOLUME, "volume" },
        { I_AUDIOQUALITY, "quality" },
        { I_HISTORYSIZE, "historyLength" },
        { I_AUTOTUNE_MODE, "autotuneMode" },
        { I_PAUSE_TIMEOUT, "pauseTimeout" },
        { I_PLAYLIST_TIMEOUT, "playlistTimeout" },
        { I_PLAYLIST, Music::Key::PlaylistName },
        { I_PROXY, "proxy" },
        { I_CONTROLPROXY, "controlProxy" },
        
        { I_SERVICE_USER, "serviceUsername" },
        { I_SERVICE_PASSWORD, "servicePassword" },
        { I_CACHE_MINIMUM, "cacheMinimum" },
        { I_CACHE_MAXIMUM, "cacheMaximum" },

        { I_OUTPUT_DRIVER, "outputDriver" },
        { I_OUTPUT_DEVICE, "outputDevice" },
        { I_OUTPUT_ID, "outputID" },
        { I_OUTPUT_SERVER, "outputServer" },
        { I_INFO_URL, "seeAlso" },
        { I_USER_PRIVILEGES, "privileges" },
        { I_INFO, "information" },
        
        { I_STATISTICS_ATTEMPTS, "trackPlayAttempts" },
        { I_STATISTICS_PLAYS, "tracksPlayed" },
        { I_STATISTICS_FAILURES, "playbackFailures" },
        { I_STATISTICS_SEQUENTIAL_FAILS, "sequentialFailures" },
        { I_STATISTICS_REPLACEMENTS, "tracksReplaced" },
        { I_STATISTICS_DONATIONS, "tracksDonated" }
    };
    // clang-format on
    auto it = field_names.find (code);
    if (it == field_names.end()) {
        assert (!"Unfound status in jsonFieldname()");
        flog (LOG_WHERE (Log::ERROR), "Unknown field ", (int) code);
        return "Unknown status";
    }
    return it->second;
}

/** Format a response for JSON protocol.
    @return For data responses, a Parsnip::Data containing response data,
    otherwise a dictionary containing event/response details. */
Parsnip::Data Response::serialize() const {
    switch (message) {
        case V_SELECTEDPLAYLIST:
            assert (related);
            return related->serialize();
        case I_WELCOME:
            return Parsnip::Data::make_dictionary ({
                { "application", PACKAGE },
                { "version", atoi (PACKAGE_VERSION) }
            });
        default:
            break;
    }

    Parsnip::Data data;
    switch (type) {
        case Type::EMPTY:
            break;
        case Type::STRING:
            if (message == V_YELL) {
                data = (user ? user->username() + " " : "A visitor ") + ResponseText (message) + value;
            } else {
                data = value;
            }
            break;
        case Type::LONG:
            data = long_value;
            break;
        case Type::DOUBLE:
            data = double_value;
            break;
        case Type::WORDLIST:
            assert (json_data);
            assert (!json_data->isNull());
            data = *(json_data.get());
            break;
    }
    if (isUserAction()) {
        std::ostringstream rewrite;
        rewrite << (user ? user->username() : "A visitor") << ' ' << ResponseText (message);
        if (type == Type::STRING) {
            rewrite << ": " << value;
        } else if (!data.isNull()) {
            rewrite << ": ";
            data.toJson (rewrite);
        }
        data = rewrite.str();
    }
    if (isDataField() || message == V_SELECTIONMETHOD || message == V_SELECTEDSOURCE) {
        return data;
    }

    // clang-format off
    Parsnip::Data response {Parsnip::Data::Dictionary,
        JSON::Key::StatusNumeric, long (isUserAction() ? V_USERACTION : message),
        JSON::Key::StatusText, ResponseText (message),
        JSON::Key::Details, std::move (data)
    };
    // clang-format on
    if (related) {
        response [JSON::Key::RelatedId] = related->id();
        response [JSON::Key::RelatedName] = related->name();
    } else if (!regarding.empty()) {
        response [JSON::Key::RelatedId] = regarding;
        response [JSON::Key::RelatedName] = regarding;
    }
    return response;
}

/** Format successes or diagnostics for JSON transmission.
    @return A list containing the responses. */
Parsnip::Data ResponseGroup::serializeReply() const {
    Parsnip::Data response{ Parsnip::Data::List };
    for (const Response &resp : *this) {
        assert (resp.isSuccess() || resp.isCommandError());
        response.push_back (resp.serialize());
    }
    return response;
}

/** Format a record's worth of reponses for JSON protocol.
    @return A dictionary containing the record's data. */
Parsnip::Data ResponseGroup::serializeData (const User *user) const {
    Parsnip::Data response{ Parsnip::Data::Dictionary };
    for (const Response &item : *this) {
        if (item.message == I_ATTACHED_THING || item.message == I_ATTACHED_SONG) {
            assert (item.type == Response::Type::EMPTY);
            assert (item.related);
            assert (response.empty());
            response = item.related->serialize();
            if (item.message == I_ATTACHED_THING) {
                item.related->serializePrivate (response, user);
            }
        } else {
            response [JSONFieldName (item.message)] = item.serialize();
        }
    }
    return response;
}

/** Format a homogenous list of responses as a series JSON records.
    @return A list containing dictionaries, each with their one data item. */
Parsnip::Data ResponseGroup::serializeHomogenousData (const User *user) const {
    Parsnip::Data response{ Parsnip::Data::List };
    for (const Response &item : *this) {
        response.push_back (Parsnip::Data{ Parsnip::Data::Dictionary, JSONFieldName (item.message), item.serialize() });
    }
    return response;
}

/** Re-sort messages to be broadcast into categories:
    - State updates (such as queue mode has been changed to X)
    - Current song (which could be in state, but isn't).
    - Events (track ended, someone logged in, etc)
    - Spontaneous errors on the server.  Much like events,
      except that they are errors.
    @param inline_details If true, include user-specific info (ratings).
    @param user The user for whom to include details.
    @return A dictionary containing dictionaries for any
    non-empty categories, suitable for JSON transmission. */
Parsnip::Data ResponseGroup::serializeEvents (bool inline_details, const User *user) const {
    Parsnip::Data events{ Parsnip::Data::List };
    Parsnip::Data server_failures{ Parsnip::Data::List };
    Parsnip::Data status{ Parsnip::Data::Dictionary };
    Parsnip::Data current_song{ Parsnip::Data::Dictionary };
    for (const Response &item : *this) {
        if (item.isServerFailure()) {
            server_failures.push_back (item.serialize());
            continue;
        }
        if (item.isUserAction()) {
            events.push_back (item.serialize());
            continue;
        }
        switch (item.message) {
            case I_ATTACHED_THING:
                assert (current_song.empty());
                current_song = item.related->serialize();
                if (inline_details) {
                    item.related->serializePrivate (current_song, user);
                }
                break;
            case V_PLAYING:
            case V_STALLED:
            case V_PAUSED:
                assert (item.json_data);
                current_song [JSON::Key::PlayDuration] = item.json_data->at (JSON::Key::PlayDuration);
                current_song [JSON::Key::PlayPoint] = item.json_data->at (JSON::Key::PlayPoint);
                current_song [JSON::Key::PlayRemaining] = item.json_data->at (JSON::Key::PlayRemaining);
                [[fallthrough]];
            case V_IDLE:
            case V_BETWEEN_TRACKS:
                status [JSON::Key::PlaybackState] = JSONFieldName (item.message);
                break;
            case V_QUEUE_STOPPED:
            case V_QUEUE_REQUEST:
            case V_QUEUE_RANDOM:
                status [JSON::Key::QueueMode] = JSONFieldName (item.message);
                break;
            case V_TRACK_COMPLETE:
                assert (current_song.empty());
                current_song = nullptr;
                [[fallthrough]];
            case V_MIX_CHANGED:
            case V_PLAYLISTS_CHANGED:
            case V_PLAYLISTRATING_CHANGED:
            case V_SOURCES_CHANGED:
            case V_SONGRATING_CHANGED:
            case V_QUEUE_CHANGED:
            case V_YELL:
            case V_USERACTION:
            case V_SERVER_STATUS:
            case V_SOURCE_STATUS:
                events.push_back (item.serialize());
                break;
            case I_RATING:
            case I_ACTIONS:
                current_song [JSONFieldName (item.message)] = item.serialize();
                break;
            default:
                assert (item.isStatusChange() || item.isDataField());
                status [JSONFieldName (item.message)] = item.serialize();
                break;
        }
    }
    Parsnip::Data response{ Parsnip::Data::Dictionary };
    if (!events.empty())
        response [JSON::Key::Events] = std::move (events);
    if (!status.empty())
        response [JSON::Key::State] = std::move (status);
    if (!server_failures.empty())
        response [JSON::Key::Errors] = std::move (server_failures);
    if (current_song.isNull() || !current_song.empty())
        response [JSON::Key::CurrentSong] = std::move (current_song);
    return response;
}

/** Transmit a group of responses JSON connection on a service.
    If the responses include the current song, follow up with
    user-specific ratings and actions.
    @param destination The service to which to send responses. */
void ResponseGroup::transmitJSON (class PianodService &destination) const {
    Parsnip::Data serial{ serializeEvents (false, nullptr) };
    std::ostringstream out;
    serial.toJson (out) << "\n";
    destination.conditional_broadcast (&PianodConnection::json_connections_only, out.str());

    // If there's a current song in there, transmit user-specific details.
    serial = Parsnip::Data::make_dictionary ({});
    for (const Response &item : *this) {
        if (item.message == I_ATTACHED_THING) {
            for (PianodConnection *conn : destination) {
                if (conn->transmitJSON()) {
                    serial [JSON::Key::CurrentSong] = Parsnip::Data::make_dictionary ({});
                    item.related->serializePrivate (serial [JSON::Key::CurrentSong], conn->user);
                    out.str ("");
                    serial.toJson (out) << "\n";
                    conn->print (out.str());
                }
            }
            break;
        }
    }
}

/** Assemble a response collection into *either* a data response *or*
    a reply (i.e., successes/failures).
    @return The JSON-formatted data response or reply. */
Parsnip::Data ResponseCollector::serialize (const User *user) const {
    if (!data_reply.empty() || !data_groups.empty()) {
        assert (reason == S_DATA);
        assert (successes.empty());
        assert (diagnostics.empty());
        assert (user_events.empty());
        assert (room_events.empty());
        assert (broadcast_events.empty());
        assert (information.empty());
        Parsnip::Data response{ Parsnip::Data::List };
        if (!data_reply.empty()) {
            if (data_reply.size() >= 2 && data_reply [0].message == data_reply [1].message) {
                response = data_reply.serializeHomogenousData (user);
            } else {
                response.push_back (data_reply.serializeData (user));
            }
        }
        for (const ResponseGroup &dg : data_groups) {
            response.push_back (dg.serializeData (user));
        }
        // clang-format off
        return Parsnip::Data {Parsnip::Data::Dictionary,
            JSON::Key::StatusNumeric, long (S_DATA),
            JSON::Key::StatusText, ResponseText (S_DATA),
            JSON::Key::Data, std::move (response)
        };
        // clang-format on
    }
    Parsnip::Data response{ information.serializeEvents (true, user) };
    if (reason != NO_REPLY) {
        response [JSON::Key::StatusNumeric] = long (reason);
        response [JSON::Key::StatusText] = ResponseText (reason);
        response [JSON::Key::Successes] = successes.serializeReply();
        response [JSON::Key::Diagnostics] = diagnostics.serializeReply();
    }
    return response;
}

/** Format and transmit a response collection to JSON client.
    @param destination The client to which to send the reply. */
void ResponseCollector::transmitJSON (PianodConnection &destination) const {
    Parsnip::Data serial{ serialize (destination.user) };
    std::ostringstream out;
    serial.toJson (out) << "\n";
    destination.print (out.str());
}
