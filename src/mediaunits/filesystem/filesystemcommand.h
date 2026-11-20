///
/// Command handlers for filesystem source.
/// @file       filesystemcommand.h - pianod
/// @author     Perette Barella
/// @date       2020-12-02
/// @copyright  Copyright 2015-2021 Devious Fish.  All rights reserved.
///

#pragma once

#include <parsnip/parsnip.h>
#include <parsnip/parsnip_command.h>

#include "interpreter.h"

/** Parser for filesystem media source commands. */
class FilesystemCommands : public PianodInterpreter {
private:
    virtual bool authorizedCommand (Parsnip::Parser::CommandId, PianodConnection &conn) override;
    virtual ResponseCollector handleCommand (Parsnip::Parser::CommandId, const Parsnip::Data &options, PianodConnection &conn) override;
public:
    virtual const Parsnip::Parser::Definitions &getParserDefinitions () override;
    static const Parsnip::Parser::Definitions &parser_definitions();
    static const PianodSchema::CommandIds &json_request_names ();
};

extern Parsnip::OptionParserRef retrieve_filesystem_option_parser ();
extern void restore_filesystem_source (UserData::JSONData *params, User *user);
