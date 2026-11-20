///
/// Manage messages of various kinds to clients.
/// @file       response.cpp - pianod project
/// @author     Perette Barella
/// @date       Initial: 2012-03-16.  C++ Rework: 2014-10-27.
/// @copyright  Copyright 2012–2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cstdarg>
#include <cassert>
#include <ctime>

#include <exception>
#include <sstream>
#include <iomanip>

#include <football/fb_public.h>
#include <parsnip/parsnip.h>

#include "logging.h"
#include "musictypes.h"
#include "retainedlist.h"
#include "musickeys.h"
#include "response.h"
#include "connection.h"
#include "servicemanager.h"
#include "fundamentals.h"
#include "utility.h"
#include "user.h"
#include "mediaunit.h"

// Needed for exception typing.
#include <stdexcept>
#include <future>
#include <regex>

#include "filter.h"
#include "querylist.h"

/** Format a duration as minutes and seconds.
    @param duration The duration.
    @param minute_places The minimum number of digits to use for minutes.
    If less than this are required, the duration is zero-padded.
    @return The duration in m:ss format. */
std::string format_duration (time_t duration, int minute_places) {
    std::ostringstream duration_string;
    duration_string << std::setfill ('0') << std::setw (minute_places) << (duration / 60) << ':' << std::setw (2) << duration % 60;
    return duration_string.str();
}

/** Retrieve the text for a success, failure, event or data message.
    @param response The number of the status or datum.
    @return The corresponding text. */
