///
/// pianod command processing.
/// @file       interpreter.cpp - pianod command interpretation and execution.
/// @author     Perette Barella
/// @date       2020-11-23
/// @copyright  Copyright 2021 Devious Fish. All rights reserved.
///

#include <cctype>

#include <cstring>
#include <set>

#include <parsnip/parsnip_command.h>
#include <parsnip/parsnip_argv.h>

#include "fundamentals.h"

#include "interpreter.h"

/** Determine if search criteria match a help string.
    @param help The help string to check.
    @param search A list of search criteria (words).
    @return True if the search matches, false otherwise. */
static bool helpMatches (std::string help, const std::vector<std::string> search) {
    for (auto &ch : help) {
        ch = tolower (ch);
    }
    for (const std::string &word : search) {
        if (help.find (word) == std::string::npos) {
            return false;
        }
    }
    return true;
}

/** Remove cruft from a statement definition to clean up its presentation as help.
    @param statement The statement to clean up.
    @param option_parsers If the statement references option parsers, they are added to the set.
    @return The statement, ready for presentation as help. */
static std::string simplify_statement (const std::string &statement, std::set<std::string> *option_parsers) {
    Parsnip::ArgumentVector tokenized (statement);
    std::string command;
    for (Parsnip::ArgvCursor cursor (&tokenized); !cursor.isEnd(); cursor++) {
        const std::string &token = cursor.value();
        if (!command.empty()) {
            command += ' ';
        }
        if (token.size() >= 3 && (token[0] == '{' || token[0] == '<' || token[0] == '[')) {
            std::string::size_type colon = token.find (':');
            bool fill_in = (token[0] == '{' || (token [0] == '[' && token [1] == '{'));
            if (colon == std::string::npos) {
                command += token;
            } else if (fill_in) {
                bool optional = (token [0] == '[');
                if (token[optional ? 2 : 1] == '#') {
                    command += token.substr (0, colon);
                    command += '}';
                    if (optional) {
                        command += ']';
                    }
                } else {
                    command += token;
                    if (option_parsers) {
                        option_parsers->insert (token.substr (colon + 1, token.size() - colon - (optional ? 3 : 2)));
                    }
                }
            } else {
                command += token[0];
                command += token.substr (colon + 1);
            }
        } else {
            command += token;
        }
    }
    return command;
}

/*
 *                  PianodParser
 */

/** Construct and register an option parser.
    Parser definition is saved for later use in providing help.
    @param name The parser name, as will be used in statement definitions.
    @param defs The parser's option format definitions. */
void PianodParser::addOptionParser (const std::string &name, Parsnip::OptionParser::Definitions defs) {
    Parsnip::Parser::addOptionParser (name, Parsnip::OptionParserRef{new Parsnip::OptionParser (defs, this)});
    option_definitions.insert (std::make_pair (name, std::move (defs)));
}

/*
 *                  PianodSchema
 */

/** Retrieve a request's ID by name.
    @param request_name The JSON request's name.
    @return The command ID for the request.
    @throw NoSchemaDefine if `request_name` wasn't found. */
PianodSchema::CommandId PianodSchema::getCommandId (const std::string &request_name) const {
    auto it = command_ids.find (request_name);
    if (it == command_ids.end()) {
        throw Parsnip::NoSchemaDefined (request_name);
    }
    return it->second;
}

/** Register additional name/ID mappings for requests.
    @param mappings A map from name to ID. */
void PianodSchema::addRequestNames (const CommandIds &mappings) {
    for (const auto &mapping : mappings) {
        command_ids[mapping.first] = mapping.second;
    }
}

/** Verify a request complies with its schema.
    @param request_name The request name.
    @param parameters The request parameters to be validated. */
void PianodSchema::validate (const std::string &request_name, const Parsnip::Data &parameters) {
    validate (getCommandId (request_name), parameters);
}


std::ostream &PianodSchema::dump (const std::string &command, std::ostream &target) const {
    SchemaSet::dump (command, getCommandId (command), target);
    return target;
}

std::ostream &PianodSchema::dumpAll (std::ostream &target) const {
    for (const auto &command : command_ids) {
        SchemaSet::dump (command.first, command.second, target);
    }
    return target;
}

PianodSchema::SchemaList PianodSchema::allRequestNames() const {
    SchemaList list;
    for (const auto &command : command_ids) {
        list.push_back (command.first);
    }
    return list;
}

