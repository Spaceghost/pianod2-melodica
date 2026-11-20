///
/// Parsnip command-line parsing.
/// @file       parsnip_command.cpp - Parsnip serialization & parsing
/// @author     Perette Barella
/// @date       2020-05-06
/// @copyright  Copyright 2012-2021 Devious Fish. All rights reserved.
/// History:    This parser is based on the Football parser from 2012.
///

/** @details
 *
 *  Classes providing evaluation for command lines.
 * 
 *  The Parser class accepts arrays of strings/command ID combinations
 *  (Parser::Definition).  From these, it builds a parse tree against which
 *  it can subsequently evaluate commands, identifying commands by ID and
 *  providing a dictionary the commands parameters.
 *  Multiple definition sets can be added to a single parser.
 *
 *  Lexically, commands are split at spaces, unless quoted.  Single or double
 *  quotes may be used, but must match.  Open quotes are only identified at
 *  the start of a word, close quotes only at the end of a word.
 *  Quotes may be doubled within a quoted span to insert a literal quote.
 *
 *  Statement patterns can be composed of:
 *
 *  Literal:
 *      - `keyword`           -- a bare word for matching command pattern.
 *  Alternation:
 *      - `<name:one|two>`    -- accepts any 1 of the required words in that position
 *  Optional:
 *      - `[name:optional]`   -- accepts an optional word in the current position
 *      - `[name:four|five]`  -- accepts 0 or 1 optional words in the current position
 *      - `[word]`            -- accepts an optional word, named itself, in the current position
 *  Fillable fields:
 *      - `{name}`            -- accepts any string/number/value in that position
 *      - `{#name:3-5}`       -- Accepts numeric range in that position.  See range note below.
 *      - `{name...}`         -- accepts the remainder of the command line, raw/unparsed as a single string.
 *  Optional remainder fillable fields (only at end of command):
 *      - `[{name}]`          -- accepts a single optional value; may be string or numeric fillable.
 *      - `[{name}] ...`      -- accepts 0 or more optional values; may be string or numeric fillable.
 *      - `[{name...}]`       -- accepts the raw/unparsed remainder, if present.  Always string.
 *
 *  Names:
 *      - Must not vary within consistent parsing patterns
 *      - May be reused in different branches; use with care.
 *
 *  Alternation and optional patterns may contain an arbitrary number of possible keywords,
 *  Storage behavior:
 *      - Numeric values with ranges are evaluted and stored in the dictionary as their proper type.
 *      - Alternation options are stored as strings.
 *      - Optional keywords are stored as strings when present, otherwise omitted.
 *      - Remainders (parsed form, optional or not) return an array of the appropriate type.
 *        No values returns an empty array, rather than the array being omitted.
 *
 *  Ranges:
 *      - If either min or max in range have a decimal point, values are treated as
 *        doubles.  Otherwise, they are considered integers.
 *      - For integers, if either min or max has a leading zero, it enables radix detection:
 *        0 -> octal, 0x -> hex, otherwise decimal.
 *      - If neither max nor min has a leading 0, only base 10 is accepted.
 *      - Reals are always evaluated base 10.
 *
 *  Examples: @code
 *      { CREATE_USER, "create <rank:admin|guest|user> {username} {password}" }
 *          -- Recognizes the 3 types of users and requires username & password parameter, nothing more.
 *      { CREATE_USER_EXTENDED, "create <rank:admin|guest|user> {username} {password} [{additional}] ..." }
 *          -- Recognizes the 3 types of users and requires username, password, & at least 1 additional parameter.
 *      { CREATE_USER_OPTIONAL_EXTENDED, "create <rank:admin|guest|user> {user} {password} ... }
 *          -- Recognizes the 3 types of users, requires username and password, allows additional parameters.
 *             This definition is incompatible with CREATE_USER above--the parser would error when adding this
 *             definition.  (There is no way to delineate that command and this command with 0 parameters.)
 *  @endcode
 *      A completely different approach to the above would be: @code
 
 *      { CREATE_ADMIN_USER,    "create admin {username} {password} ..." },
 *      { CREATE_STANDARD_USER, "create standard {username} {password} ..." },
 *      { CREATE_GUEST_USER,    "create guest {username} {password} ..." }
 *          -- Returns different values for the different user types, so you can determine that
 *             from the command's enumeration instead of having to further parse. As written, these
 *             would each allow additional optional parameters beyond username and password.
 *  @endcode
 *
 *   Since the parse tree is assumed to be hard-coded in your application, this module simply
 *   throws a runtime_error it finds problems in the parse definitions.  
 *
 *  **Options parsing**
 *
 *   An option parser may be used inline or as a remainder handler:
 *
 *      - {name:parser}     -- accepts one matching sequence in that position
 *      - {name:parser} ... -- accepts 0 or more trailing sequences
 *
 *  The option parser must be predefined and registered with the containing Parser
 *  prior to adding statements that reference it.  Values from within the option
 *  parser sequences are stored in a dictionary named `name`.
 *
 *  - It is an error to specify the same option twice.
 *  - Remainder fill-ins are allowed.
 *  - If the parser name in the fill-in referencing it is written as `<name>`,
 *    the first keyword of each option is are used to select the option parser.
 *
 *    Example: `{options:<my_options>}`
 *
 *    This allows optionals before the option parser, or multiple parallel option
 *    parsers.  Only one option parser will be used at a time (though an optional
 *    non-iterating option parsing could precede a final iterating one).
 *
 */