const char *ResponseText (RESPONSE_CODE response) {
    assert (response != NO_REPLY);
    // clang-format off
    const static std::unordered_map<RESPONSE_CODE, const char *> responses {
        { NO_REPLY, "No text" },
        { V_PLAYING, "Playing" },
        { V_STALLED, "Stalled" },
        { V_PAUSED, "Paused" },
        { V_BETWEEN_TRACKS, "Intertrack" },
        { V_IDLE, "Idle" },
        { V_TRACK_COMPLETE, "Track playback complete" },

        { V_QUEUE_STOPPED, "Stopped" },
        { V_QUEUE_REQUEST, "Request" },
        { V_QUEUE_RANDOM, "Random Play" },

        { V_SELECTEDSOURCE, "SelectedSource" },
        { V_SELECTEDPLAYLIST, "SelectedPlaylist" },

        { V_MIX_CHANGED, "Mix has been changed" },
        { V_PLAYLISTS_CHANGED, "Playlist list has changed" },
        { V_PLAYLISTRATING_CHANGED, "PlaylistRatingChanged" },
        { V_SOURCES_CHANGED, "Available sources have changed" },
        { V_SONGRATING_CHANGED, "SongRatingChanged" },
        { V_QUEUE_CHANGED, "QueueChanged" },
        { V_YELL, "says" },
        { V_SOURCE_STATUS, "Status" },
        { V_SERVER_STATUS, "Status" },
        { V_USERACTION, "UserAction" },
        { V_SELECTIONMETHOD, "QueueRandomize" },

        { I_WELCOME, "pianod2 " PACKAGE_VERSION ". Welcome!" },
        { I_ID, "ID" },
        { I_ALBUM, "Album" },
        { I_ARTIST, "Artist" },
        { I_SONG, "Title" },
        { I_PLAYLIST, "Playlist" },
        { I_RATING, "Rating" },
        { I_COVERART, "CoverArt" },
        { I_GENRE, "Genre" },
        { I_PLAYLISTRATING, "PlaylistRating" },
        { I_CHOICEEXPLANATION, "Explanation" },
        { I_OWNER, "Owner" },
        { I_SOURCE, "Source" },
        { I_NAME, "Name" },
        { I_YEAR, "Year" },
        { I_DURATION, "Duration" },
        { I_ACTIONS, "Actions" },

        { I_ROOM, "Room" },

        { I_VOLUME, "Volume" },
        { I_AUDIOQUALITY, "Quality" },
        { I_HISTORYSIZE, "HistoryLength" },
        { I_AUTOTUNE_MODE, "AutotuneMode" },
        { I_PAUSE_TIMEOUT, "PauseTimeout" },
        { I_PLAYLIST_TIMEOUT, "PlaylistTimeout" },
        
        { I_PROXY, "Proxy" },
        { I_CONTROLPROXY, "ControlProxy" },
        
        { I_SERVICE_USER, "ServiceUser" },
        { I_SERVICE_PASSWORD, "ServicePassword" },
        { I_CACHE_MINIMUM, "CacheMinimum" },
        { I_CACHE_MAXIMUM, "CacheMaximum" },
        { I_PATHNAME, "PathName" },
        { I_SELECTION_ALGORITHM, "SelectionAlgorithm" },
        
        
        { I_OUTPUT_DRIVER, "OutputDriver" },
        { I_OUTPUT_DEVICE, "OutputDevice" },
        { I_OUTPUT_ID, "OutputID" },
        { I_OUTPUT_SERVER, "OutputServer" },
        { I_INFO_URL, "SeeAlso" },
        { I_USER_PRIVILEGES, "Privileges" },
        { I_INFO, "Information" },
        
        { I_STATISTICS_ATTEMPTS, "PlayAttempts" },
        { I_STATISTICS_PLAYS, "TracksPlayed" },
        { I_STATISTICS_FAILURES, "PlaybackFailures" },
        { I_STATISTICS_SEQUENTIAL_FAILS, "SequentialFailures" },
        { I_STATISTICS_REPLACEMENTS, "SongsReplaced" },
        { I_STATISTICS_DONATIONS, "SongsDonated" },

        { S_OK, "Success" },
        { S_ANSWER_YES, "True, yes, 1, on" },
        { S_ANSWER_NO, "False, no, 0, off" },
        { S_DATA, "Data" },
        { S_DATA_END, "No data or end of data" },
        { S_SIGNOFF, "Good-bye" },
        { S_NOOP, "Doing nothing was successful" },
        { S_PARTIAL, "Partial success" },
        { S_MATCH, "Matches" },
        { S_ROUNDING, "Success, but value was approximated" },
        { S_PENDING, "Success pending" },

        { E_BAD_COMMAND, "Bad command" },
        { E_UNAUTHORIZED, "Not authorized for requested action" },
        { E_NAK, "Action failed" },
        { E_DUPLICATE, "Already exists" },
        { E_NOTFOUND, "Requested item not found" },
        { E_WRONG_STATE, "Action is not applicable to current state" },
        { E_CREDENTIALS, "Invalid login or password" },
        { E_REQUESTPENDING, "Temporary failure, future completion unknown" },
        { E_INVALID, "Invalid parameter" },
        { E_TRANSFORM_FAILED, "Playlist personalization failed" },
        { E_QUOTA, "Quota exceeded" },
        { E_LOGINREQUIRED, "Must be logged in" },
        { E_UNSUPPORTED, "Not supported" },
        { E_CONFLICT, "Conflict encountered" },
        { E_RESOURCE, "Insufficient resources" },
        { E_RANGE, "Limit has been reached" },
        { E_WRONGTYPE, "Wrong type for action" },
        { E_PERSISTENT, "Persistent expression or value required" },
        { E_AMBIGUOUS, "Ambiguous expression" },
        { E_PARTIAL, "Partial failure" },
        { E_VARIOUS, "Assorted failures" },
        { E_NO_ASSOCIATION, "No association" },
        { E_TYPE_DISALLOWED, "Cannot specify type with expression form" },
        { E_EXPRESSION, "Syntax error in expression" },
        { E_PLAYLIST_REQUIRED, "Song is not associated with a playlist" },
        { E_BAD_SCHEMA, "Request does not comply with schema" },

        { E_TIMEOUT, "Timeout" },
        { E_METAPLAYLIST, "Operation only valid on primary playlists" },
        { E_MEDIA_ACTION, "Action not supported by source" },
        { E_MEDIA_VALUE, "Value is not supported by source" },
        { E_MEDIA_MANAGER, "Action not possible on media manager" },
        { E_MEDIA_FAILURE, "Source failed to execute a request" },
        { E_MEDIA_TRANSIENT, "Action not supported on transient playlist" },
        { E_BUG, "There is a bug in " PACKAGE },
        { E_NOT_IMPLEMENTED, "Not implemented" },

        { F_FAILURE, "Internal server error" },
        { F_PLAYER_EMPTY, "Nothing to play" },
        { F_NETWORK_FAILURE, "Network failure " },
        { F_SHUTDOWN, "Service shutting down" },
        { F_AUTHENTICATION, "Authentication failure" },
        { F_RESOURCE, "Insufficent resources" },
        { F_PANDORA, "Error communicating with Pandora" },
        { F_INCOMPLETE, "Command execution incomplete" },
        { F_PERMISSION, "Permission denied" },
        { F_EXCEPTION, "Exception" },
        { F_NETWORK_TIMEOUT, "Network timeout" },
        { F_CANNOT_OUTPUT, "Cannot open audio output" },
        { F_AUDIO_FAILURE, "Audio failure" },

        { A_SIGNED_IN, "signed in" },
        { A_SIGNED_OUT, "has disconnected" },
        { A_KICKED, "kicked" },
        { A_IMBECILE, "executed an imbecilic command" },
        { A_SKIPPED, "skipped the song" },
        { A_STOPPED, "requested stop" },
        { A_REQUESTS, "set requests-only mode" },
        { A_RANDOMPLAY, "enabled random play" },
        { A_ADJUSTAUDIO, "adjusted audio settings" },
        { A_PAUSED, "paused playback" },
        { A_RESUMED, "resumed playback" },
        { A_CHANGED_MIX, "changed the mix" },
        { A_MIX_ADDED, "added to the mix" },
        { A_MIX_REMOVED, "removed from the mix" },
        { A_SELECTED_PLAYLIST, "selected the playlist" },
        { A_CREATED_PLAYLIST, "created the playlist" },
        { A_RENAMED_PLAYLIST, "renamed the playlist" },
        { A_DELETED_PLAYLIST, "deleted the playlist" },
        { A_SOURCE_ADD, "added a source" },
        { A_SOURCE_BORROW, "borrowed a source" },
        { A_SOURCE_REMOVE, "removed a source" },
        { A_REQUEST_ADD, "requested music" },
        { A_REQUEST_CANCEL, "cancelled requests" },
        { A_REQUEST_CLEAR, "cleared requests" },
        { A_SHUTDOWN, "initiated player shutdown" },
    };
    // clang-format on
    auto it = responses.find (response);
    if (it == responses.end()) {
        assert (!"Unfound status in ResponseText()");
        flog (LOG_WHERE (Log::ERROR), "Unknown status ", (int) response);
        return "Unknown status";
    }
    return it->second;
}

