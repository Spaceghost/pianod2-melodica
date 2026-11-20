///
/// Handle commands controlling the overall pianod deamon/service.
/// Manages "rooms": separate services each with its own audio etc.
/// @file       servicemanager.cpp - pianod project
/// @author     Perette Barella
/// @date       Initial: 2012-03-10.  C++ Rework: 2014-10-27.
/// @copyright  Copyright 2012–2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cstdio>
#include <cassert>
#include <ctime>
#include <sstream>
#include <iomanip>

#include <functional>

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include <football/football.h>
#include <parsnip/parsnip_schema.h>

#include "fundamentals.h"
#include "lookup.h"
#include "utility.h"
#include "sources.h"
#include "connection.h"
#include "engine.h"
#include "response.h"
#include "user.h"
#include "users.h"
#include "servicemanager.h"
#include "mediamanager.h"
#include "audiooptionsparser.h"
#include "musiclibraryparameters.h"

// These are only needed for constructing the master parser:
#include "tuner.h"
#include "predicate.h"

/// Construct a parser for use by all rooms.
static Parsnip::ParserRef construct_parser() {
    PianodParser *aggregate = new PianodParser;
    Parsnip::ParserRef parser (aggregate);

    aggregate->addOptionParser (KEY_AUDIOOPTIONS, AudioOptions::parser_definitions());
    aggregate->addOptionParser (KEY_WAITOPTIONS, WaitOptions::parser_definitions());
    aggregate->addOptionParser (KEY_TUNEROPTIONS, Tuner::autotuning_option_parser_definitions());
    aggregate->addOptionParser (KEY_LIBRARYOPTIONS, MusicLibrary::LibraryParameters::parser_definitions());
    aggregate->addOptionParser (PARSER_SOURCEIDENTITY, Media::Manager::source_id_parser_definitions());
    Predicate::construct_predicate_parsers (aggregate);
    aggregate->addOptionParser (KEY_PLAYBACKSELECTION, AudioEngine::playback_selection_option_definitions());

    aggregate->addStatements (Media::Manager::parser_definitions());
    aggregate->addStatements (ServiceManager::parser_definitions());
    aggregate->addStatements (Tuner::parser_definitions());
    aggregate->addStatements (AudioEngine::parser_definitions());
    aggregate->addStatements (UserManager::parser_definitions());

    Sources::registerCommands (aggregate);
    return parser;
}

enum class LoggingAction {
    SET,
    ENABLE,
    DISABLE,
    TOGGLE
};

static const LookupTable<LoggingAction> LoggingActions {
    { "set", LoggingAction::SET },
    { "enable", LoggingAction::ENABLE },
    { "disable", LoggingAction::DISABLE },
    {"toggle", LoggingAction::TOGGLE }
};

static std::initializer_list <StandardLookupValues<LogType>> LoggingFlagValues {
    { "state", Log::LOG_000 },
    { "information", Log::LOG_100 },
    { "successes", Log::LOG_200 },
    { "diagnostics", Log::LOG_300 },
    { "errors", Log::LOG_400 },
    { "failures", Log::LOG_500 },
    { "general", Log::GENERAL },
    { "queue", Log::QUEUE },
    { "warnings", Log::WARNING },
    { "actions", Log::USERACTION },
    { "protocol", Log::PROTOCOL },
    { "allocations", Log::ALLOCATIONS },
    { "caches", Log::CACHES },
    { "tuning", Log::TUNING },
    { "biasing", Log::BIASING },
    { "metadata", Log::METADATA },
    { "audio", Log::AUDIO },
    { "pandora", Log::PANDORA },
    { "filesystem", Log::FILESYSTEM }
};
static const LookupTable<LogType> LoggingFlags { LoggingFlagValues };

typedef enum service_commands_t {
    NOP = CMD_RANGE_SERVICE,
    HELP,
    SCHEMAHELP,
    YELL,
    QUIT,
    NEWROOM,
    DELETEROOM,
    ROOMINFO,
    CHOOSEROOM,
    INROOMEXEC,
    LISTROOMS,
    SHUTDOWN,
    GETLOGGINGFLAGS,
    SETLOGGINGFLAGS,
    ALTERLOGGINGFLAGS,
    SHOWUSERACTIONS,
    SYNC,
    UPTIME
} SERVICECOMMAND;

