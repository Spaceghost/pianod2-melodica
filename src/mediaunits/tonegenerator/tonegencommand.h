///
/// Command handlers for tone generator source.
/// @file       tonegencommand.h - pianod
/// @author     Perette Barella
/// @date       2014-12-04
/// @copyright  Copyright 2014-2020 Devious Fish.  All rights reserved.
///

#pragma once

#include <parsnip/parsnip.h>
#include <parsnip/parsnip_command.h>

#include "interpreter.h"

#define KEY_TONEGENERATOR_OPTIONS "tonegenOptions"

/// Interpreter for tonegenerator commands.
class ToneGeneratorCommands : public PianodInterpreter {
    virtual bool authorizedCommand (Parsnip::Parser::CommandId, PianodConnection &conn) override;
    virtual ResponseCollector handleCommand (Parsnip::Parser::CommandId, const Parsnip::Data &options, PianodConnection &conn) override;
public:    
    virtual const Parsnip::Parser::Definitions &getParserDefinitions () override;
    static const Parsnip::Parser::Definitions &parser_definitions();
    static const PianodSchema::CommandIds &json_request_names ();
};

extern Parsnip::OptionParser::Definitions retrieve_tonegenerator_option_definitions ();
extern void restore_tonegenerator_source (UserData::JSONData *params, User *user);
