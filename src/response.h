///
/// Send messages of various kinds to clients.
/// @file       response.h - pianod project
/// @author     Perette Barella
/// @date       Initial: 2012-03-16.  C++ Rework: 2014-10-27.
/// @copyright  Copyright 2012–2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <ctime>

#include <string>
#include <vector>
#include <ostream>
#include <exception>
#include <memory>

#include "fundamentals.h"
#include "musictypes.h"
#include "retainer.h"

namespace Football {
    class Thingie;
}
namespace Parsnip {
    class Data;
}

extern const char *ResponseText (RESPONSE_CODE code);
extern const char *JSONFieldName (RESPONSE_CODE code);

namespace JSON {
    namespace Key {
        extern const char *PlayDuration;
        extern const char *PlayPoint;
        extern const char *PlayRemaining;
    }  // namespace Key
}  // namespace JSON

std::string format_duration (time_t duration, int minute_places = 1);

/** Type combining `RESPONSE_CODE` with a value or explanation:
    - Data response have values.
    - Successes and failures have an associated value.
    - Failures can have additional explanation. */
class Response {
    friend class ResponseCollector;
    friend class ResponseGroup;
    friend class CommandReply;

public:
    using List = std::vector<std::string>;
    static constexpr int TransmitUnaltered = 0;        ///< Transmit data as stored.
    static constexpr int TransmitAsDiagnostic = -100;  ///< Transform command errors to diagnostic codes.

    static Parsnip::Data NoJsonData;
private:

    enum class Type { EMPTY, STRING, LONG, DOUBLE, WORDLIST };  // Additional data types.
    RESPONSE_CODE message = NO_REPLY;                           ///< Message code, initialized to "empty" value.
    Type type{ Type::EMPTY };                                   ///< Type of additional data stored herein.
    std::string value;
    long long_value;
    double double_value;
    int double_precision;
    List list;
    std::shared_ptr<Parsnip::Data> json_data;  ///< Parsnip data to be used for JSON replies.
    std::string regarding;                     ///< ID or name of item response is in regards to.
    Retainer<MusicThingie *> related;          ///< Music item to send, or in regards to.
    mutable const class User *user = nullptr;  ///< User triggering the response.

public:
    inline bool isStatusChange() const {
        return ::isStatusChange (message);
    }
    inline bool isDataField() const {
        return ::isDataField (message);
    }
    inline bool isSuccess() const {
        return ::isSuccess (message);
    }
    inline bool isCommandError() const {
        return ::isCommandError (message);
    }
    inline bool isServerFailure() const {
        return ::isServerFailure (message);
    }
    inline bool isUserAction() const {
        return ::isUserAction (message);
    }
    /// Associate a user triggering the response (for user actions & yells).
    inline void bindUser (const class User *u) const {
        user = u;
    };

    void transmitLine (Football::Thingie &there, int offset = TransmitUnaltered) const;
    Parsnip::Data serialize() const;

    Response() = default;
    Response (const Response &) = default;
    Response (Response &&) = default;
    Response &operator= (const Response &) = default;
    Response &operator= (Response &&) = default;

    Response (RESPONSE_CODE msg);
    Response (RESPONSE_CODE msg, const std::string &details);
    Response (RESPONSE_CODE msg, long details);
    Response (RESPONSE_CODE msg, double details, int precision);
    Response (RESPONSE_CODE msg, List &&details, Parsnip::Data &&json_details);

    Response (const std::string &regard, RESPONSE_CODE msg);
    Response (const std::string &regard, RESPONSE_CODE msg, const std::string &details);
    Response (const std::string &regard, RESPONSE_CODE msg, long details);
    Response (const std::string &regard, RESPONSE_CODE msg, double details, int precision);
    Response (const std::string &regard, RESPONSE_CODE msg, List &&details, Parsnip::Data &&json_details);

    Response (Retainer<MusicThingie *> rel, RESPONSE_CODE msg);
    Response (Retainer<MusicThingie *> rel, RESPONSE_CODE msg, const std::string &details);
    Response (Retainer<MusicThingie *> rel, RESPONSE_CODE msg, long details);
    Response (Retainer<MusicThingie *> rel, RESPONSE_CODE msg, double details, int precision);
    Response (Retainer<MusicThingie *> rel, RESPONSE_CODE msg, List &&details, Parsnip::Data &&json_details);
    
    Response (const CommandError &err);
    Response (const std::string &regard, const CommandError &err);
    Response (Retainer<MusicThingie *> rel, const CommandError &err);
};

/// Use << as output operator for `Response`.
inline Football::Thingie &operator<< (Football::Thingie &there, const Response &response) {
    response.transmitLine (there);
    return there;
}