/// Request all services (rooms) close so pianod can shutdown.
/// @param immediate If true, aborts playback in all rooms.
void ServiceManager::shutdown (bool immediate) {
    if (!shutdown_pending) {
        shutdown_pending = true;
        for (auto service : *this) {
            service.second->audioEngine()->shutdown (immediate);
        }
        broadcast (Response (F_SHUTDOWN, "Shutdown pending."));
    }
}

/// Persist data before shutdown
bool ServiceManager::flush (void) {
    bool status = user_manager->persist();
    status = media_manager->flush() && status;
    return status;
}

/** Perform periodic duties, and determine when next ones are due.
    All sources, the audio engines (rooms) and users are invoked,
    allowing them to perform periodic duties (freeing resources,
    persisting data, background tasks). */
float ServiceManager::periodic (void) {
    float next_request = A_LONG_TIME;
    for (auto service : *this) {
        float next_req = service.second->engine->periodic();
        if (next_req < next_request)
            next_request = next_req;
    }
    // Defer media unit/user periodics if the audio engines want attention soon.
    if (next_request > 3) {
        float next_req = media_manager->periodic();
        if (next_req < next_request)
            next_request = next_req;
        next_req = user_manager->periodic();
        if (next_req < next_request)
            next_request = next_req;
    }
    // Deal with WAIT FOR event timeouts.
    time_t now = time (nullptr);
    if (now >= WaitEvent::nextTimeout) {
        WaitEvent::nextTimeout = FAR_FUTURE;
        for (auto service : *this) {
            for (auto conn : *(service.second)) {
                conn->checkTimeouts();
            }
        }
    }
    if (WaitEvent::nextTimeout != FAR_FUTURE) {
        now = time (nullptr);
        if (WaitEvent::nextTimeout - now < next_request) {
            next_request = WaitEvent::nextTimeout - now;
        }
    }

    return (shutdown_pending ? 1 : next_request < 0 ? 0 : next_request);
}

/** Distribute an event to connections waiting on it.
    @param type The type of event.
    @param detail Pointer representing a specific event instance.
    @param reply Status code to report to connections waiting for the event. */
void ServiceManager::event (WaitEvent::Type type, const void *detail, RESPONSE_CODE reply) {
    assert (type != WaitEvent::Type::None);
    for (auto &svc : *this) {
        for (auto conn : *(svc.second)) {
            conn->event (type, detail, reply);
        }
    }
}

/** Send a message to every user on all services/rooms
    @param messages The message(s) to broadcast. */
void ServiceManager::broadcast (const ResponseGroup &messages) {
    if (!messages.empty()) {
        for (auto &svc : *this) {
            messages.transmit (*(svc.second));
        }
    }
}

/** Send a message to one user on all services/rooms
    @param messages The messages to broadcast.
    @param target The user to send the message to.  If `nullptr`,
    broadcasts to visitors. */
void ServiceManager::broadcast (const ResponseGroup &messages, User *target) {
    if (!messages.empty()) {
        for (auto &svc : *this) {
            for (auto conn : *(svc.second)) {
                if (target == conn->user) {
                    messages.transmit (*conn);
                }
            }
        }
    }
}

/** Broadcast privileges to all users. */
void ServiceManager::broadcastEffectivePrivileges() {
    for (auto &svc : *this) {
        for (auto conn : *(svc.second)) {
            ResponseGroup (conn->sendEffectivePrivileges()).transmit (*conn);
        }
    }
}

/** Broadcast privileges to a user.
    @param target The target user, or NULL to target visitors. */
void ServiceManager::broadcastEffectivePrivileges (User *target) {
    for (auto &svc : *this) {
        for (auto conn : *(svc.second)) {
            if (target == conn->user) {
                ResponseGroup (conn->sendEffectivePrivileges()).transmit (*conn);
            }
        }
    }
}

/** Send events to those waiting on source ready, either a specific source,
    any source, or all sources.
    @param source The source that is transitioning from pending to ready or dead.
    @param result The status of the source's transition. */
void ServiceManager::sendSourceReadyEvents (const Media::Source * const source, RESPONSE_CODE result) {
    // Waiting on specific source
    event (WaitEvent::Type::SourceReady, source, result);

    // Waiting on any source)
    event (WaitEvent::Type::SourceReady, media_manager, result);

    // Waiting on all sources ready
    if (!media_manager->areSourcesPending()) {
        event (WaitEvent::Type::SourceReady, nullptr, media_manager->areSourcesReady() ? S_OK : E_NAK);
    }
}

