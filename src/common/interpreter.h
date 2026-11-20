///
/// pianod command processing.
/// @file       interpreter.h - pianod command interpretation and execution.
/// @author     Perette Barella
/// @date       2020-11-23
/// @copyright  Copyright 2021 Devious Fish. All rights reserved.
///

#pragma once

#include <vector>
#include <set>
#include <string>
#include <unordered_map>
#include <map>
#include <memory>

#include <parsnip/parsnip_command.h>

#include "fundamentals.h"
#include "response.h"

#define KEY_AUDIOOPTIONS "audioOptions"
#define KEY_WAITOPTIONS "waitOptions"
#define KEY_TUNEROPTIONS "tunerOptions"
#define KEY_LIBRARYOPTIONS "libraryOptions"
#define KEY_PANDORAOPTIONS "pandoraOptions"
#define KEY_PLAYBACKSELECTION "playbackOptions"

#define KEY_OPTIONS "options"
#define KEY_COMMAND "command"
#define KEY_REQUEST "request"

#define KEY_ASUSER "asUser"
#define KEY_WITHSOURCE "withSource"
#define KEY_INROOM "inRoom"
#define REQUEST_ASUSER "asUserExecute"
#define REQUEST_WITHSOURCE "withSourceExecute"
#define REQUEST_INROOM "inRoomExecute"

#define PARSER_SOURCEIDENTITY "sourceId"
#define KEY_SOURCE "source"
#define SOURCEIDENTITY " {" KEY_SOURCE ":" PARSER_SOURCEIDENTITY "}"

class PianodConnection;

/// Command line parser based on Parsnip, but with some help support added for pianod.
class PianodParser : public Parsnip::Parser {
    friend class PianodDispatcher;
    std::unordered_map <std::string, const Parsnip::OptionParser::Definitions> option_definitions;
public:
    void addOptionParser (const std::string &name, Parsnip::OptionParser::Definitions defs);
};



class PianodSchema : public Parsnip::SchemaSet {
public:
    using CommandIds = std::map <std::string, CommandId>;
    using SchemaList = std::vector<std::string>;
private:
    CommandIds command_ids;
public:
    using RequestPair = std::pair <CommandId, const Parsnip::Data *>;
    using SchemaSet::SchemaSet;
    using SchemaSet::validate;

    CommandId getCommandId (const std::string &name) const;
    void addRequestNames (const CommandIds &mappings);
    void validate (const std::string &command, const Parsnip::Data &request);
    std::ostream &dump (const std::string &command, std::ostream &target = std::clog) const;
    std::ostream &dumpAll (std::ostream &target = std::clog) const;
    SchemaList allRequestNames() const;
};
using PianodSchemaRef = std::shared_ptr <PianodSchema>;



/// Dispatcher based on Parsnip template, customized for pianod by adding help support.
class PianodDispatcher : public Parsnip::Dispatcher <ResponseCollector, PianodConnection &> {
    using base_type = Parsnip::Dispatcher <ResponseCollector, PianodConnection &>;
    PianodSchemaRef schema;
    std::vector <class PianodInterpreter *> interpreters; ///< List of all associated interpreters.
public:
    using HelpList = std::vector<std::string>;
    using base_type::operator();
    
    PianodDispatcher (const Parsnip::ParserRef &parser, const PianodSchemaRef &schema);
    void addHandler (const Parsnip::Parser::Definitions &defs, class PianodInterpreter *interpreter);
    void rewriteJsonRequest (Parsnip::Data &request) const;
    ResponseCollector jsonRequest (const Parsnip::Data &request, PianodConnection &context) const;
    ResponseCollector redispatch (const Parsnip::Data &options, PianodConnection &context) const;

    const HelpList getHelp (std::vector <std::string> search);
};



/// Interpreter class based on Parsnip template, customized base for pianod interpreters
/// with the addition of help support.
class PianodInterpreter : public Parsnip::Interpreter <ResponseCollector , PianodConnection &> {
    friend PianodDispatcher;
    virtual ResponseCollector interpret (Parsnip::Parser::CommandId command_id, const Parsnip::Data &parameters, PianodConnection & context) override final;
protected:
    using StringVector = std::vector <std::string>;
    static const Parsnip::Data EmptyDictionary;
    static const StringVector EmptyStringVector;

    /** Check whether a command is authorized.
        @param command_id The command requesting execution.
        @param context Connection details, including authentication state and privileges.
        @return True if allowed, false otherwise. */
    virtual bool authorizedCommand (Parsnip::Parser::CommandId command_id, PianodConnection &context) = 0;

    /** Command handler.
        @param command_id Command to execute.
        @param parameters Command parameters.
        @param context Connection details, including authentication state and privileges.
        @return Data or success/failure indications.
        @throw CommandError or other exception if an error occurs. */
    virtual ResponseCollector handleCommand (Parsnip::Parser::CommandId command_id, const Parsnip::Data &parameters, PianodConnection &context) = 0;

    /** Retrieve the parser definitions handled by the interpreter.
        @return Parser definitions. */
    virtual const Parsnip::Parser::Definitions &getParserDefinitions () = 0;

public:
    static bool optionIs (const Parsnip::Data &options, const char *name, const char *expected);
    /** Retrieve the ID of a string value.
        @param options Where to look for the value.
        @param name The name of the value.
        @param lookup The lookup table for the value.
        @param default_value The default value, if not specified.
        @return The ID value from the lookup table, or default.
        @throws If the value is not in the table. */
    template <typename ValueType, typename LookupType>
    static inline ValueType optionalValue (const Parsnip::Data &options, const char *name, const LookupType &lookup, const ValueType default_value) {
        return (options.contains (name) ? lookup [options [name].asString()] : default_value);
    }
    using HelpList = PianodDispatcher::HelpList;
    virtual void registerInterpreter (PianodDispatcher &dispatcher);
    const HelpList getHelp (const std::vector <std::string> &search, std::set <std::string> *option_parsers);
};