#include <config.h>

#include <vector>
#include <unordered_map>
#include <string>
#include <exception>
#include <stdexcept>

#include <cassert>

#include "parsnip.h"
#include "parsnip_command.h"
#include "parsnip_evaluate.h"
#include "parsnip_argv.h"

namespace Parsnip {

    const std::string EmptyString;

    /** Construct token handler accepting a list of literal strings
        from the parsing definition.
        @param parser See Evaluator::construct
        @param cursor See Evaluator::construct
        @param id See Evaluator::construct
        @param parent_parser Passed through; see ValueEvaluator::construct.
        @param name The name by which to access the selected keyword.
        @param keywords The list of keywords, as an alternation string (a|b|c). */
    void KeywordEvaluator::construct (EvaluatorRef &parser,
                                      const ArgvCursor &cursor,
                                      CommandId id,
                                      Parser *parent_parser,
                                      const std::string &name,
                                      std::string keywords) {
        ValueEvaluator *value_fillin = dynamic_cast<ValueEvaluator *> (parser.get());
        KeywordEvaluator *keyparser{nullptr};
        if (value_fillin) {
            // If existing evaluator was a numeric input, make it our numeric value handler
            parser_assert (!dynamic_cast<StringEvaluator *> (value_fillin), "String fill-in cannot parallel keywords");
            keyparser = new KeywordEvaluator();
            keyparser->numeric_evaluator = std::move (parser);
            parser.reset (keyparser);
        } else {
            keyparser = uptype_construct<KeywordEvaluator> (parser);
        }

        std::string::size_type word_begin = 0;
        while (word_begin != std::string::npos) {
            auto word_end = keywords.find ('|', word_begin);
            std::string keyword;
            if (word_end == std::string::npos) {
                keyword = tolower (keywords.substr (word_begin));
                word_begin = std::string::npos;
            } else {
                keyword = tolower (keywords.substr (word_begin, word_end - word_begin));
                word_begin = word_end + 1;
            }
            keyparser->numbers_present = keyparser->numbers_present || is_numeric (keyword);
            if (keyparser->numeric_evaluator) {
                parser_assert (!keyparser->numbers_present, "Numeric keywords cannot parallel numeric fill-in");
            }
            EvaluatorRef &next_eval = keyparser->tokens[keyword];
            Evaluator::construct (next_eval, cursor + 1, id, parent_parser);
            Evaluator::set_name (next_eval, name);
        }
    }