/*
 *                  PianodDispatcher
 */

/// Empty set
const static std::set<std::string> IgnoreNothing;

/** Retrieve the request name by searching keys in a dictionary.
    @param request The request.
    @param ignore A set of keys to ignore.
    @return The request name.
    @throw Command error if there are no keys or multiple non-ignored
    keys in the dictionary. */
static std::string getRequestName (const Parsnip::Data &request, const std::set<std::string> &ignore = IgnoreNothing) {
    std::string request_name;
    if (request.empty()) {
        throw CommandError (E_BAD_SCHEMA, "Request name missing");
    }
    std::function<void (const std::string &, const Parsnip::Data &)> get_request_name{
        [&request_name, &ignore] (const std::string &key, const Parsnip::Data &) {
            if (ignore.find (key) != ignore.end()) {
                // Do nothing
            } else if (!request_name.empty()) {
                throw CommandError (E_BAD_SCHEMA, "Ambiguous command name");
            } else {
                request_name = key;
            }
        }};
    request.foreach (get_request_name);
    return request_name;
}

/** Construct a dispatcher for Pianod.
    @param parser A parser for the pianod command set.
    @param schema_in A schema fr the pianod request set. */
PianodDispatcher::PianodDispatcher (const Parsnip::ParserRef &parser, const PianodSchemaRef &schema_in)
    : base_type::Dispatcher (parser), schema (schema_in) {
}

/** Add a handler to/for which commands will be dispatched.
    @param defs The list of commands the interpreter will handle.
    @param interpreter The interpreter instance. */
void PianodDispatcher::addHandler (const Parsnip::Parser::Definitions &defs, PianodInterpreter *interpreter) {
    interpreters.push_back (interpreter);
    base_type::addHandler (defs, interpreter);
}

/** Move pieces of requests into a tree structure for execution.
    @param name The new name of the piece being moved.
    @param piece The piece being moved.  If nullptr, then nothing is performed.
    @param request The existing request.  On return, the moved piece is moved to wrap this. */
static void rewrite_push (const char *name, Parsnip::Data *piece, Parsnip::Data *request) {
    if (piece) {
        if (piece->contains (KEY_REQUEST)) {
            throw CommandError (E_BAD_SCHEMA, std::string (name) + " contains a request field.");
        }
        Parsnip::Data new_request { Parsnip::Data::make_dictionary ({{name, std::move (*piece)}}) };
        new_request[name][KEY_REQUEST] = std::move (*request);
        *request = std::move (new_request);
    }
}

/** Rewrite the JSON request, wrapping the actual command in any as-user, in-room,
    and with-source specifications.
    @param request The request, rewritten on return if required. */
void PianodDispatcher::rewriteJsonRequest (Parsnip::Data &request) const {
    const static std::set<std::string> IgnoreClauses {KEY_ASUSER, KEY_INROOM, KEY_WITHSOURCE};
    Parsnip::Data *as_user = (request.contains (KEY_ASUSER) ? &request[KEY_ASUSER] : nullptr);
    Parsnip::Data *in_room = (request.contains (KEY_INROOM) ? &request[KEY_INROOM] : nullptr);
    Parsnip::Data *with_source = (request.contains (KEY_WITHSOURCE) ? &request[KEY_WITHSOURCE] : nullptr);
    if (as_user || in_room || with_source) {
        std::string command_name = getRequestName (request, IgnoreClauses);
        Parsnip::Data reassembled { Parsnip::Data::make_dictionary ({{ command_name.c_str(), std::move (request[command_name]) }}) };
        rewrite_push (REQUEST_WITHSOURCE, with_source, &reassembled);
        rewrite_push (REQUEST_INROOM, in_room, &reassembled);
        rewrite_push (REQUEST_ASUSER, as_user, &reassembled);
        request = std::move (reassembled);
    }
}

/** Dispatch a JSON request for execution.
    @param request The request to dispatch.
    @param context The request's context.
    @return Data or success/failure indications. */
ResponseCollector PianodDispatcher::jsonRequest (const Parsnip::Data &request, PianodConnection &context) const {
    std::string command_name = getRequestName (request);
    const Parsnip::Data &parameters = request [command_name];
    schema->validate (command_name, parameters);
    return (*this) (schema->getCommandId (command_name), parameters, context);
}

