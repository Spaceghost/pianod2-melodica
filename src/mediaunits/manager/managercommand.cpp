///
/// Command handlers for media manager and source-related actions.
/// @file       managercommand.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-01-29
/// @copyright  Copyright (c) 2015-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cstdio>
#include <cassert>

#include "fundamentals.h"
#include "utility.h"
#include "interpreter.h"
#include "sources.h"
#include "connection.h"
#include "response.h"
#include "mediaunit.h"
#include "mediamanager.h"
#include "users.h"

#include <parsnip/parsnip_schema.h>

typedef enum manager_commands_t {
    SOURCESELECT = CMD_RANGE_MEDIA_MANAGER,
    SOURCEEXEC,
    SOURCETYPELIST,
    SOURCELIST,
    SOURCEAVAILABLE,
    SOURCEMINE,
    SOURCEREMOVE,
    SOURCEFORGET,
    SOURCERENAME,
    WAITFORONEREADY,
    WAITFORMANYREADY,
    SOURCESTATISTICS,
    SOURCESTATISTICSALL
} COMMAND;

const Parsnip::OptionParser::Definitions &Media::Manager::source_id_parser_definitions() {
    // clang-format off
    static const Parsnip::OptionParser::Definitions option_list {
        "id {#id:1-999999999}",
        "type {type} name {name}",
        "name {name} type {type}"
    };
    // clang-format on
    return option_list;
}

const Parsnip::Parser::Definitions &Media::Manager::parser_definitions() {
    static const Parsnip::Parser::Definitions statement_list{
        { SOURCETYPELIST, "source list type" },
        { SOURCELIST, "source list [enabled]" },
        { SOURCEAVAILABLE, "source list available" },
        { SOURCEMINE, "source list mine" },
        { SOURCEEXEC, "with source" SOURCEIDENTITY " [{command...}]" },
        { SOURCESELECT, "source select" SOURCEIDENTITY },
        { SOURCEREMOVE, "source disconnect" SOURCEIDENTITY },
        { SOURCEFORGET, "source forget" SOURCEIDENTITY },
        { SOURCERENAME, "source rename" SOURCEIDENTITY " to {newname}" },
        { WAITFORMANYREADY,
          "wait for source <which:any|all> [pending] ready [{" KEY_WAITOPTIONS ":" KEY_WAITOPTIONS "}] ..." },
        { WAITFORONEREADY, "wait for source" SOURCEIDENTITY " ready [{" KEY_WAITOPTIONS ":" KEY_WAITOPTIONS "}] ..." },
        { SOURCESTATISTICSALL, "source statistics all" },
        { SOURCESTATISTICS, "source statistics" },
        { SOURCESTATISTICS, "source statistics" SOURCEIDENTITY }
    };
    return statement_list;
};

const Parsnip::Parser::Definitions &Media::Manager::getParserDefinitions() {
    return parser_definitions();
}

/** Retrieve names for our JSON requests, and make schema revisions.
    @param schema The schemaset containing our requests.
    @return Request name to command ID mappings. */
const PianodSchema::CommandIds &Media::Manager::json_request_names (PianodSchema &schema) {
    static const PianodSchema::CommandIds mappings{
        { "getSourceTypes", SOURCETYPELIST },
        { "getSourcesEnabled", SOURCELIST },
        { "getSourcesAvailable", SOURCEAVAILABLE },
        { "getMySources", SOURCEMINE },
        { REQUEST_WITHSOURCE, SOURCEEXEC },
        { "selectSource", SOURCESELECT },
        { "disconnectSource", SOURCEREMOVE },
        { "forgetSource", SOURCEFORGET },
        { "renameSource", SOURCERENAME },
        { "waitForSourcesReady", WAITFORMANYREADY },
        { "waitForSpecifiedSourceReady", WAITFORONEREADY },
        { "getAllSourceStatistics", SOURCESTATISTICSALL },
        { "getSourceStatistics", SOURCESTATISTICS },
    };
    schema.removeMember (SOURCEEXEC, KEY_COMMAND);
    schema.addMember (SOURCEEXEC, KEY_REQUEST, Parsnip::UncheckedSchema{});
    return mappings;
}