/// Use << as output operator for `Response`.
inline Football::Thingie &operator<< (Football::Thingie *there, const Response &response) {
    response.transmitLine (*there);
    return *there;
}

extern PianodConnection &operator<< (PianodConnection &there, const Response &response);

/// Use << as output operator for `Response`.
inline PianodConnection &operator<< (PianodConnection *there, const Response &response) {
    return (*there << response);
}

/** Container for multiple `Response`s.
    May contain any type, but for data, success and command errors
    contents should be consistent.  Others types may mix.
    For performance, this avoids copy constructors/adders and provides
    only move constructors/adders. */
class ResponseGroup : public std::vector<Response> {
    friend class ResponseCollector;

private:
    Parsnip::Data serializeReply() const;
    Parsnip::Data serializeData (const User *) const;
    Parsnip::Data serializeHomogenousData (const User *) const;
    Parsnip::Data serializeEvents (bool, const User *) const;

    void transmitLine (class PianodService &) const;
    void transmitJSON (class PianodService &) const;

public:
    ResponseGroup() = default;
    ResponseGroup (ResponseGroup &&) = default;
    ResponseGroup &operator= (ResponseGroup &&) = default;

    /// Construct from a single response.
    ResponseGroup (Response &&r) {
        push_back (std::move (r));
    }
    /// Add a new response to the list, after constructing it.
    template <typename... Parameters>
    inline void operator() (Parameters &&...params) {
        push_back (Response (std::forward<Parameters> (params)...));
    }

    void operator() (ResponseGroup &&);

    void transmit (class PianodService &) const;

    void transmitLine (PianodConnection &, int offset = Response::TransmitUnaltered) const;
    void transmit (class PianodConnection &) const;
};

/** A response collector/aggregator.
    This class implements a "funnel" pattern.
    Whatever is coming back from user commands---success indications,
    error messages, status updates, and data---is passed back
    through this structure, then serialized into a message and sent.
    Note: This class is used to return data, but the child DataResponse
    class is used to construct data responses. */
class ResponseCollector {
    friend class PianodConnection &operator<< (class PianodConnection &there, const ResponseCollector &response);

private:
    bool close_after_response = false;  ///< If true, the connection is closed after response is sent.

    Parsnip::Data serialize (const User *user) const;
    void transmitJSON (PianodConnection &) const;
    void transmitLine (PianodConnection &) const;

protected:
    RESPONSE_CODE reason{ S_NOOP };  ///< The final response that will be sent.

    ResponseGroup successes;    ///< Success messages
    ResponseGroup diagnostics;  ///< Details of errors encountered.

    std::vector<ResponseGroup> data_groups;  ///< Complete data records destined for the client.
    ResponseGroup data_reply;                ///< Discrete data responses destined for the client.

    // These are not publicly  accessible through ResponseCollector; use DataResponse instead.
    void data (ResponseGroup &&group);
    void data (const ThingieList &item_list);
    void data (const SongList &songs);
    void data (const PlaylistList &playlists);

    ResponseGroup user_events;       ///< Broadcast to other same-user sessions
    ResponseGroup room_events;       ///< Broadcast to all same-room sessions
    ResponseGroup broadcast_events;  ///< Broadcast to all users, all rooms
public:
    ResponseGroup information;  ///< Session notices going back to user

    inline void setNoResponse() {
        assert (noop() && reason != S_DATA);
        reason = NO_REPLY;
    }

    /// ResponseCollector contains successes but no failures.
    inline bool allSuccess() const {
        assert (data_reply.empty());
        return diagnostics.empty() && !successes.empty();
    };
    /// ResponseCollector contains failures but no successes.
    inline bool allFailure() const {
        assert (data_reply.empty());
        return !diagnostics.empty() && successes.empty();
    }
    /// ResponseCollector contains at least one success, regardless of failures.
    inline bool anySuccess() const {
        assert (data_reply.empty());
        return !successes.empty();
    };
    /// ResponseCollector contains at least one failure, regardless of successes.
    inline bool anyFailure() const {
        assert (data_reply.empty());
        return !diagnostics.empty();
    }
    /// ResponseCollector contains a mix of successes and failures.
    inline bool partial() const {
        assert (data_reply.empty());
        return !diagnostics.empty() && !successes.empty();
    }
    /// ResponseCollector contains neither successes nor failures.
    inline bool noop() const {
        return diagnostics.empty() && successes.empty();
    }
    /// ResponseCollector is transporting data for transmission.
    inline bool dataResponse() const {
        return (reason == S_DATA);
    }
    /// ResponseCollector should not send a reply to a command.
    inline bool isNoResponse() const {
        return (reason == NO_REPLY);
    }
    /// Causes the connection to be closed after the response is sent.
    inline void close() {
        close_after_response = true;
    }

