///
/// Pianod specializations of Football connections & services.
/// @file       connection.h - pianod
/// @author     Perette Barella
/// @date       2014-11-28
/// @copyright  Copyright (c) 2014-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <football/football.h>
#include <parsnip/parsnip_command.h>

#include "fundamentals.h"
#include "response.h"
#include "interpreter.h"
#include "user.h"

class PianodService;
namespace Media {
    class Source;
}
//class ThingieList;

/// Details of an event being awaited.
class WaitEvent {
public:
    /// Kinds of events that can be waited for.
    enum class Type {
        None,
        SourceReady,
        TrackEnded,
        TrackStarted
    };
    Type event = Type::None; ///< The kind of event being awaited.
    const void *parameter; ///< Identifies a specific event instance.
    time_t timeout = 0; ///< Time at which to stop waiting.
    bool close_after_event = false; ///< True if connection is 'AS USER' and should closed after event.

    ///< Next timeout time, here to avoid unnecessary recalculation.
    static time_t nextTimeout;
};

namespace WaitOptions {
    extern const Parsnip::OptionParser::Definitions &parser_definitions ();
    extern void extract_options (const Parsnip::Data &options, WaitEvent &dest);
};


/** Connection to a pianod client, along with context and state
 of that connection. */
class PianodConnection : public Football::Connection {
    // Override original event handlers 
    virtual void newConnection (const FB_EVENT *event) override;
    virtual void connectionClose (const FB_EVENT *event) override;
    virtual void inputReceived (const FB_EVENT *event) override;

    // Connection state
    Media::Source * _source = nullptr; ///< The current source.
    WaitEvent pending; ///< An event this connection is waited for.
    bool use_json = false; ///< Set to true if protocol is JSON

public:
    virtual ~PianodConnection () override;
    inline bool transmitJSON () const {
        return use_json;
    }

    static bool json_connections_only (FB_CONNECTION *);
    static bool line_connections_only (FB_CONNECTION *);
    
    // Now add pianod junk
    // Visibility of this item should be adjusted later??? ZZZZZ 
    User *user = nullptr;

    void close_after_events ();
    /** Determine if the connection is authenticated, i.e., has a user.
        @return true if the connection is authenticated. */
    inline bool authenticated (void) const { return user != nullptr; };
    ResponseGroup updateConnection ();
    inline std::string username (void) const { return authenticated() ? user->username() : "A visitor"; };
    /** Get the connection's selected source.
        @return The selected source. */
    inline Media::Source * const source() const { return _source; };
    /** Set the selected source.
        @param source The new source.
        @param announce True (or omitted) to announce the new source in protocol. */
    [[nodiscard]] ResponseGroup source (Media::Source * const source, bool announce = true);
    bool haveRank (Rank rank) const;
    Rank effectiveRank (void) const;
    bool havePrivilege (Privilege priv) const;

    void waitForEventWithOptions (WaitEvent::Type type, const Parsnip::Data &options, const void *detail);
    void waitForEvent (WaitEvent::Type type, const void *detail);
    void event (WaitEvent::Type type, const void *detail, RESPONSE_CODE reply);
    void checkTimeouts ();

    [[nodiscard]] ResponseGroup sendSelectedSource () const;
    [[nodiscard]] ResponseGroup sendEffectivePrivileges () const;
    void announceToRoom (ResponseGroup &&announcement) const;
    void announceToAll (ResponseGroup &&announcement) const;

    inline PianodService &service (void) const {
        return *((PianodService *) Football::Connection::service());
    }
};


class AudioEngine;
class ServiceManager;

/// Pianod service, a customized FootballService for Pianod connections.
class PianodService : public Football::Service<PianodConnection> {
    friend class ServiceManager;
private:
    AudioEngine *engine = nullptr;
    const std::string room_name;
public:
    PianodDispatcher dispatch;

    ~PianodService ();
    PianodService (const FB_SERVICE_OPTIONS &options,
                   const std::string &room,
                   const AudioSettings &audio,
                   PianodService *parent,
                   const Parsnip::ParserRef &parser,
                   const PianodSchemaRef &schema);
    inline AudioEngine *audioEngine() { return engine; };
    void serviceShutdown (void);
    inline const std::string &roomName (void) { return room_name; };
    void usersChangedNotification (void);
    void announceToRoom (const Response &message);
    void announceToRoom (const ResponseGroup &message);
};