/** Prepare for a new source by alerting auto engines of it. */
void ServiceManager::sourceReady (const Media::Source * const source) {
    sendSourceReadyEvents (source, S_OK);
}

/** Handle a source going offline by removing it from use.  Unlike
 invalidating sources, however, historical and queue references
 remain. */
void ServiceManager::sourceOffline (const Media::Source * const source) {
    for (auto svc : *this) {
        // Remove source from any connections on the service
        for (auto conn : *(svc.second)) {
            if (conn->source() == source) {
                ResponseGroup { conn->source (media_manager) }.transmit (*conn);
            }
        }
    }
}

/** Handle a source being deleted. */
void ServiceManager::sourceRemoved (const Media::Source * const source) {
    sendSourceReadyEvents (source, E_NAK);
}

/** Add a room name/service pair to our list.
    @param name The name of the room, so users can select it.
    @param audio Audio settings (presumably a certain audio out) for the room.
    @param options Football options for the room's service.
    Each room gets a separate service.*/
PianodService *ServiceManager::createRoom (const std::string &name,
                                           const AudioSettings &audio,
                                           FB_SERVICE_OPTIONS &options) {
    options.queue_size = 5;
    options.greeting_mode = FB_GREETING_ALLOW;
    options.name = (char *) name.c_str();
    PianodService *service = nullptr;
    try {
        service = new PianodService (options, name, audio, master_service, master_parser, master_schema);
        auto result = insert (make_pair (name, service));
        if (result.second) {
            if (!master_service)
                master_service = service;
            return service;
        }
    } catch (...) {
        // Continue on
    }
    if (service)
        delete service;
    return nullptr;
}

/** Remove a room.  Triggers a flush when the last room is removed.
    This is a callback function, invoked when Football calls the
    serviceShutdown() method.
    @param service The room to remove. */
void ServiceManager::removeRoom (PianodService *service) {
    for (iterator it = begin(); it != end(); it++) {
        if (it->second == service) {
            erase (it);
            if (empty())
                flush();
            return;
        }
    }
    assert (0);
    return;
}

/** Determine if a user is authenticated an connected in any room.
    @param user The user to look for.
    @return True if the user is online, false otherwise. */
bool ServiceManager::userIsOnline (const User *user) {
    for (auto const &svc : *this) {
        if (user->online (*svc.second))
            return true;
    }
    return false;
}

const Parsnip::Parser::Definitions &ServiceManager::parser_definitions() {
    // clang-format off
    static const Parsnip::Parser::Definitions statement_list = {
            {NOP, "# [{comment...}]"},                                   // Comment
            {HELP, "help [{command}] ..."},                              // Request help
            {SCHEMAHELP, "schema [{request}] ..."},                        // List JSON requests/show request schema.
            {YELL, "yell {announcement...}"},                            // Broadcast to all connected terminals
            {QUIT, "quit"},                                              // Logoff terminal
            {SHUTDOWN, "shutdown"},                                      // Shutdown the player and quit
            {NEWROOM, "room create {room} [{" KEY_AUDIOOPTIONS ":" KEY_AUDIOOPTIONS "}] ..."}, // Create a room with audio options
            {DELETEROOM, "room delete {room} [when:now]"},               // Delete a room.
            {CHOOSEROOM, "room enter {room}"},                           // Switch rooms/audio output channel
            {ROOMINFO, "room <info|information> {room}"},                // Query room information
            {INROOMEXEC, "in room {room} [{command...}]"},               // Perform an action in a different room.
            {LISTROOMS, "room list"},                                    // Get a list of rooms
            {SHOWUSERACTIONS, "announce user actions <switch:on|off>"},  // Whether to broadcast events
            {GETLOGGINGFLAGS, "logging"},                                // Output logging details
            {SETLOGGINGFLAGS, "set [which:pianod|football] logging flags {#logging-flags:0x0-0xffffff}"},
            {ALTERLOGGINGFLAGS, "logging <action:set|enable|disable|toggle> {feature} ..."},
            {SYNC, "sync [which:all|userdata]"},
            {UPTIME, "uptime"}  // Unofficial
    };
    // clang-format on
    return statement_list;
}

