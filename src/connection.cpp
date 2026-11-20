///
/// Pianod specializations of Football connections & services.
/// @file       connection.cpp - pianod
/// @author     Perette Barella
/// @date       2014-11-28
/// @copyright  Copyright 2014-2021 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <exception>
#include <algorithm>

#include <cassert>

#include <parsnip/parsnip.h>
#include <parsnip/parsnip_command.h>
#include <football/football.h>

#include "fundamentals.h"
#include "logging.h"
#include "response.h"
#include "interpreter.h"
#include "connection.h"
#include "enum.h"
#include "musictypes.h"
#include "musickeys.h"
#include "mediamanager.h"
#include "servicemanager.h"
#include "engine.h"
#include "users.h"
#include "sources.h"

time_t WaitEvent::nextTimeout = FAR_FUTURE;

PianodConnection::~PianodConnection() {
}

PianodService::PianodService (const FB_SERVICE_OPTIONS &options,
                              const std::string &room,
                              const AudioSettings &audio,
                              PianodService *parent,
                              const Parsnip::ParserRef &parser,
                              const PianodSchemaRef &master_schema)
: Service (options, parent),
  engine (new AudioEngine (this, audio)),
  room_name (room),
  dispatch (parser, master_schema) {
    // Add in the various command sets.
    service_manager->registerInterpreter (dispatch);
    engine->registerInterpreter (dispatch);
    user_manager->registerInterpreter (dispatch);
    media_manager->registerInterpreter (dispatch);

    Sources::registerInterpreters (dispatch);
};

/// Parser for options for events/WAIT commands.
namespace WaitOptions {

#define KEY_WAIT_TIMEOUT "duration"

    /// Create a parser
    const Parsnip::OptionParser::Definitions &parser_definitions() {
        static const Parsnip::OptionParser::Definitions wait_option_statements = {
            "timeout {#" KEY_WAIT_TIMEOUT ":0-999999999}",  // how long to wait before failing
        };
        return wait_option_statements;
    }

    /// Extract the wait options into the WaitEvent structure.
    void extract_options (const Parsnip::Data &options, WaitEvent *dest) {
        if (options.contains (KEY_WAIT_TIMEOUT)) {
            dest->timeout = time (nullptr) + options [KEY_WAIT_TIMEOUT].asInteger();
        }
    }
}  // namespace WaitOptions

/*
 *              Conditional tranmission callbacks
 */

/** Callback function used to identify JSON connections.
    @param conn A football connection.
    @return True if the connection is JSON. */
bool PianodConnection::json_connections_only (FB_CONNECTION *conn) {
    assert (conn);
    PianodConnection *connection = static_cast<PianodConnection *> (PianodConnection::tryGetFromOld (conn));
    return (connection && connection->use_json);
}

/** Callback function used to identify line protocol connections.
    @param conn A football connection.
    @return True if the connection is line-oriented. */
bool PianodConnection::line_connections_only (FB_CONNECTION *conn) {
    assert (conn);
    PianodConnection *connection = static_cast<PianodConnection *> (PianodConnection::tryGetFromOld (conn));
    return (connection && !connection->use_json);
}

/*
 *              Event handlers
 */

enum class Greeting { FULL, BRIEF, NONE };
static Greeting get_greeting_type (const char *greeting) {
    if (strcasecmp (greeting, "none") == 0) {
        return Greeting::NONE;
    } else if (strcasecmp (greeting, "brief") == 0) {
        return Greeting::BRIEF;
    }
    return Greeting::FULL;
}

void PianodConnection::newConnection (const FB_EVENT *event) {
    Greeting greeting{ Greeting::FULL };
    if (event && event->param_names) {
        // HTTP connection, we have names & values
        for (int i = 0; i < event->param_count; i++) {
            if (strcasecmp (event->param_names [i], "protocol") == 0) {
                use_json = (strcasecmp (event->param_values [i], "json") == 0);
            } else if (strcasecmp (event->param_names [i], "greeting") == 0) {
                greeting = get_greeting_type (event->param_values [1]);
            }
        }
    } else if (event && event->argv) {
        // Line connection, we just have values
        for (const char *const *argv = event->argv; *argv; argv++) {
            if (strcasecmp (*argv, "json") == 0) {
                use_json = true;
            } else if (strncasecmp (*argv, "greeting=", 9) == 0) {
                greeting = get_greeting_type ((*argv) + 9);
            }
        }
    }

    CommandReply response (greeting == Greeting::NONE ? NO_REPLY : S_OK);
    response.information (I_WELCOME);
    if (greeting == Greeting::FULL) {
        response.information (sendEffectivePrivileges());
        response.information (I_ROOM, service().roomName());
        response.information (source (media_manager));
        response.information (service().audioEngine()->assembleStatus());
    } else {
        (void) source (media_manager);
    }
    response.transmit (*this, use_json);
};