Parsnip::Data Response::NoJsonData{ nullptr };

Response::Response (RESPONSE_CODE msg) : message (msg), type (Type::EMPTY) {
}

Response::Response (RESPONSE_CODE msg, const std::string &details)
: message (msg), type (details.empty() ? Type::EMPTY : Type::STRING), value (details) {
}

Response::Response (RESPONSE_CODE msg, long details) : message (msg), type (Type::LONG), long_value (details) {
}

Response::Response (RESPONSE_CODE msg, double details, int precision)
: message (msg), type (Type::DOUBLE), double_value (details), double_precision (precision) {
}

Response::Response (RESPONSE_CODE msg, List &&details, Parsnip::Data &&json_details)
: message (msg),
  type (Type::WORDLIST),
  list (std::move (details)),
  json_data (new Parsnip::Data{ std::move (json_details) }) {
}

Response::Response (const std::string &regard, RESPONSE_CODE msg)
: message (msg), type (Type::EMPTY), regarding (regard){};
Response::Response (const std::string &regard, RESPONSE_CODE msg, const std::string &details)
: message (msg), type (details.empty() ? Type::EMPTY : Type::STRING), value (details), regarding (regard){};
Response::Response (const std::string &regard, RESPONSE_CODE msg, long details)
: message (msg), type (Type::LONG), long_value (details), regarding (regard){};
Response::Response (const std::string &regard, RESPONSE_CODE msg, double details, int precision)
: message (msg), type (Type::DOUBLE), double_value (details), double_precision (precision), regarding (regard){};
Response::Response (const std::string &regard, RESPONSE_CODE msg, List &&details, Parsnip::Data &&json_details)
: message (msg),
  type (Type::WORDLIST),
  list (std::move (details)),
  json_data (new Parsnip::Data{ std::move (json_details) }),
  regarding (regard) {};

