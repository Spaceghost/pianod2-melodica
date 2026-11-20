///
/// Command handlers for Pandora-specific stuff.
/// @file       mediaunits/pandora/pandoracommand.cpp - pianod
/// @author     Perette Barella
/// @date       2014-11-30
/// @copyright  Copyright 2014-2023 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <cassert>

#include "interpreter.h"
#include "fundamentals.h"
#include "sources.h"
#include "connection.h"
#include "response.h"
#include "user.h"
#include "users.h"
#include "mediamanager.h"

#include "pandora.h"
#include "pandoraparameters.h"
#include "pandoracommand.h"


#define KEY_USER "user"
#define KEY_PASSWORD "password"

typedef enum pandora_commands_t {
    PANDORAUSER = CMD_RANGE_PANDORA,
    PANDORAEXISTING,
    PANDORASETTINGS,
} COMMAND;

const Parsnip::Parser::Definitions &PandoraCommands::parser_definitions() {
    static const Parsnip::Parser::Definitions statement_list = {
        { PANDORAUSER,
          "pandora user {" KEY_USER "} {" KEY_PASSWORD "} [{" KEY_OPTIONS ":" KEY_PANDORAOPTIONS "}] ..." },
        // Set Pandora password username/password
        { PANDORAEXISTING, "pandora use {name} [{" KEY_OPTIONS ":" KEY_PANDORAOPTIONS "}] ..." },
        // Use existing/revised user's Pandora credentials
        { PANDORASETTINGS, "pandora settings" }  // Retrieve connection parameters
    };
    return statement_list;
}

const Parsnip::Parser::Definitions &PandoraCommands::getParserDefinitions() {
    return parser_definitions();
}

/** Retrieve names for our JSON requests.
    @return Request name to command ID mappings. */
const PianodSchema::CommandIds &PandoraCommands::json_request_names () {
    static const PianodSchema::CommandIds mappings{ { "createPandoraSource", PANDORAUSER },
                                                    { "usePandoraSource", PANDORAEXISTING },
                                                    { "getPandoraSettings", PANDORASETTINGS } };
    return mappings;
}



bool PandoraCommands::authorizedCommand (Parsnip::Parser::CommandId command, PianodConnection &conn) {
    return conn.havePrivilege (Privilege::Service);
}

ResponseCollector PandoraCommands::handleCommand (Parsnip::Parser::CommandId command,
                                                  const Parsnip::Data &options,
                                                  PianodConnection &conn) {
    switch (command) {
        case PANDORAUSER: {
            if (!conn.authenticated()) {
                throw CommandError (E_LOGINREQUIRED);
            }
            Pandora::ConnectionParameters params;
            params.username = options [KEY_USER].asString();
            params.password = options [KEY_PASSWORD].asString();
            params.owner = conn.user;
            params.name = conn.username();
            params.extractOptions (options.getOr (KEY_OPTIONS, EmptyDictionary));
            return media_manager->add (Media::SourcePtr{ new Pandora::Source (params) }, conn);
        }
        case PANDORAEXISTING: {
            UserData::JSONData *persisted;
            User *owner;
            RESPONSE_CODE status = user_manager->findStoredSource (SourceName::Pandora,
                                                                   options ["name"].asString(),
                                                                   conn.user,
                                                                   &persisted,
                                                                   &owner);
            if (status != S_OK) {
                throw CommandError (status);
            }
            Pandora::ConnectionParameters params (*persisted);
            params.persistence = Media::PersistenceMode::Loaded;
            params.owner = owner;
            params.extractOptions (options.getOr (KEY_OPTIONS, EmptyDictionary));
            return media_manager->add (Media::SourcePtr{ new Pandora::Source (params) }, conn);
        }
        case PANDORASETTINGS: {
            if (!conn.source()->isDecendableBy (conn.user)) {
                throw CommandError (E_UNAUTHORIZED);
            }
            Pandora::Source * const source = dynamic_cast<Pandora::Source * const > (conn.source());
            if (!source) {
                throw CommandError (E_WRONG_STATE, "Pandora source required");
            }
            DataResponse response;
            response.data (Response (I_PROXY, source->connectionParams()->proxy));
            response.data (Response (I_CONTROLPROXY, source->connectionParams()->control_proxy));
            response.data (Response (I_PAUSE_TIMEOUT, source->connectionParams()->pause_timeout));
            response.data (Response (I_PLAYLIST_TIMEOUT, source->connectionParams()->playlist_expiration));
            response.data (Response (I_CACHE_MINIMUM, source->connectionParams()->cache_minimum));
            response.data (Response (I_CACHE_MAXIMUM, source->connectionParams()->cache_maximum));
            return std::move (response);
        }
        default:
            flog (LOG_WHERE (Log::WARNING), "Unimplemented command ", command);
            throw CommandError (E_NOT_IMPLEMENTED);
    }
}

Parsnip::OptionParser::Definitions retrieve_pandora_option_definitions() {
    return Pandora::ConnectionParameters::parser_definitions();
}

void restore_pandora_source (UserData::JSONData *persisted, User *user) {
    Pandora::ConnectionParameters params (*persisted);
    params.persistence = Media::PersistenceMode::Loaded;
    params.owner = user;
    media_manager->add (Media::SourcePtr{ new Pandora::Source (params) });
}