/** Retrieve names for our JSON requests, and make schema revisions.
    @param schema The schemaset containing our requests.
    @return Request name to command ID mappings. */
const PianodSchema::CommandIds &ServiceManager::json_request_names (PianodSchema &schema) {
    static const PianodSchema::CommandIds mappings{ { "help", HELP },
                                                    { "getSchema", SCHEMAHELP },
                                                    { "broadcastMessage", YELL },
                                                    { "disconnect", QUIT },
                                                    { "shutdown", SHUTDOWN },
                                                    { "createRoom", NEWROOM },
                                                    { "deleteRoom", DELETEROOM },
                                                    { "selectRoom", CHOOSEROOM },
                                                    { "getRoomInfo", ROOMINFO },
                                                    { REQUEST_INROOM, INROOMEXEC },
                                                    { "listRooms", LISTROOMS },
                                                    { "broadcastUsersActions", SHOWUSERACTIONS },
                                                    { "setLoggingFlags", SETLOGGINGFLAGS },
                                                    { "adjustLogging", ALTERLOGGINGFLAGS },
                                                    { "getLogging", GETLOGGINGFLAGS },
                                                    { "sync", SYNC },
                                                    { "getUptime", UPTIME } };
    schema.removeMember (INROOMEXEC, KEY_COMMAND);
    schema.addMember (INROOMEXEC, KEY_REQUEST, Parsnip::UncheckedSchema{});
    return mappings;
}

const Parsnip::Parser::Definitions &ServiceManager::getParserDefinitions() {
    return parser_definitions();
}

bool ServiceManager::authorizedCommand (Parsnip::Parser::CommandId command, PianodConnection &conn) {
    switch (command) {
        case NOP:
        case HELP:
        case SCHEMAHELP:
        case QUIT:
            return true;
        case YELL:
        case CHOOSEROOM:
        case INROOMEXEC:
        case LISTROOMS:
        case UPTIME:
        case SYNC:
        case GETLOGGINGFLAGS:
            return conn.haveRank (Rank::Listener);
        default:
            return conn.haveRank (Rank::Administrator);
    }
};

