///
/// Command handlers for filesystem source.
/// @file       filesystemcommand.cpp - pianod
/// @author     Perette Barella
/// @date       2015-02-25
/// @copyright  Copyright 2015-2023 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <cstdio>
#include <cassert>

#include <interpreter.h>

#include "fundamentals.h"
#include "sources.h"
#include "connection.h"
#include "user.h"
#include "users.h"
#include "response.h"
#include "mediamanager.h"
#include "mediaparameters.h"
#include "musiclibraryparameters.h"

#include "filesystem.h"
#include "filesystemcommand.h"


typedef enum filesystem_command_t { FSADDSOURCE = CMD_RANGE_FILESYSTEM, FSEXISTING, FSRESCAN } FSCOMMAND;

const Parsnip::Parser::Definitions &FilesystemCommands::parser_definitions() {
    static const Parsnip::Parser::Definitions statement_list{
        { FSADDSOURCE, "filesystem add {path} [{" KEY_OPTIONS ":" KEY_LIBRARYOPTIONS "}] ..." },
        { FSEXISTING, "filesystem use {name} [{" KEY_OPTIONS ":" KEY_LIBRARYOPTIONS "}] ..." },
        { FSRESCAN, "filesystem rescan [fresh:fresh|update]" }
    };
    return statement_list;
}

/** Retrieve names for our JSON requests.
    @return Request name to command ID mappings. */
const PianodSchema::CommandIds &FilesystemCommands::json_request_names () {
    static const PianodSchema::CommandIds mappings{ { "createFilesystemSource", FSADDSOURCE },
                                                    { "useFilesystemSource", FSEXISTING },
                                                    { "rescanFilesystem", FSRESCAN } };
    return mappings;
}


const Parsnip::Parser::Definitions &FilesystemCommands::getParserDefinitions() {
    return parser_definitions();
}

bool FilesystemCommands::authorizedCommand (Parsnip::Parser::CommandId, PianodConnection &conn) {
    return conn.havePrivilege (Privilege::Service);
};

ResponseCollector FilesystemCommands::handleCommand (Parsnip::Parser::CommandId command,
                                                     const Parsnip::Data &options,
                                                     PianodConnection &conn) {
    switch (command) {
        case FSADDSOURCE: {
            if (!conn.user) {
                throw CommandError (E_LOGINREQUIRED);
            }
            Filesystem::Parameters p (Ownership::Type::PUBLISHED, conn.user);
            p.path = options ["path"].asString();
            if (options.contains (KEY_OPTIONS)) {
                p.extractOptions (options [KEY_OPTIONS]);
            }
            p.sanitize();
            return media_manager->add (Media::SourcePtr{ new Filesystem::Source (p) }, conn);
        }
        case FSEXISTING: {
            UserData::JSONData *persisted;
            User *owner;
            RESPONSE_CODE status = user_manager->findStoredSource (SourceName::FileSystem,
                                                                   options ["name"].asString(),
                                                                   conn.user,
                                                                   &persisted,
                                                                   &owner);
            if (status != S_OK) {
                throw CommandError (status);
            }
            Filesystem::Parameters params (*persisted);
            params.persistence = Media::PersistenceMode::Loaded;
            params.owner = owner;
            if (options.contains (KEY_OPTIONS)) {
                params.extractOptions (options [KEY_OPTIONS]);
            }
            params.sanitize();
            return media_manager->add (Media::SourcePtr{ new Filesystem::Source (params) }, conn);
        }
        case FSRESCAN: {
            auto fs = dynamic_cast<Filesystem::Source * const > (conn.source());
            if (!fs) {
                throw CommandError (E_WRONGTYPE);
            } else if (!fs->isEditableBy (conn.user)) {
                throw CommandError (E_UNAUTHORIZED);
            }
            fs->rescan (optionIs (options, "fresh", "fresh"));
            return S_OK;
        }
        default:
            flog (LOG_WHERE (Log::FILESYSTEM|Log::WARNING), "Unimplemented command ", command);
            throw CommandError (E_NOT_IMPLEMENTED);
    }
}

void restore_filesystem_source (UserData::JSONData *persisted, User *user) {
    Filesystem::Parameters params (*persisted);
    params.persistence = Media::PersistenceMode::Loaded;
    params.owner = user;
    media_manager->add (Media::SourcePtr{ new Filesystem::Source (params) });
}