    /** Construct token handler accepting a fixed list of values
        from the parsing definition.
        @param parser See Evaluator::construct
        @param cursor See Evaluator::construct
        @param id See Evaluator::construct
        @param parent_parser The containing parser, with previously
        constructed option parsers that may be referenced.
        @param name The name by which to access the selected keyword.
        @param format Format and range details for numbers. */
    /// Construct a value/fill-in token handler
    void ValueEvaluator::construct (EvaluatorRef &parser,
                                    const ArgvCursor &cursor,
                                    CommandId id,
                                    Parser *parent_parser,
                                    std::string name,
                                    std::string format) {
        ArgvCursor next = cursor + 1;
        bool more = !next.isEnd();
        bool the_rest = (more && next.value() == "...");
        bool raw_remainder = name.size() > 3 && name.substr (name.size() - 3) == "...";
        bool regexp = format.size() > 3 && format [0] == '/';
        bool numeric = (name[0] == '#');
        if (numeric) {
            name.erase (0, 1);
        }
        bool option_parser = parent_parser && !regexp && !numeric && !format.empty();

        RemainderEvaluator *collector{nullptr};
        KeywordEvaluator *keyword_evaluator{nullptr};
        if (option_parser) {
            parser_assert (!raw_remainder, "Option parser cannot be raw remainder");
            std::vector <OptionEvaluator *> collectors = OptionEvaluator::construct (parser, format, the_rest, parent_parser);
            for (RemainderEvaluator *collector : collectors) {
                Evaluator::construct (collector->terminating_evaluator, the_rest ? next + 1 : next, id, parent_parser);
                Evaluator::set_name (collector->terminating_evaluator, name);
            }
            return;
        } else if (the_rest) {
            parser_assert ((cursor + 2).isEnd(), "Remainder must be at end");
            parser_assert (!raw_remainder, "Remainder cannot collect raw remainders");
            collector = uptype_construct<RemainingValuesEvaluator> (parser);
        } else if ((keyword_evaluator = dynamic_cast<KeywordEvaluator *> (parser.get()))) {
            parser_assert (numeric, "Only numeric inputs allowed alongside keywords");
            parser_assert (!keyword_evaluator->numbers_present, "Numeric input cannot be alongside numeric keywords");
        }
        EvaluatorRef &value_parser = (collector ? collector->terminating_evaluator
                                                : keyword_evaluator ? keyword_evaluator->numeric_evaluator : parser);
        ValueEvaluator *vp{nullptr};

        if (raw_remainder) {
            parser_assert (!numeric, "Raw remainder must not be numeric");
            parser_assert (!more, "Raw remainder must be at end");
            name.erase (name.size() - 3, 3);
            vp = uptype_construct<RawRemainderEvaluator> (parser);
        } else if (numeric) {
            parser_assert (!format.empty(), "Missing range");
            auto sep = format.find ('-', 1);
            parser_assert (sep != std::string::npos, "missing \'-\' in range");
            auto min = format.substr (0, sep);
            auto max = format.substr (sep + 1);
            if (format.find (".") == std::string::npos) {
                vp = IntegerEvaluator::construct (value_parser, name, min, max);
            } else {
                vp = RealEvaluator::construct (value_parser, name, min, max);
            }
        } else if (regexp) {
            format.erase (0, 1);
            bool ignore = format.substr (format.size() - 2) == "/i";
            if (ignore) {
                format.erase (format.size() - 2);
            } else {
                assert (format [format.size() - 1] == '/');
                format.erase (format.size() - 1);
            }
            vp = RegExEvaluator::construct (value_parser, name, format, ignore);
        } else {
            parser_assert (format.empty(), "Unknown format for string value");            
            vp = uptype_construct<StringEvaluator> (value_parser);
        }
        if (collector) {
            Evaluator::construct (collector->terminating_evaluator, the_rest ? next + 1 : next, id, parent_parser);
            Evaluator::set_name (collector->terminating_evaluator, name);
            if (vp) {
                Evaluator::construct (vp->next_evaluator, the_rest ? next + 1 : next, id, parent_parser);
            }
        } else {
            Evaluator::construct (vp->next_evaluator, next, id, parent_parser);
            Evaluator::set_name (vp->next_evaluator, name);
        }
    }

    /** Construct token handler from the token in a parsing definition.
        @param parser The parser to update or construct with new definitions.
        @param cursor Points to the token defining what to construct.
        @param id The command ID to use for completed commands of this definition.
        @param parent_parser Passed through; see ValueEvaluator::construct. */
    void Evaluator::construct (EvaluatorRef &parser,
                               const ArgvCursor &cursor,
                               CommandId id,
                               Parser *parent_parser) {
        if (cursor.isEnd()) {
            if (!parser) {
                parser.reset (new TerminatorEvaluator);
            }
            parser->setCommandId (id);
            return;
        }
        StringType token = cursor.value();
        assert (!token.empty());
        auto type = token[0];
        if (type != '{' && type != '[' && type != '<') {
            KeywordEvaluator::construct (parser, cursor, id, parent_parser, EmptyString, cursor.value());
            return;
        }

        // Some basic checks on what this token is
        parser_assert (token.size() >= 3, "Pattern too short");
        std::string::size_type name_start = 1;
        bool mandatory = (type != '[');
        auto check = token[token.size() - 1];
        if (type == '[') {
            parser_assert (check == ']', "Unmatched [");
            auto inner_type = token[name_start];
            if (inner_type == '{') {
                parser_assert (token.size() >= 5, "Pattern too short");
                parser_assert ((cursor + 1).isEnd() || ((cursor + 2).isEnd() && (cursor + 1).value() == "..."),
                               "Optional fill-in must be at end");
                type = inner_type;
                check = token[token.size() - 2];
                name_start++;
            }
        }
        // Check close bracket type
        parser_assert ((type != '{') || (check == '}'), "Unmatched {");
        parser_assert ((type != '[') || (check == ']'), "Unmatched [");
        parser_assert ((type != '<') || (check == '>'), "Unmatched <");

        token.erase (token.size() - name_start, name_start);
        token.erase (0, name_start);

        // Extract element's name
        std::string name;
        std::string format;
        auto name_end = token.find (':');
        if (name_end == std::string::npos) {
            name = token;
        } else {
            name = token.substr (0, name_end);
            format = token.substr (name_end + 1);
        }
        switch (type) {
            case '{':
                ValueEvaluator::construct (parser, cursor, id, parent_parser, name, format);
                if (!mandatory) {
                    parser->setCommandId (id);
                }
                break;
            case '<':
                if (format.empty()) {
                    std::swap (format, name);
                }
                /* FALLTHROUGH */
            case '[':
                KeywordEvaluator::construct (parser, cursor, id, parent_parser, name, format.empty() ? name : format);
                if (!mandatory) {
                    construct (parser, cursor + 1, id, parent_parser);
                }
                break;
            default:
                assert (!"Unreachable");
        }
    }