    void transmit (PianodConnection &, const bool json);

    ResponseCollector() = default;
    ResponseCollector (ResponseCollector &&from) = default;
    ResponseCollector &operator =(ResponseCollector &&from) = default;
    
    ResponseCollector (Response &&initial);
    inline ResponseCollector (const RESPONSE_CODE response) : ResponseCollector (Response{ response }){};
    ResponseCollector (MusicThingie *item);
    ResponseCollector (const ThingieList &things);
    ResponseCollector (const SongList &songs);
    ResponseCollector (ResponseGroup &&group);
};

/** This class is derived from ResponseCollector and adds success/failure adder
    functions and a few variables to support this.  The name makes its purpose
    clearer, the interface avoids data mixing with successes/failures, which
    is invalid.  The cost is needing std::move() whereever we return as a
    ResponseCollector; when this is done, we are indeed slicing the object. */
class CommandReply : public ResponseCollector {
public:
    enum class Aggregation { PESSIMISTIC, OPTIMISTIC };

private:
    RESPONSE_CODE mixed_reason{ E_PARTIAL };  ///< The final response if there are both successes and failures.
    void mergeReason (RESPONSE_CODE why);

public:
    using ResponseCollector::information;
    using ResponseCollector::user_events;
    using ResponseCollector::room_events;
    using ResponseCollector::broadcast_events;

    void fail (Response &&failure);
    /// Add a failure to the failures list, after constructing it.
    template <typename ParamOne, typename ParamTwo, typename... Additional>
    inline void fail (ParamOne &&first, ParamTwo &&second, Additional &&...more) {
        fail (Response (std::forward<ParamOne> (first),
                        std::forward<ParamTwo> (second),
                        std::forward<Additional> (more)...));
    }

    void succeed (Response &&success);
    /// Add a simple success to the list.
    inline void succeed() {
        succeed (S_OK);
    }
    /// Add a succeess to the successes list, after constructing it.
    template <typename ParamOne, typename ParamTwo, typename... Additional>
    inline void succeed (ParamOne &&first, ParamTwo &&second, Additional &&...more) {
        succeed (Response (std::forward<ParamOne> (first),
                           std::forward<ParamTwo> (second),
                           std::forward<Additional> (more)...));
    }

    void operator() (Response &&r);
    /// Construct a response and add it to the successes, failures or data list based on its type.
    template <typename ParamOne, typename... Parameters>
    inline void operator() (ParamOne &&first, Parameters &&...more) {
        operator() (Response (std::forward<ParamOne> (first), std::forward<Parameters> (more)...));
    }

    CommandReply (Aggregation kind = Aggregation::OPTIMISTIC);
    inline CommandReply (Response &&initial) : ResponseCollector (std::move (initial)){};
    CommandReply (const std::exception_ptr except, const char *triggering_command = nullptr);
};

/** This class is derived from ResponseCollector, and adds no member variables
    but adds data adder functions.  The name makes what we're returning more
    explicit, and the interface makes it more difficult to mix data with
    successes/failures, which is invalid.  The cost is needing std::move()
    whereever we return as a ResponseCollector.  */
class DataResponse : public ResponseCollector {
public:
    using ResponseCollector::data;
    using ResponseCollector::information;

    void data (Response &&r);

    /// Add data to the data response list, after constructing it.
    template <typename ParamOne, typename ParamTwo, typename... Additional>
    inline void data (ParamOne &&first, ParamTwo &&second, Additional &&...more) {
        data (Response (std::forward<ParamOne> (first),
                        std::forward<ParamTwo> (second),
                        std::forward<Additional> (more)...));
    }

    /// Add a music item to the data response list.
    template <typename T>
    inline void data (const Retainer<T> &from) {
        ResponseGroup group;
        group (from, I_ATTACHED_THING);
        data_groups.push_back (std::move (group));
    }

    /** Construct a new DataResponse (i.e., a ResponseCollector that's
        been earmarked for returning data. */
    inline DataResponse() : ResponseCollector (S_DATA) {
    }
};

extern class PianodConnection &operator<< (class PianodConnection &there, const ResponseCollector &response);

/// Use << as output operator for `ResponseCollector`.
inline class PianodConnection &operator<< (class PianodConnection *there, const ResponseCollector &response) {
    *there << response;
    return *there;
}

// Helper for sending updated ratings.
void sendUpdatedRatings (PianodConnection &conn, const PianodSong *song, CommandReply *reply);