ResponseGroup PianodConnection::updateConnection() {
    return service().audioEngine()->updateStatus (*this);
}

void PianodConnection::connectionClose (const FB_EVENT *) {
    // Free user context resources
    if (user) {
        announceToRoom (Response{ A_SIGNED_OUT });
        user = nullptr;
        service().usersChangedNotification();
    }
}

void PianodConnection::inputReceived (const FB_EVENT *event) {
    ResponseCollector response;
    try {
        for (const char *ch = event->command; true; ch++) {
            if (*ch == '{') {
                Parsnip::Data request{ Parsnip::parse_json (event->command) };
                service().dispatch.rewriteJsonRequest (request);
                response = service().dispatch.jsonRequest (request, *this);
                break;
            } else if (ch == NULL || !isspace (*ch)) {
                response = service().dispatch (event->command, *this);
                break;
            }
        }
    } catch (...) {
        response = CommandReply (std::current_exception(), event->command);
    }

    response.transmit (*this, use_json);
}

/*
 *              Setters
 */
ResponseGroup PianodConnection::source (Media::Source * const source, bool announce) {
    _source = source;
    return (announce ? sendSelectedSource() : ResponseGroup{});
};

/*
 *              Getters
 */

/** Get connected user's rank.
    @return User's rank, or visitor rank if not authenticated. */
Rank PianodConnection::effectiveRank (void) const {
    return authenticated() ? user->getRank() : User::getVisitorRank();
}

// Determine if a user or visitor has a rank or better.
bool PianodConnection::haveRank (Rank minimum) const {
    return (effectiveRank() >= minimum);
};

/** Determine if the user has a privilege.
    Visitors cannot be assigned privileges but they may be implied by rank.
    @param priv The privilege to check.
    @return True if the user either has the privilege or it is implied by rank. */
bool PianodConnection::havePrivilege (Privilege priv) const {
    assert (Enum<Privilege>().isValid (priv));
    // Administrators inherently get certain privileges
    if (haveRank (Rank::Administrator) && (priv == Privilege::Service || priv == Privilege::Tuner)) {
        return true;
    }
    if (haveRank (Rank::Standard) && priv == Privilege::Queue)
        return true;
    return authenticated() ? user->havePrivilege (priv) : false;
};

/*
 *              Events
 */

/** Begin waiting for an event on a connection.
    @param type The type of event to wait for.
    @param detail A pointer representing a specific event instance to wait for. */
void PianodConnection::waitForEvent (WaitEvent::Type type, const void *detail) {
    assert (pending.event == WaitEvent::Type::None);
    assert (type != WaitEvent::Type::None);
    pending.event = type;
    pending.parameter = detail;
    acceptInput (false);
}

/** Interpret options and begin waiting for an event on a connection.
    @param type The type of event to wait for.
    @param options Wait-related details from command line.
    @param detail A pointer representing a specific event instance to wait for.
    @return True on success, false if event options are invalid. */
void PianodConnection::waitForEventWithOptions (WaitEvent::Type type,
                                                const Parsnip::Data &options,
                                                const void *detail) {
    assert (pending.event == WaitEvent::Type::None);
    assert (type != WaitEvent::Type::None);
    WaitOptions::extract_options (options, &pending);
    pending.event = type;
    pending.parameter = detail;
    acceptInput (false);
    if (pending.timeout < WaitEvent::nextTimeout) {
        WaitEvent::nextTimeout = pending.timeout;
    }
}

/** Process an event for a connection.
    If waiting for the event, the status is announced and input is resumed.
    If not waiting, or waiting for a different event, nothing happens.
    @param type The type of event occurring.
    @param detail A pointer representing a specific event instance.
    @param reply The status to report if the event applies to the connection. */