Response::Response (Retainer<MusicThingie *> rel, RESPONSE_CODE msg)
: message (msg), type (Type::EMPTY), related (std::move (rel)){};
Response::Response (Retainer<MusicThingie *> rel, RESPONSE_CODE msg, const std::string &details)
: message (msg), type (details.empty() ? Type::EMPTY : Type::STRING), value (details), related (std::move (rel)){};
Response::Response (Retainer<MusicThingie *> rel, RESPONSE_CODE msg, long details)
: message (msg), type (Type::LONG), long_value (details), related (std::move (rel)){};
Response::Response (Retainer<MusicThingie *> rel, RESPONSE_CODE msg, double details, int precision)
: message (msg), type (Type::DOUBLE), double_value (details), double_precision (precision), related (std::move (rel)){};
Response::Response (Retainer<MusicThingie *> rel, RESPONSE_CODE msg, List &&details, Parsnip::Data &&json_details)
: message (msg),
  type (Type::WORDLIST),
  list (std::move (details)),
  json_data (new Parsnip::Data{ std::move (json_details) }),
  related (std::move (rel)){};

Response::Response (const CommandError &err)
: message (err.reason()), type (err.what() ? Type::STRING : Type::EMPTY), value (err.what() ? err.what() : "") {}
Response::Response (const std::string &regard, const CommandError &err)
: message (err.reason()), type (err.what() ? Type::STRING : Type::EMPTY), value (err.what() ? err.what() : ""),
  regarding (regard) {};
Response::Response (Retainer<MusicThingie *> rel, const CommandError &err)
: message (err.reason()), type (err.what() ? Type::STRING : Type::EMPTY), value (err.what() ? err.what() : ""),
  related (rel) {};

/*
 *                  Response grouping
 */

/** Merge members from another ResponseGroup.
    @param merge_from The other group.  */
void ResponseGroup::operator() (ResponseGroup &&merge_from) {
    for (Response &response : merge_from) {
        push_back (std::move (response));
    }
    merge_from.clear();
}

/** Transmit a group of response messages to a single node using
    the node's selected protocol.
    @param destination The node to direct the responses to. */
void ResponseGroup::transmit (class PianodConnection &destination) const {
    if (!empty()) {
        if (destination.transmitJSON()) {
            Parsnip::Data serial{ serializeEvents(true, destination.user) };
            std::ostringstream out;
            serial.toJson (out) << "\n";
            destination.print (out.str());
        } else {
            transmitLine (destination);
        }
    }
}

/** Transmit a group of response messages to all nodes on a service,
    using the appropriate protocol for each node.
    @param destination The service to direct the responses to. */
void ResponseGroup::transmit (PianodService &destination) const {
    if (!empty()) {
        transmitLine (destination);
        transmitJSON (destination);
    }
}

/*
 *                  Response aggregation
 */

/** Initialize a response collection, and add the initial response to the
    appropriate collection if applicable. */
ResponseCollector::ResponseCollector (Response &&initial) : reason (initial.message) {
    if (initial.message == NO_REPLY) {
        assert (initial.type == Response::Type::EMPTY);
    } else if (initial.message == S_DATA) {
        assert (initial.type == Response::Type::EMPTY);
    } else if (initial.isCommandError()) {
        this->diagnostics (std::move (initial));
    } else if (initial.isSuccess()) {
        this->successes (std::move (initial));
    } else {
        reason = S_DATA;
        data (std::move (initial));
    }
}

/** Implicitly wrap a music item into a data response. */
ResponseCollector::ResponseCollector (MusicThingie *item) : reason (S_DATA) {
    ResponseGroup group;
    group (item, I_ATTACHED_THING);
    data_groups.push_back (std::move (group));
}

