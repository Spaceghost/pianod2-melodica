///
/// Command handlers for Pandora-specific stuff.
/// @file       mediaunits/pandora/pandoracommand.h - pianod
/// @author     Perette Barella
/// @date       2020-12-02
/// @copyright  Copyright 2014-2021 Devious Fish.  All rights reserved.
///

#pragma once

#include <parsnip/parsnip.h>
#include <parsnip/parsnip_command.h>

#include "interpreter.h"

#define KEY_PANDORA_OPTIONS "pandoraOptions"

/// Interpreter for tonegenerator commands.
class PandoraCommands : public PianodInterpreter {
    virtual bool authorizedCommand (Parsnip::Parser::CommandId, PianodConnection &conn) override;
    virtual ResponseCollector handleCommand (Parsnip::Parser::CommandId, const Parsnip::Data &options, PianodConnection &conn) override;
public:    
    virtual const Parsnip::Parser::Definitions &getParserDefinitions () override;
    static const Parsnip::Parser::Definitions &parser_definitions();
    static const PianodSchema::CommandIds &json_request_names ();
};

extern Parsnip::OptionParser::Definitions retrieve_pandora_option_definitions ();
extern void restore_pandora_source (UserData::JSONData *params, User *user);