/** Dispatch a command or request embedded within another command.
    I.e., ADD USER, IN ROOM, or WITH SOURCE.
    @param options The options provided to one of the aforementioned commands.
    @param context The request's context.
    @return Data or success/failure indications. */
ResponseCollector PianodDispatcher::redispatch (const Parsnip::Data &options, PianodConnection &context) const {
    if (options.contains (KEY_COMMAND)) {
        return (*this) (options[KEY_COMMAND].asString(), context);
    } else {
        return jsonRequest (options[KEY_REQUEST], context);
    }
}

/** Request help for all interpreters which a dispatcher handles.
    @param search Search criteria.
    @return A list of related help, or an empty list on no matches. */
const std::vector<std::string> PianodDispatcher::getHelp (std::vector<std::string> search) {
    // Convert search terms to lowercase for easier comparison.
    for (auto &line : search) {
        for (auto &ch : line) {
            ch = tolower (ch);
        }
    }

    // Search through all parse definitions for all interpreters for matching statements.
    HelpList help;
    std::set<std::string> option_parsers;
    for (const auto interpreter : interpreters) {
        std::vector<std::string> partial_help = interpreter->getHelp (search, &option_parsers);
        for (auto &item : partial_help) {
            help.push_back (std::move (item));
        }
    }

    // Only if the results are few do we automatically add option parser patterns...
    if (help.size() > 5 && help.size() + option_parsers.size() * 7 > 16) {
        option_parsers.clear();
    }
    // ...but always include option parsers matching search patterns.
    PianodParser *pdparser = static_cast<PianodParser *> (parser.get());
    if (!search.empty()) {
        for (const auto &parser : pdparser->option_definitions) {
            if (helpMatches (parser.first, search)) {
                option_parsers.insert (parser.first);
            }
        }
    }

    // Insert the help for any option parsers
    for (const auto &option_parser : option_parsers) {
        help.push_back (option_parser + " is defined as:");
        for (const char *def : pdparser->option_definitions[option_parser]) {
            help.push_back ("- " + simplify_statement (def, nullptr));
        }
    }
    return help;
}

/*
 *                  PianodInterpreter
 */

/// An empty dictionary, often used as a default with Parsnip::Data::getOr.
const Parsnip::Data PianodInterpreter::EmptyDictionary{Parsnip::Data::Dictionary};

/// An empty string vector, often used as a default with Parsnip::Data::getOr.
const PianodInterpreter::StringVector PianodInterpreter::EmptyStringVector;

/** Check if a string value exists and is case-blind equal to a particular value.
    @param options The options dictionary to look in.
    @param name The name of the value to check
    @param expected The expected value.
    @return True if the options value exists and has the expected value.
    @throw If the value has the wrong type. */
bool PianodInterpreter::optionIs (const Parsnip::Data &options, const char *name, const char *expected) {
    return options.contains (name) && (strcasecmp (options[name].asString().c_str(), expected) == 0);
}

/** Authorize and execute a command.
    @param command_id The command to execute.
    @param parameters Options for the command.
    @param context The connection and context executing the command.
    @return Data or success/failure indications.
    @throw CommandError or any other exception. */
ResponseCollector PianodInterpreter::interpret (Parsnip::Parser::CommandId command_id,
        const Parsnip::Data &parameters,
        class PianodConnection &context) {
    if (!authorizedCommand (command_id, context)) {
        throw CommandError (E_UNAUTHORIZED);
    }
    return handleCommand (command_id, parameters, context);
}

/** Register the interpreter with a dispatcher.
    @param dispatcher The dispatcher with which to register. */
void PianodInterpreter::registerInterpreter (PianodDispatcher &dispatcher) {
    dispatcher.addHandler (getParserDefinitions(), this);
}

/** Search the interpreter's definitions for any definitions matching search criteria.
    @param search The search criteria.  If empty, returns all.
    @param option_parsers Any option parsers used by matching statements are added to this set.
    @return Matching definitions, ready for presentation as help. */
const std::vector<std::string> PianodInterpreter::getHelp (const std::vector<std::string> &search,
        std::set<std::string> *option_parsers) {
    const Parsnip::Parser::Definitions &defs = getParserDefinitions();
    HelpList results;
    for (const auto &item : defs) {
        if (helpMatches (item.statement, search)) {
            results.push_back (simplify_statement (item.statement, option_parsers));
        }
    }
    return results;
}