/** Implicitly wrap a list of music items into a data response. */
ResponseCollector::ResponseCollector (const ThingieList &things) : reason (S_DATA) {
    data (things);
}

/** Implicitly wrap a list of songs into a data response. */
ResponseCollector::ResponseCollector (const SongList &songs) : reason (S_DATA) {
    data (songs);
}

/** Implicitly wrap a response group into a data response. */
ResponseCollector::ResponseCollector (ResponseGroup &&group) : reason (S_DATA) {
    data_groups.push_back (std::move (group));
}

/*
 * ResponseCollector: Inserters
 */

/** Add a list of music items for transmission as response data. */
void ResponseCollector::data (const ThingieList &item_list) {
    assert (reason == S_DATA);
    ResponseGroup group;
    for (auto thing : item_list) {
        group.clear();
        group (thing, I_ATTACHED_THING);
        data_groups.push_back (std::move (group));
    }
}

/** Add a list of songs for transmission as response data. */
void ResponseCollector::data (const SongList &songs) {
    assert (reason == S_DATA);
    ResponseGroup group;
    for (auto song : songs) {
        group.clear();
        group (song, I_ATTACHED_THING);
        data_groups.push_back (std::move (group));
    }
}

/** Add a list of playlists for transmission as response data. */
void ResponseCollector::data (const PlaylistList &playlists) {
    assert (reason == S_DATA);
    ResponseGroup group;
    for (auto playlist : playlists) {
        group.clear();
        group (playlist, I_ATTACHED_THING);
        data_groups.push_back (std::move (group));
    }
}

/** Add a record for transmission as response data. */
void ResponseCollector::data (ResponseGroup &&group) {
    assert (reason == S_DATA);
#ifndef NDEBUG
    for (Response &item : group) {
        assert (item.isDataField());
    }
#endif
    data_groups.push_back (std::move (group));
}

/*
 * ResponseCollector: Other
 */

/** Transmit a reply.
    Send requested data or reply to the requesting node using their desired protocol.
    Broadcast events as follows, using the appropriate protocol for each node:
    - broadcast_events go system-wide, all connections, all services
    - room_events go to all connections on the current service
    - user_events go to all connections by the same user on any services.
    @param destination The client to which to send the reponse.
    @param json If true, use JSON for data or reply; otherwise use line protocol. */
void ResponseCollector::transmit (PianodConnection &destination, const bool json) {
    if (json) {
        transmitJSON (destination);
    } else {
        transmitLine (destination);
    }

    destination.announceToRoom (std::move (room_events));
    destination.announceToAll (std::move (broadcast_events));
    service_manager->broadcast (user_events, destination.user);
    if (close_after_response) {
        destination.user = nullptr;
        destination.close_after_events();
    }
}

/*
 *                 CommandReply
 */

/** Update the collection's final status.
    @param why The new response code being accommodated;
    must be a success or command error code. */
void CommandReply::mergeReason (RESPONSE_CODE why) {
    assert (reason != S_DATA && reason != NO_REPLY);
    if (noop()) {
        reason = why;
    } else if (partial()) {
        reason = mixed_reason;
    } else if (allSuccess() && reason != why) {
        reason = S_OK;
    } else if (allFailure() && reason != why) {
        reason = E_VARIOUS;
    }
}

/** Add a failure response to the collection.
    @param failure The failure to add. */
void CommandReply::fail (Response &&failure) {
    assert (failure.isCommandError());
    mergeReason (failure.message);
    diagnostics (std::move (failure));
};

/** Add a success response to the collection.
    @param success The success to add. */
void CommandReply::succeed (Response &&success) {
    assert (success.isSuccess());
    mergeReason (success.message);
    successes (std::move (success));
};

/** Add a success or failure response to the collection.
    @param status The response to add. */
void CommandReply::operator() (Response &&status) {
    if (status.isSuccess()) {
        succeed (std::move (status));
    } else {
        fail (std::move (status));
    }
};