    /*
     *              Parser
     */

    /** Evaluate a token in the command line, and recurse if
        there is more to go.
        @param cursor Provides the command line tokens.
        @param result_dict Dictionary into which to put values.
        @return ID code of command selected. */
    int Evaluator::evaluate (ArgvCursor *cursor, Parsnip::Data &result_dict) {
        if (cursor->isEnd()) {
            auto rve = dynamic_cast<RemainingValuesEvaluator *> (this);
            if (rve) {
                // Special case: Allow remaing values evaluators at the end to return empty lists.
                result_dict[rve->terminating_evaluator->name] = evaluateToken (cursor);  // Modifies cursor
            }
            if (command_id == NoCommand) {
                if (cursor->isStart()) {
                    throw IncompleteCommand();
                } else {
                    throw IncompleteCommand ("after " + (*cursor - 1).value());
                }
            }
            return command_id;
        }
        Evaluator *next = getNextEvaluator (cursor->value());
        if (!next) {
            assert (command_id == EndOfOption);
            return 1;
        }
        if (!next->name.empty()) {
            if (result_dict.contains (next->name)) {
                throw DuplicateOption (cursor->value());
            }
            result_dict[next->name] = evaluateToken (cursor);  // Modifies cursor
        } else {
            (*cursor)++;
        }
        return next->evaluate (cursor, result_dict);
    }

    /** Construct a parser and initialize it with some statements.
        @param defs The statement definitions. */
    Parser::Parser (const Definitions &defs) {
        addStatements (defs);
    }

    /** Add statements to the parser.
        @param defs The statement definitions. */
    void Parser::addStatements (const Parser::Definitions &defs) {
        auto it = defs.begin();
        try {
            for (; it != defs.end(); it++) {
                assert (it->statement);
                ArgumentVector argv (it->statement);
                Evaluator::construct (evaluator, ArgvCursor (&argv), it->command_id, this);
            }
        } catch (std::runtime_error &ex) {
            throw std::runtime_error (std::string (it->statement) + ": " + ex.what());
        }
    }

    /** Register an options parser for use.
        @param name The option parser's name.
        @param add The option parser to register. */
    void Parser::addOptionParser (const std::string &name, const OptionParserRef &add) {
        parser_assert (option_parsers.find (name) == option_parsers.end(), "option parser already exists", name);
        option_parsers [name] = add;
    }

    /** Evaluate a command.
        @param command the command.
        @return A result, which contains the command ID and extracted parameters.
        @throw Exceptions representing various command errors. */
    Parser::Result Parser::evaluate (const StringType &command) const {
        ArgumentVector argv (command);
        Parser::Result result;
        ArgvCursor cursor (&argv);
        assert (evaluator);
        result.command_id = evaluator->evaluate (&cursor, result.parameters);
        assert (cursor.isEnd());
        return result;
    }

    /** Check if a parser is identical to another parser. */
    bool Parser::operator== (const Parser &other) const {
        return (*evaluator == *(other.evaluator));
    }

    /*
     *              Option Parser
     */

    /** Construct a new option parser initialized with some
        option patterns.
        @param defs The initial option patterns.
        @param aggregate Optional pointer to aggregate parser containing referenced parsers. */
    OptionParser::OptionParser (const Definitions &defs, Parser *aggregate) {
        addOptions (defs, aggregate);
    }

    /** Register new option patterns.
        @param defs The option patterns.
        @param aggregate Optional pointer to parser containing referenced parsers. */
    void OptionParser::addOptions (const Definitions &defs, Parser *aggregate) {
        auto it = defs.begin();
        try {
            for (; it != defs.end(); it++) {
                assert (*it);
                ArgumentVector argv (*it);
                Evaluator::construct (evaluator, ArgvCursor (&argv), Evaluator::EndOfOption, aggregate);
            }
        } catch (std::runtime_error &ex) {
            throw std::runtime_error (std::string (*it) + ": " + ex.what());
        }
        evaluator->convertToOptionEvaluator();
    }
    
    bool OptionParser::operator== (const OptionParser &other) const {
        return *evaluator == *(other.evaluator);
    }

}  // namespace Parsnip
