///
/// Parsnip command-line parsing.
/// @file       parsnip_command.h - Parsnip serialization & parsing
/// @author     Perette Barella
/// @date       2020-05-06
/// @copyright  Copyright 2012-2021 Devious Fish. All rights reserved.
/// history     This parser is based on the Football parser from 2012.
///

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <exception>
#include <functional>

#include "parsnip.h"

namespace Parsnip {
    /// NumberOutOfRange: The number was outside the range defined in the command pattern.
    class NumberOutOfRange : public Exception {
    public:
        inline NumberOutOfRange() : Exception ("Value out of range"){};
        inline NumberOutOfRange (const std::string &value) : Exception ("Value out of range", value){};
    };

    /// Not numeric: A command pattern specified a number, but the token was not valid number.
    class NotNumeric : public Exception {
    public:
        inline NotNumeric() : Exception ("Not a number"){};
        inline NotNumeric (const std::string &value) : Exception ("Not a number", value){};
    };

    /// A token encountered was not one of the expected keywords.
    class InvalidValue : public Exception {
    public:
        inline InvalidValue() : Exception ("Invalid value"){};
        inline InvalidValue (const std::string &value) : Exception ("Invalid value", value){};
    };

    /// More tokens were expected.
    class IncompleteCommand : public Exception {
    public:
        inline IncompleteCommand() : Exception ("Incomplete command"){};
        inline IncompleteCommand (const std::string &value) : Exception ("Incomplete command", value){};
    };

    /// There were extra tokens at the end of the command pattern.
    class RunOnCommand : public Exception {
    public:
        inline RunOnCommand() : Exception ("Run-on command"){};
        inline RunOnCommand (const std::string &value) : Exception ("Run-on command", value){};
    };
    
    /// An option was supplied twice to an iterating option parser.
    class DuplicateOption : public Exception {
    public:
        inline DuplicateOption() : Exception ("Option respecified: "){};
        inline DuplicateOption (const std::string &value) : Exception ("Option respecified", value){};
    };

    /// A schema is not defined for a certain command ID
    class NoSchemaDefined : public Exception {
    public:
        inline NoSchemaDefined() : Exception ("No Schema defined: "){};
        inline NoSchemaDefined (std::string command) : Exception ("No schema defined", command){};
        inline NoSchemaDefined (const int command) : Exception ("No schema defined", std::to_string (command)){};
    };

    extern const std::string EmptyString;

    /** This does what std::default_delete would do, but is required since
        we haven't defined Evaluator yet. */
    struct EvaluatorDeleter {
        void operator() (class Evaluator *free_me);
    };
    using EvaluatorRef = std::unique_ptr<class Evaluator, EvaluatorDeleter>;

    /// A class for parsing command line options (name-value pairs, for instance).
    class OptionParser {
        friend class OptionEvaluator;
        friend class Schema;
    public:
        EvaluatorRef evaluator;

    public:
        // Structure for defining new option patterns.
        using Definitions = std::vector<const char *>;

        OptionParser() = default;
        OptionParser (const Definitions &defs, class Parser * = nullptr);
        void addOptions (const Definitions &defs, class Parser * = nullptr);
        bool operator== (const OptionParser &) const;
    };
    using OptionParserRef = std::shared_ptr<OptionParser>;

    /// A class for parsing command lines.
    class Parser {
        friend class OptionEvaluator;
        friend class SchemaSet;
    private:
        EvaluatorRef evaluator;
        using OptionParsers = std::unordered_map<std::string, OptionParserRef>;
        OptionParsers option_parsers;

    public:
        using CommandId = int;
        using StringType = std::string;

        // Structure for defining new parser patterns.
        struct Definition {
            CommandId command_id;
            const char *statement;
        };
        using Definitions = std::vector<Definition>;

        // Structure for evaluation results.
        struct Result {
            ///< ID of command pattern that was matched.
            CommandId command_id;
            /// Parameters collected from the command line.
            Parsnip::Data parameters{Parsnip::Data::Dictionary};
        };

        Parser() = default;
        Parser (const Definitions &defs);
        virtual ~Parser() = default;
        void addOptionParser (const std::string &name, const OptionParserRef &add);
        void addStatements (const Definitions &defs);
        Result evaluate (const StringType &command) const;
        bool operator== (const Parser &) const;
        inline bool operator!= (const Parser &other) const {
            return !(*this == other);
        }
    };
    using ParserRef = std::shared_ptr<Parser>;

    template <typename ReturnType, typename ContextType>
    class Interpreter {
    public:
        virtual ReturnType interpret (Parser::CommandId command_id, const Parsnip::Data &parameters, ContextType context) = 0;
    };
    
    // A parser that includes dispatching capabilities.
    template <typename DispatchReturnType, typename DispatchContextType>
    class Dispatcher {
    protected:
        const ParserRef parser;
    public:
        using ReturnType = DispatchReturnType;
        using ContextType = DispatchContextType;
        using ExecutorType = std::function <ReturnType (Parser::CommandId, const Parsnip::Data &, ContextType)>;
    private:
        std::unordered_map <Parser::CommandId, ExecutorType> handlers;
#ifndef NDEBUG
        int parser_number = 0;
        std::unordered_map <Parser::CommandId, int> mapping_check;
#endif
    public:
        Dispatcher (const ParserRef &parser);
        void addHandler (const Parser::Definitions &defs, const ExecutorType &handler);
        inline void addHandler (const Parser::Definitions &defs, Interpreter<DispatchReturnType, DispatchContextType> *interpreter);
        void addStatements (const Parser::Definitions &defs, ExecutorType handler);
        ReturnType operator () (Parser::CommandId command_id, const Parsnip::Data &parameters, DispatchContextType context) const;
        ReturnType operator () (Parser::StringType command, ContextType context) const;
    };