ResponseCollector ServiceManager::handleCommand (Parsnip::Parser::CommandId command,
                                                 const Parsnip::Data &options,
                                                 PianodConnection &conn) {
    switch (command) {
        case HELP: {
            static const PianodDispatcher::HelpList help_all;
            PianodDispatcher::HelpList help = conn.service().dispatch.getHelp (
                    options.contains ("command") ? options ["command"].toList<std::string>() : help_all);
            if (help.empty()) {
                throw CommandError (E_NOTFOUND);
            }
            DataResponse response;
            for (auto item : help) {
                response.data (Response (I_INFO, item));
            }
            return std::move (response);
        }
        case SCHEMAHELP: {
            PianodSchema::SchemaList help;
            const auto requests = options.getOr ("request", EmptyStringVector);
            if (!requests.empty()) {
                for (const auto &request : requests) {
                    std::ostringstream schema;
                    master_schema->dump (request, schema);
                    StringVector split_schema = split_string (schema.str(), "\n", false);
                    std::copy (split_schema.begin(), split_schema.end(), std::back_inserter (help));
                }
            } else {
                help = master_schema->allRequestNames();
            }
            DataResponse response;
            response.data (Response (I_INFO, "All requests take the form:"));
            response.data (Response (I_INFO, "  {\"requestname\": {request parameters/options...}}"));
            for (auto item : help) {
                response.data (Response (I_INFO, item));
            }
            return std::move (response);
        }
        case QUIT: {
            CommandReply response (S_OK);
            response.close();
            return std::move (response);
        }
        case SHOWUSERACTIONS:
            broadcastUserActions (optionIs (options, "switch", "on"));
            return S_OK;
        case NOP:
            return S_OK;
        case SHUTDOWN: {
            // Commence a server shutdown, which will take effect after the current song.
            shutdown (false);
            CommandReply response (S_OK);
            response.broadcast_events (A_SHUTDOWN);
            return std::move (response);
        }
        case YELL: {
            CommandReply response (S_OK);
            response.broadcast_events (V_YELL, options ["announcement"].asString());
            return std::move (response);
        }
        case CHOOSEROOM: {
            iterator new_room = find (options ["room"].asString());
            if (new_room == end()) {
                throw CommandError (E_NOTFOUND);
            } else {
                PianodService *old_room = &conn.service();
                if (conn.transfer (new_room->second, true)) {
                    if (old_room != new_room->second) {
                        old_room->usersChangedNotification();
                        new_room->second->usersChangedNotification();
                    }
                } else {
                    throw CommandError (E_NAK);
                };
            }
            return S_OK;
        }
        case INROOMEXEC: {
            PianodService *original_room = &conn.service();
            iterator item = find (options ["room"].asString());
            if (item == end()) {
                throw CommandError (E_NOTFOUND);
            } else if (!conn.transfer (item->second, false)) {
                throw CommandError (E_NAK);
            }
            ResponseCollector response;
            try {
                response = conn.service().dispatch.redispatch (options, conn);
            } catch (...) {
                response = CommandReply (std::current_exception());
            }
            kept_assert (conn.transfer (original_room, false));
            return response;
        }
        case DELETEROOM: {
            iterator item = find (options ["room"].asString());
            if (item == end()) {
                throw CommandError (E_NOTFOUND);
            } else if (strcasecmp (item->second->room_name, "pianod") == 0) {
                throw CommandError (E_UNSUPPORTED, "Cannot remove initial room.");
            }
            item->second->audioEngine()->shutdown (optionIs (options, "when", "now"));
            return S_OK;
        }
        case ROOMINFO: {
            iterator item = find (options ["room"].asString());
            if (item == end()) {
                throw CommandError (E_NOTFOUND);
            }
            const AudioSettings &audio = item->second->audioEngine()->audioSettings();

            DataResponse response;
            response.data (Response (I_INFO, "library: " + audio.output_library));
            response.data (Response (I_INFO, "device: " + audio.output_device));
            response.data (Response (I_INFO, "driver: " + audio.output_driver));
            response.data (Response (I_INFO, "options: " + audio.output_options));
            response.data (Response (I_INFO, "server: " + audio.output_server));
            response.data (Response (I_INFO, "options: " + audio.output_options));
            response.data (Response (I_INFO, "crossfade level: " + std::to_string (audio.crossfade_level)));
            response.data (Response (I_INFO, "crossfade time: " + std::to_string (audio.crossfade_time)));
            response.data (Response (I_INFO, "options: " + audio.output_options));
            return std::move (response);
        }
        case NEWROOM: {
            if (shutdown_pending) {
                throw CommandError (E_NAK, "Shutdown is pending");
            }
            FB_SERVICE_OPTIONS room_options = {};
            room_options.transfer_only = true;
            AudioSettings audio = {};
            if (options.contains (KEY_AUDIOOPTIONS)) {
                AudioOptions::extract_options (options [KEY_AUDIOOPTIONS], audio);
            }
            return (createRoom (options ["room"].asString(), audio, room_options) ? S_OK : E_NAK);
        }
        case LISTROOMS: {
            DataResponse response;
            for (auto const &room : *this) {
                response.data (Response (I_ROOM, room.second->roomName()));
            }
            return std::move (response);
        }
        case SETLOGGINGFLAGS: {
            long flags = options ["logging-flags"].as<long>();
            if (optionIs (options, "which", "football")) {
                fb_set_logging (flags, NULL);
            } else {
                set_logging (static_cast <LogType> (flags));
            }
            return S_OK;
        }
        case GETLOGGINGFLAGS: {
            LogType flags = get_logging();
            Response::List enabled_list;
            enabled_list.push_back ("enabled: ");
            Response::List disabled_list;
            disabled_list.push_back ("disabled: ");
            Parsnip::Data json_list { Parsnip::Data::Dictionary };
            for (const auto &it : LoggingFlagValues) {
                bool enabled = (it.value & flags) != LogType::ERROR;
                json_list [it.name] = enabled;
                if (enabled) {
                    enabled_list.push_back (it.name);
                } else {
                    disabled_list.push_back (it.name);
                }
            }
            DataResponse response;
            response.data (I_INFO, std::move (enabled_list), std::move (json_list));
            if (!conn.transmitJSON()) {
                response.data (I_INFO, std::move (disabled_list), std::move (json_list));
            }
            return std::move (response);
        }
        case ALTERLOGGINGFLAGS: {
            LoggingAction action = LoggingActions [options ["action"].asString()];
            LogType alteration = Log::ERROR;
            auto features = options ["feature"].as<std::vector<std::string>>();
            for (const std::string &feature : features) {
                alteration = alteration ^ LoggingFlags [feature];
            }
            switch (action) {
                case LoggingAction::SET:
                    set_logging (alteration);
                    break;
                case LoggingAction::ENABLE:
                    set_logging (get_logging() | alteration);
                    break;
                case LoggingAction::DISABLE:
                    set_logging (get_logging() - alteration);
                    break;
                case LoggingAction::TOGGLE:
                    set_logging (get_logging() ^ alteration);
                    break;
            }
            return S_OK;
        }
        case SYNC: {
            bool status = (optionIs (options, "which", "userdata") ? user_manager->persist() : flush());
            return (status ? S_OK : E_NAK);
        }
        case UPTIME: {
            time_t now = time (nullptr);
            time_t duration = now - startup_time;
            int secs = duration % 60;
            duration /= 60;
            int mins = duration % 60;
            duration /= 60;
            int hours = duration % 24;
            duration /= 24;

            char *since = ctime (&startup_time);
            if (!since) {
                throw CommandError (E_RESOURCE);
            }

            since [24] = '\0';
            DataResponse response;
            std::ostringstream buffer;
            buffer << std::fixed << std::setfill ('0');
            // clang-format off
            buffer << "Up since " << since << " (" << duration
                   << "d+" << std::setw (2) << hours
                   << ":" << std::setw (2) << mins
                   << ":" << std::setw (2) << secs << ")";
            // clang-format on
            response.data (I_INFO, buffer.str());
#if defined(HAVE_GETRUSAGE) && defined(HAVE_SYS_RESOURCE_H)
            struct rusage usage;
            if (getrusage (RUSAGE_SELF, &usage) == 0) {
                buffer.str ("");
                buffer.clear();
                // clang-format off
                buffer << "Compute time=" << usage.ru_utime.tv_sec / 60
                       << "m" << std::setw (2) << usage.ru_utime.tv_sec % 60
                       << "." << std::setw (2) << usage.ru_utime.tv_usec / 10000
                       << "s (user, " << usage.ru_stime.tv_sec / 60
                       << "m" << std::setw (2) << usage.ru_stime.tv_sec % 60
                       << "." << std::setw (2) << usage.ru_stime.tv_usec / 10000
                       << " (system)";
                // clang-format on
                response.data (I_INFO, buffer.str());
            }
#endif
            buffer.str ("");
            buffer.clear();
            char *nowtime = ctime (&now);
            if (!nowtime) {
                throw CommandError (E_RESOURCE);
            }
            nowtime [24] = '\0';
            buffer << "Current local time " << nowtime;
            response.data (I_INFO, buffer.str());
            return std::move (response);
        }
        default:
            flog (LOG_WHERE (Log::WARNING), "Unimplemented command ", command);
            throw CommandError (E_NOT_IMPLEMENTED, std::to_string (command));
    }
}

ServiceManager::ServiceManager()
: master_parser (construct_parser()), master_schema (new PianodSchema (*master_parser)) {
    master_schema->addRequestNames (Media::Manager::json_request_names (*master_schema));
    master_schema->addRequestNames (ServiceManager::json_request_names (*master_schema));
    master_schema->addRequestNames (Tuner::json_request_names());
    master_schema->addRequestNames (AudioEngine::json_request_names());
    master_schema->addRequestNames (UserManager::json_request_names (*master_schema));
    Sources::addRequestNames (*master_schema);


    Media::Manager::Callbacks callbacks;
    callbacks.sourceReady = std::bind (&ServiceManager::sourceReady, this, std::placeholders::_1);
    callbacks.sourceOffline = std::bind (&ServiceManager::sourceOffline, this, std::placeholders::_1);
    callbacks.sourceRemoved = std::bind (&ServiceManager::sourceRemoved, this, std::placeholders::_1);
    media_manager->callback.subscribe (this, callbacks);
}

ServiceManager::~ServiceManager() {
    media_manager->callback.unsubscribe (this);
}

/* Global */ ServiceManager *service_manager{ nullptr };
