///
/// Command handlers for tone generator source.
/// @file       tonegencommand.cpp - pianod
/// @author     Perette Barella
/// @date       2014-12-04
/// @copyright  Copyright 2014-2023 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <cstdio>
#include <cassert>

#include <algorithm>

#include <parsnip/parsnip.h>
#include <parsnip/parsnip_command.h>

#include "fundamentals.h"
#include "sources.h"
#include "connection.h"
#include "user.h"
#include "users.h"
#include "response.h"
#include "mediamanager.h"
#include "mediaparameters.h"
#include "musiclibraryparameters.h"

#include "tonegen.h"
#include "tonegencommand.h"


typedef enum test_tone_command_t { TTADDSOURCE = CMD_RANGE_TONEGENERATOR, TTEXISTING } TTCOMMAND;

class ToneGeneratorOptions : MusicLibrary::LibraryParameters {
public:
    bool discrete_channels{ false };

    static Parsnip::OptionParser::Definitions parser_definitions() {
        Parsnip::OptionParser::Definitions parser{ MusicLibrary::LibraryParameters::parser_definitions() };
        static const Parsnip::OptionParser::Definitions tone_generator_options = {
            "<channels:discrete> channels ...",  // Ask for discrete channels
        };
        std::copy (tone_generator_options.begin(), tone_generator_options.end(), std::back_inserter (parser));
        return parser;
    }

    void extractOptions (const Parsnip::Data &options) {
        if (options.contains (KEY_RESCAN_BEHAVIOR)) {
            throw CommandError (E_WRONGTYPE, "Rescan not applicable to tone generator.");
        }
        discrete_channels = options.contains ("channels");
        MusicLibrary::LibraryParameters::extractOptions (options);
    }
};

extern Parsnip::OptionParser::Definitions retrieve_tonegenerator_option_definitions() {
    return ToneGeneratorOptions::parser_definitions();
}

const Parsnip::Parser::Definitions &ToneGeneratorCommands::parser_definitions() {
    static const Parsnip::Parser::Definitions statement_list = {
        { TTADDSOURCE, "tonegenerator activate [{" KEY_OPTIONS ":" KEY_TONEGENERATOR_OPTIONS "}] ..." },
        { TTEXISTING, "tonegenerator use {name} [{" KEY_OPTIONS ":" KEY_TONEGENERATOR_OPTIONS "}] ..." },
    };
    return statement_list;
}

/** Retrieve names for our JSON requests.
    @return Request name to command ID mappings. */
const PianodSchema::CommandIds &ToneGeneratorCommands::json_request_names () {
    static const PianodSchema::CommandIds mappings{ { "createTonegeneratorSource", TTADDSOURCE },
                                                    { "useTonegeneratorSource", TTEXISTING } };
    return mappings;
}

bool ToneGeneratorCommands::authorizedCommand (Parsnip::Parser::CommandId, PianodConnection &conn) {
    return conn.havePrivilege (Privilege::Service);
};

ResponseCollector ToneGeneratorCommands::handleCommand (Parsnip::Parser::CommandId command,
                                                        const Parsnip::Data &options,
                                                        PianodConnection &conn) {
    switch (command) {
        case TTADDSOURCE: {
            if (!conn.user) {
                throw CommandError (E_LOGINREQUIRED);
            }
            ToneGenerator::Parameters p (Ownership::Type::PUBLISHED, conn.user);
            if (options.contains (KEY_OPTIONS)) {
                p.extractOptions (options [KEY_OPTIONS]);
            }
            return media_manager->add (Media::SourcePtr{ new ToneGenerator::Source (p) }, conn);
        }
        case TTEXISTING: {
            UserData::JSONData *persisted;
            User *owner;
            RESPONSE_CODE status = user_manager->findStoredSource (SourceName::ToneGenerator,
                                                                   options ["name"].asString(),
                                                                   conn.user,
                                                                   &persisted,
                                                                   &owner);
            if (status != S_OK) {
                throw CommandError (status);
            }
            ToneGenerator::Parameters params (*persisted);
            params.persistence = Media::PersistenceMode::Loaded;
            params.owner = owner;
            params.extractOptions (options.getOr (KEY_OPTIONS, EmptyDictionary));
            return media_manager->add (Media::SourcePtr{ new ToneGenerator::Source (params) }, conn);
        }
        default:
            flog (LOG_WHERE (Log::WARNING), "Unimplemented command ", command);
            throw CommandError (E_NOT_IMPLEMENTED);
    }
}

const Parsnip::Parser::Definitions &ToneGeneratorCommands::getParserDefinitions() {
    return parser_definitions();
}

void restore_tonegenerator_source (UserData::JSONData *persisted, User *user) {
    ToneGenerator::Parameters params (*persisted);
    params.persistence = Media::PersistenceMode::Loaded;
    params.owner = user;
    media_manager->add (Media::SourcePtr{ new ToneGenerator::Source (params) });
    return;
}