/** Construct a response collector for status collection.
    @param kind If the evemt of mixed successes/failures, an `Aggregation::OPTIMISTIC`
    collector will report success, a `Aggregation::PESSIMISTIC` will return failure
    for the final/overall status. */
CommandReply::CommandReply (Aggregation kind) : mixed_reason (kind == Aggregation::OPTIMISTIC ? S_PARTIAL : E_PARTIAL) {
}

/*
 *                 DataResponse
 */

/** Add more data to the response.
    @param data The data to add. */
void DataResponse::data (Response &&data) {
    assert (data.isDataField());
    data_reply (std::move (data));
}

/** Send updated ratings.
    @param conn The connection initiating the ratings change.
    @param song The ratings to send.
    @param response The reply in which ratings are inserted. */
void sendUpdatedRatings (PianodConnection &conn, const PianodSong *song, CommandReply *response) {
    if (song->ratingScheme() == RatingScheme::OWNER) {
        response->room_events (song->assembleRatings (conn.user, song->playlist(), true));
    } else {
        response->user_events (song->assembleRatings (conn.user, song->playlist(), true));
    }
}

/*
 *                     Convert Exceptions to Responses
 */

/** Translate a standard exception message into a Response.
    @param except An exception. */
static Response standard_exception (const std::exception &except) {
    static struct {
        const std::type_info *type;
        const char *name;
    } exception_names [] = { { &typeid (std::exception), "exception" },

                             { &typeid (std::logic_error), "logic error" },
                             { &typeid (std::invalid_argument), "invalid argument" },
                             { &typeid (std::domain_error), "domain error" },
                             { &typeid (std::length_error), "length error" },
                             { &typeid (std::out_of_range), "out-of-range value" },
                             { &typeid (std::future_error), "future error" },

                             { &typeid (std::range_error), "range error" },
                             { &typeid (std::overflow_error), "numeric overflow" },
                             { &typeid (std::underflow_error), "numeric underflow" },
                             { &typeid (std::regex_error), "regular expression error" },

                             { &typeid (std::system_error), "system error" },
                             { &typeid (std::ios_base::failure), "System or I/O error" },

                             { &typeid (std::runtime_error), "runtime error" },
                             { &typeid (std::bad_typeid), "bad typeid" },
                             { &typeid (std::bad_cast), "invalid typecast" },
                             { &typeid (std::bad_weak_ptr), "invalid weak_ptr" },
                             { &typeid (std::bad_function_call), "bad function call" },
                             { &typeid (std::bad_exception), "bad exception" } };

    const char *type = "Exception name missing";
    for (auto const &ex : exception_names) {
        if (*(ex.type) == typeid (except)) {
            type = ex.name;
            break;
        }
    }
    return Response (E_NAK, std::string (type) + ": " + except.what());
}

/** Contruct a maningful failure response from an exception.
    @param except An exception of any type.
    @param triggering_command The command causing the exception (for logging; may be omitted.) */
CommandReply::CommandReply (const std::exception_ptr except, const char *triggering_command) {
    LogType loglevel{ Log::WARNING };
    try {
        std::rethrow_exception (except);
    } catch (const std::bad_alloc &error) {
        fail (Response (E_RESOURCE, "Could not allocate memory);"));
    } catch (const Query::impossible &error) {
        fail (Response (E_MEDIA_ACTION, "Query complexity exceeds source ability"));
    } catch (const CommandError &error) {
        fail (Response (error.reason(), error.what()));
    } catch (const Parsnip::Exception &error) {
        std::string message = error.what();
        if (error.where()) {
            message += " at ";
            message += error.where();
        }
        fail (Response (E_BAD_COMMAND, message));
    } catch (const std::exception &error) {
        fail (standard_exception (error));
    } catch (...) {
        loglevel = Log::ERROR;
        fail (Response (E_NAK, std::string ("Custom exception")));
    }
    if (triggering_command) {
        flog (LOG_WHERE (loglevel),
              "Command exception: ",
              diagnostics.front().value,
              "\nCommand was: ",
              triggering_command);
    } else {
        flog (LOG_WHERE (loglevel), "Command exception: ", diagnostics.front().value);
    }
}