void PianodConnection::event (WaitEvent::Type type, const void *detail, RESPONSE_CODE reply) {
    assert (type != WaitEvent::Type::None);
    if (pending.event == type && pending.parameter == detail) {
        acceptInput (true);
        this << Response (reply);
        if (pending.close_after_event) {
            Football::Connection::close();
        }
        pending = WaitEvent{};
    }
}

/** Check if a pending event has timed out.  If so, fire it with failure.
    Otherwise, check then pending event for a closer next timeout time. */
void PianodConnection::checkTimeouts() {
    if (pending.timeout) {
        time_t now = time (nullptr);
        if (pending.timeout <= now) {
            event (pending.event, pending.parameter, E_TIMEOUT);
        } else {
            if (pending.timeout < WaitEvent::nextTimeout) {
                WaitEvent::nextTimeout = pending.timeout;
            }
        }
    }
}

/** Close after events are handled, or now if not waiting on one. */
void PianodConnection::close_after_events() {
    if (pending.event == WaitEvent::Type::None) {
        close();
    } else {
        pending.close_after_event = true;
    }
}

/*
 *              Transmitters
 */

/** Send some messages to all users in a room.  If announcing user actions
    is disabled, any such messages are deleted.
    @param announcements The announcements to send. */
void PianodConnection::announceToRoom (ResponseGroup &&announcements) const {
    if (announcements.empty()) {
        return;
    }
    if (!service_manager->broadcastingActions()) {
        announcements.erase (std::remove_if (announcements.begin(),
                                             announcements.end(),
                                             [] (const Response &ann) -> bool { return ann.isUserAction(); }),
                             announcements.end());
    }
    if (announcements.empty()) {
        return;
    }
    for (const Response &message : announcements) {
        message.bindUser (user);
    }
    service().announceToRoom (announcements);
}

/** Broadcast some messages to all connected sessions, all rooms.
    If announcing user actions is disabled, any such messages are deleted.
    @param announcements The announcements to send. */
void PianodConnection::announceToAll (ResponseGroup &&announcements) const {
    if (announcements.empty()) {
        return;
    }
    if (!service_manager->broadcastingActions()) {
        announcements.erase (std::remove_if (announcements.begin(),
                                             announcements.end(),
                                             [] (const Response &ann) -> bool { return ann.isUserAction(); }),
                             announcements.end());
    }
    if (announcements.empty()) {
        return;
    }
    for (const Response &message : announcements) {
        message.bindUser (user);
    }
    service_manager->broadcast (announcements);
}

/** Report the selected source to the connection. */
ResponseGroup PianodConnection::sendSelectedSource() const {
    assert (_source);
    return Response (V_SELECTEDSOURCE,
                     Response::List{ std::to_string (_source->serialNumber()), _source->kind(), _source->name() },
                     _source->serializeIdentity());
    ;
}

/** Transmit the user's effective privileges. */
ResponseGroup PianodConnection::sendEffectivePrivileges() const {
    Response::List privs{ User::getRankName (effectiveRank()) };
    Parsnip::Data privs_json{ Parsnip::Data::Dictionary, "rank", User::getRankName (effectiveRank()) };
    for (Privilege p : Enum<Privilege>()) {
        privs_json [User::getPrivilegeName (p)] = havePrivilege (p);
        if (havePrivilege (p)) {
            privs.push_back (User::getPrivilegeName (p));
        }
    }
    return Response (I_USER_PRIVILEGES, std::move (privs), std::move (privs_json));
}

/*
 *              PianodService
 */
PianodService::~PianodService() {
    if (engine)
        delete engine;
}

/** When a service has been completely shut down, remove it from the service manager. */
void PianodService::serviceShutdown (void) {
    service_manager->removeRoom (this);
}

void PianodService::usersChangedNotification (void) {
    engine->usersChangedNotification();
}

/** Send a message to all users in a room.
    @param announcement The lone announcement to send. */
void PianodService::announceToRoom (const Response &announcement) {
    assert (!announcement.isUserAction());
    ResponseGroup group;
    group.push_back (announcement);
    announceToRoom (group);
}

/** Send messages to all users in a room.
    @param announcements The announcements to send. */
void PianodService::announceToRoom (const ResponseGroup &announcements) {
    announcements.transmit (*this);
}