/** Get a source commands predicate (either by ID or type and name).
    @param options The options specifying source to retrieve.
    @return The source specified.
    @throw CommandError if the specified source does not exist. */

Media::Source * const Media::Manager::getSource (const Parsnip::Data &options) {
    auto src
            = (options.contains ("id") ? media_manager->get (options ["id"].asInteger())
                                       : media_manager->get (options ["type"].asString(), options ["name"].asString()));
    if (!src) {
        throw CommandError (E_NOTFOUND);
    }
    return src;
};

/// Send statistics data for a source.
ResponseGroup Media::Manager::reportStatistics (Media::Source * const source) {
    auto stats = source->getStatistics();

    ResponseGroup response;
    response (I_ID, source->serialNumber());
    response (I_SOURCE, source->kind());
    response (I_NAME, source->name());
    response (I_STATISTICS_ATTEMPTS, stats.songs_attempted);
    response (I_STATISTICS_PLAYS, stats.tracks_played);
    response (I_STATISTICS_FAILURES, stats.playback_failures);
    response (I_STATISTICS_SEQUENTIAL_FAILS, stats.sequential_failures);
    response (I_STATISTICS_REPLACEMENTS, stats.songs_replaced);
    response (I_STATISTICS_DONATIONS, stats.songs_donated);
    return response;
}

bool Media::Manager::authorizedCommand (Parsnip::Parser::CommandId command, PianodConnection &conn) {
    if (conn.havePrivilege (Privilege::Service))
        return true;
    switch (command) {
        case SOURCESELECT:
        case SOURCEEXEC:
        case SOURCETYPELIST:
        case SOURCEFORGET:
        case SOURCERENAME:
        case SOURCELIST:
            return conn.haveRank (Rank::Listener);
        case SOURCEAVAILABLE:
            return conn.haveRank (Rank::Standard);
        case SOURCESTATISTICS:
            return conn.source()->isReadableBy (conn.user);
        default:
            return conn.havePrivilege (Privilege::Service);
    }
};

