///
/// Handle commands controlling the overall pianod deamon/service.
/// Manages "rooms": separate services each with its own audio etc.
/// @file       servicemanager.h - pianod project
/// @author     Perette Barella
/// @date       2014-12-15
/// @copyright  Copyright 2012-2023 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>
#include <unordered_map>

#include "fundamentals.h"
#include "interpreter.h"
#include "connection.h"
#include "response.h"

namespace Media {
    class Source;
}


/** Contains and manages all the services/rooms. */
class ServiceManager :
public PianodInterpreter,
private std::unordered_map<std::string, PianodService *> {
public:
//    using std::unordered_map<std::string, PianodService *>::size;
    using std::unordered_map<std::string, PianodService *>::begin;
    using std::unordered_map<std::string, PianodService *>::end;
    
    static const Parsnip::Parser::Definitions &parser_definitions ();
    virtual const Parsnip::Parser::Definitions &getParserDefinitions () override;
    static const PianodSchema::CommandIds &json_request_names (PianodSchema &schema);

    bool broadcast_user_actions = false;

private:
    const Parsnip::ParserRef master_parser; ///< A single parser shared by all rooms
    const PianodSchemaRef master_schema; ///< A single schema shared by all rooms
    PianodService *master_service = nullptr; ///< The initial room/service.
    bool shutdown_pending = false; ///< Set when shutdown initiated.
    time_t startup_time = time (nullptr); ///< Time the service manager was instantiated.

    virtual bool authorizedCommand (Parsnip::Parser::CommandId command, PianodConnection &conn) override;
    virtual ResponseCollector handleCommand (Parsnip::Parser::CommandId command, const Parsnip::Data &options, PianodConnection &conn) override;

    void sendSourceReadyEvents (const Media::Source * const source,
                                RESPONSE_CODE result);
    void sourceReady (const Media::Source * const src);
    void sourceOffline (const Media::Source * const src);
    void sourceRemoved (const Media::Source * const src);

public:
    ServiceManager ();
    virtual ~ServiceManager ();
    float periodic(void);

    void shutdown (bool immediate);
    bool flush (void);

    // Events
    void event (WaitEvent::Type type, const void *detail = nullptr, RESPONSE_CODE message = S_OK);
    /// @see event (3 parameter version).
    inline void event (WaitEvent::Type type, RESPONSE_CODE message = S_OK) {
        event (type, nullptr, message);
    };

    // Broadcast things

    /** Enable or disable announcing user actions to all connections.
        @param v True to enable, false to disable. */
    inline void broadcastUserActions (bool v) {
        broadcast_user_actions = v;
    }
    /** Check whether user actions are announced to all connections.
        @return True if enabled, false if not. */
    inline bool broadcastingActions (void) {
        return broadcast_user_actions;
    }

    void broadcast (const ResponseGroup &messages);
    void broadcast (const ResponseGroup &messages, User *target);
    void broadcastEffectivePrivileges ();
    void broadcastEffectivePrivileges (User *target);

    // Room management
    PianodService *createRoom (const std::string &name, const AudioSettings &audio,
                               FB_SERVICE_OPTIONS &options);
    void removeRoom (PianodService *service);

    // User management
    bool userIsOnline (const User *user);
};


extern ServiceManager *service_manager;