    /// A schema built to validate JSON messages based on command line patterns
    class SchemaSet {
    public:
        using CommandId = int;
        using CommandSchemas = std::unordered_map <CommandId, SchemaBaseRef>;
        using OptionSchemas = Schema::OptionSchemas;
        using Dependencies = Schema::Dependencies;
    private:
        /// Schemas for each unique command ID.
        CommandSchemas schemas;
        void integrateSchema (const int, const class DictionarySchema &);
        SchemaBaseRef &getSchemaForCommand (CommandId command_id);
        const SchemaBaseRef &getSchemaForCommand (CommandId command_id) const;

    public:
        /// Schemas for each of the option parsers.
        OptionSchemas option_schemas;
    public:
        SchemaSet (const class Parser &from);
        void validate (const CommandId, const Data &) const;
        std::ostream &dump (const std::string &intro, const CommandId command_id, std::ostream &target = std::clog) const;

        void addMember (const CommandId, const char *name, const SchemaBase &schema, bool mandatory = false,
                        const Dependencies &dependencies = Schema::NoDependencies);
        void addMember (const char *name, const SchemaBase &schema, bool mandatory = false,
                        const Dependencies &dependencies = Schema::NoDependencies);
        void replaceMember (const CommandId, const char *name, const SchemaBase &schema);
        void removeMember (const CommandId, const char *name);
    };
    using SchemaSetRef = std::shared_ptr <SchemaSet>;


    /*
     *
     *              TEMPLATE IMPLEMENTATIONS
     *
     */
     
    /*
     *              Dispatcher
     */

    /** Construct a dispatcher.
        @param par A parser for evaluating command lines to be dispatched. */
    template <typename DispatchReturnType, typename DispatchContextType>
    Dispatcher<DispatchReturnType, DispatchContextType>::Dispatcher (const ParserRef &par)
    : parser (par) {
    }

    /** Register handlers with the dispatcher.
        If assertions aren't enabled, keeps a secondary map to ensure statements are not
        redefined with different handlers.
        @param defs The statements handled by the handler.
        @param handler Something to call when dispatching to defined statements. */
    template <typename DispatchReturnType, typename DispatchContextType>
    void Dispatcher<DispatchReturnType, DispatchContextType>::addHandler (const Parser::Definitions &defs, const ExecutorType &handler) {
#ifndef NDEBUG
        parser_number++;
#endif
        for (auto &it : defs) {
#ifndef NDEBUG
            if (mapping_check [it.command_id]) {
                assert (mapping_check [it.command_id] == parser_number);
            } else {
                mapping_check [it.command_id] = parser_number;
            }
#endif
            handlers [it.command_id] = handler;
        }
    }


    /** Register handlers with the dispatcher.
        @param defs The statements handled by the handler.
        @param interpreter An Interpreter whose `interpret` routine will be
        used as the handler. */
    template <typename DispatchReturnType, typename DispatchContextType>
    void Dispatcher<DispatchReturnType, DispatchContextType>::addHandler (const Parser::Definitions &defs,
                Interpreter <DispatchReturnType, DispatchContextType> *interpreter) {
        ExecutorType handler { std::bind (&Interpreter<DispatchReturnType, DispatchContextType>::interpret,
                               interpreter, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) };
        addHandler (defs, handler);
    }

    /** Add statements to the parser and register their related handler.
        @param defs The statements to add and register.
        @param handler Something to call when dispatching to defined statements. */
    template <typename DispatchReturnType, typename DispatchContextType>
    void Dispatcher<DispatchReturnType, DispatchContextType>::addStatements (const Parsnip::Parser::Definitions &defs, ExecutorType handler) {
        parser->addStatements (defs);
        addHandler (defs, handler);
    }

    /** Dispatch a command to the executor/handler function.
        @param command_id Identifier for the requested handler.
        @param parameters Details passed to the handler.
        @param context The caller's context.
        @return Whatever value is returned by the handler. */
    template <typename DispatchReturnType, typename DispatchContextType>
    DispatchReturnType Dispatcher<DispatchReturnType, DispatchContextType>::operator () (Parser::CommandId command_id,
                                                                                         const Parsnip::Data &parameters,
                                                                                         ContextType context) const {
        return handlers.at (command_id) (command_id, parameters, context);
    }
    
    /** Parse and dispatch a command to the executor/handler function.
        @param command The command line to parse & execute.
        @param context The caller's context.
        @return Whatever value is returned by the handler. */
    template <typename DispatchReturnType, typename DispatchContextType>
    DispatchReturnType Dispatcher<DispatchReturnType, DispatchContextType>::operator () (Parser::StringType command,
                                                                                         ContextType context) const {
        Parser::Result result = parser->evaluate (command);
        return (*this) (result.command_id, result.parameters, context);
    }

}  // namespace Parsnip