ResponseCollector Media::Manager::handleCommand (Parsnip::Parser::CommandId command,
                                                 const Parsnip::Data &options,
                                                 PianodConnection &conn) {
    switch (command) {
        case SOURCESELECT: {
            auto src = getSource (options [Key::Source]);
            if (!src->isReady()) {
                throw CommandError (E_WRONG_STATE, "Source not ready or shutting down");
            }
            CommandReply response {S_OK};
            response.information (conn.source (src));
            return std::move (response);
        }
        case SOURCEEXEC: {
            Source * const temporary_source = getSource (options [Key::Source]);
            Media::Source::SerialNumber original_source = conn.source()->serialNumber();
            (void) conn.source (temporary_source, false);
            ResponseCollector response;
            try {
                response = conn.service().dispatch.redispatch (options, conn);
            } catch (...) {
                response = CommandReply (std::current_exception());
            }

            bool source_changed = (conn.source() != temporary_source);
            Source * const recovery_source = media_manager->get (original_source);
            // Return to original source, or use current if it doesn't exist.
            // Announce source change upon hinkiness.
            response.information (conn.source (recovery_source ? recovery_source : conn.source(), source_changed || !recovery_source));
            return response;
        }
        case SOURCETYPELIST: {
            DataResponse response;
            response.data (Response (I_SOURCE, SourceName::Manager));
            for (const auto &name : Sources::sourceKindNames()) {
                response.data (Response (I_SOURCE, name));
            }
            return std::move (response);
        }
        case SOURCELIST: {
            DataResponse response;
            for (auto const &src : *media_manager) {
                ResponseGroup record;
                record (Response (I_ID, src.second->serialNumber()));
                record (Response (I_SOURCE, src.second->kind()));
                record (Response (I_NAME, src.second->name()));
                if (src.second->isOwned()) {
                    record (Response (I_OWNER, src.second->ownerName()));
                }
                response.data (std::move (record));
            }
            return std::move (response);
        }
        case SOURCEAVAILABLE:
        case SOURCEMINE: {
            if (!conn.user) {
                throw CommandError (E_LOGINREQUIRED);
            }
            auto available = user_manager->getStoredSources (
                    command == SOURCEAVAILABLE ? UserManager::WhichSources::Listed : UserManager::WhichSources::User,
                    conn.user);
            DataResponse response;
            for (const auto &source : available) {
                ResponseGroup record;
                record (Response (I_SOURCE, source.second->origin()));
                record (Response (I_NAME, source.second->identity()));
                response.data (std::move (record));
            }
            return std::move (response);
        }
        case SOURCEFORGET:
        case SOURCERENAME: {
            if (!conn.user) {
                throw CommandError (E_LOGINREQUIRED);
            }
            if (options [Key::Source].contains ("id")) {
                throw CommandError (E_WRONGTYPE, "Use source type and name");
            }
            const std::string &type = options [Key::Source][Key::SourceKind].asString();
            const std::string &name = options [Key::Source][Key::SourceName].asString();
            UserData::DataStore *data;
            if ((data = conn.user->getData (type, name))) {
                if (command == SOURCERENAME) {
                    const std::string &new_name = options ["newname"].asString();
                    if (conn.user->getData (type, new_name)) {
                        throw CommandError (E_DUPLICATE);
                    }
                    UserData::JSONData *new_data = new UserData::JSONData (*static_cast<UserData::JSONData *> (data));
                    new_data->rename (new_name);
                    (*new_data) ["name"] = new_name;
                    if (!conn.user->attachData (new_data)) {
                        delete new_data;
                        throw CommandError (E_NAK);
                    }
                }
                conn.user->removeData (type, name);
                return S_OK;
            } else {
                throw CommandError (E_NOTFOUND);
            }
        }
        case SOURCEREMOVE: {
            auto src = getSource (options [Key::Source]);
            if (src->serialNumber() == 1) {
                throw CommandError (E_UNAUTHORIZED, "Media Manager cannot be removed");
            }
            CommandReply response;
            response.succeed (media_manager->erase (src) ? S_OK : S_PENDING);
            response.broadcast_events (A_SOURCE_REMOVE);
            return std::move (response);
        }
        case WAITFORMANYREADY:
            if (options.contains ("pending") && !media_manager->areSourcesPending()) {
                throw CommandError (E_WRONG_STATE);
            }
            conn.waitForEventWithOptions (WaitEvent::Type::SourceReady,
                                          options.getOr (KEY_WAITOPTIONS, EmptyDictionary),
                                          optionIs (options, "which", "all") ? nullptr : media_manager);
            return NO_REPLY;
        case WAITFORONEREADY: {
            auto src = getSource (options [Key::Source]);
            if (src->isReady())
                throw CommandError (E_WRONG_STATE, "Already ready");
            conn.waitForEventWithOptions (WaitEvent::Type::SourceReady,
                                          options.getOr (KEY_WAITOPTIONS, EmptyDictionary),
                                          src);
            return NO_REPLY;
        }
        case SOURCESTATISTICS:
            return reportStatistics (options.contains (Key::Source) ? getSource (options [Key::Source]) : conn.source());
        case SOURCESTATISTICSALL: {
            DataResponse response;
            for (auto &src : *media_manager) {
                response.data (reportStatistics (src.second));
            }
            return std::move (response);
        }
        default:
            flog (LOG_WHERE (Log::WARNING), "Unimplemented command ", command);
            throw CommandError (E_NOT_IMPLEMENTED);
    }
}
