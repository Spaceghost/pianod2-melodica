///
/// Evaluators for Parsnip command-line parsing.
/// @file       parsnip_evaluate.h - Parsnip serialization & parsing
/// @author     Perette Barella
/// @date       2020-05-06
/// @copyright  Copyright 2012-2021 Devious Fish. All rights reserved.
/// history     This parser is based on the Football parser from 2012.
///

#include <vector>
#include <regex>

#include "parsnip_command.h"
#include "parsnip_schema.h"

namespace Parsnip {
    inline void parser_assert (bool requirement, const char *explanation) {
        if (!requirement) {
            throw std::runtime_error (explanation);
        }
    }

    inline void parser_assert (bool requirement, const char *explanation, const std::string &detail) {
        if (!requirement) {
            throw std::runtime_error (std::string (explanation) + ": " + detail);
        }
    }

    /// Evaluator base class.
    class Evaluator {
        friend class ValueEvaluator;
        friend class KeywordEvaluator;
        friend class RemainingValuesEvaluator;
        friend class RemainderEvaluator;
        friend class OptionEvaluator;
        friend class OptionParser;
        friend class Schema;
        friend class SchemaSet;

    public:
        using StringType = std::string;
        using CommandId = int;

    private:
        static const CommandId NoCommand = -1;
        static const CommandId EndOfOption = -2;

    protected:
        /// If this completes a command, the command ID.
        CommandId command_id{NoCommand};
        /// If not empty, the name by which this value shall be accessed.
        std::string name;

        Evaluator() = default;

    public:
        Evaluator (Evaluator *original);
        virtual ~Evaluator() = default;

    protected:
        static void set_name (EvaluatorRef &target, const std::string &name);
        void setCommandId (CommandId id);

        /** Determine if and how to evaluate additional command line tokens.
            @param token The current token being evaluated.
            @return The evaluator for the next token, or nullptr if evaluation is complete. */
        virtual Evaluator *getNextEvaluator (const StringType &token) const = 0;

        /** Evaluate command line tokens.
            @param cursor Provides tokens from the command line.
            Must be bumped forward as they are processed.
            @return Parsnip data object containing evaluated token. */
        virtual Parsnip::Data evaluateToken (class ArgvCursor *cursor) const = 0;

        /** Make any changes to the parse tree to convert it to an options evaluator. */
        virtual void convertToOptionEvaluator() = 0;
        
        /** Construct a schema from an evaluator tree.
            This function depth-first searches the evaluation tree,
            calling the integrator function at leaf nodes and other
            valid statement endpoints.
            @param schema The schema, as constructed from the parse tree root.
            @param option_schemas Schemas for option parsers that may be needed.
            @param integrator An integrator that merges various statement
            patterns into a complete schema.
            @throw NoSuchKey A necessary option schema was not found.
            @throw SchemaConflict Statement definitions generate
            schema inconsistencies. */
        virtual void constructSchema (const DictionarySchema &schema,
                                      const Schema::OptionSchemas &option_schemas,
                                      const SchemaIntegratorCallback &integrator) const = 0;

    public:
        static void construct (EvaluatorRef &evaluator,
                               const ArgvCursor &cursor,
                               CommandId id,
                               Parser *parent_parser);
        int evaluate (ArgvCursor *cursor, Parsnip::Data &result_dict);
        virtual bool operator== (const Evaluator &) const;
        inline bool operator!= (const Evaluator &other) const {
            return !(*this == other);
        }
    };

    /** If an evaluator isn't constructed, create it.
        If it exists and is the right type, leave it alone.
        If it is a TerminatorEvaluator, upgrade it to the type we want.
        @tparam DesiredEvaluator The type of evaluator we want.
        @param parser A pointer container for the existing evaluator.
        @param existed If not null, set to indicate whether parser was already the specified type.
        @return A pointer to the parser. */
    template <class DesiredEvaluator>
    static inline DesiredEvaluator *uptype_construct (EvaluatorRef &parser, bool *existed = nullptr) {
        DesiredEvaluator *possess = dynamic_cast<DesiredEvaluator *> (parser.get());
        if (existed) {
            // If not null, set existed to indicate if object was already of required type.
            *existed = (possess != nullptr);
        }
        if (!possess) {
            possess = new DesiredEvaluator (parser.get());
            parser.reset (possess);
        }
        return possess;
    }

    /// An evaluator representing abrupt end of the command pattern.
    class TerminatorEvaluator : public Evaluator {
        virtual Evaluator *getNextEvaluator (const StringType &value) const override;
        virtual Parsnip::Data evaluateToken (ArgvCursor *cursor) const override;
        virtual void convertToOptionEvaluator() override;
        virtual void constructSchema (const DictionarySchema &scheme,
                                      const Schema::OptionSchemas &option_schemas,
                                      const SchemaIntegratorCallback &integrator) const override;
    };

    /// An evaluator representing abrupt end of the command pattern.
    class EndOfOptionEvaluator : public Evaluator {
        virtual Evaluator *getNextEvaluator (const StringType &value) const override;
        virtual Parsnip::Data evaluateToken (ArgvCursor *cursor) const override;
        virtual void convertToOptionEvaluator() override;
        virtual void constructSchema (const DictionarySchema &scheme,
                                      const Schema::OptionSchemas &option_schemas,
                                      const SchemaIntegratorCallback &integrator) const override;

    public:
        using Evaluator::Evaluator;
    };

    /// Evaluate a value/fill-in field.
    class ValueEvaluator : public Evaluator {
    protected:
        EvaluatorRef next_evaluator;

        virtual Evaluator *getNextEvaluator (const StringType &value) const override;

    public:
        using Evaluator::Evaluator;
        virtual bool operator== (const Evaluator &other) const override;
        virtual void convertToOptionEvaluator() override;

        virtual SchemaBase *getSchema() const = 0;
        virtual void constructSchema (const DictionarySchema &scheme,
                                      const Schema::OptionSchemas &option_schemas,
                                      const SchemaIntegratorCallback &integrator) const override;

        /// Construct a value/fill-in token handler
        static void construct (EvaluatorRef &parser,
                               const ArgvCursor &cursor,
                               CommandId id,
                               Parser *parent_parser,
                               std::string name,
                               std::string keywords);
    };

    /// Evaluate a string value field.
    class StringEvaluator : public ValueEvaluator {
    public:
        using ValueEvaluator::ValueEvaluator;
        virtual Parsnip::Data evaluateToken (ArgvCursor *cursor) const override;
        virtual SchemaBase *getSchema() const override;
    };
    
    // A string evaluator that is checked by a regular expression
    /// Evaluate a string value field.
    class RegExEvaluator : public StringEvaluator {
        StringType expression; ///< The string defining the regular expression.
        using RegexType = std::basic_regex <StringType::value_type>;
        RegexType regex; ///< The compiled regular expression.
    public:
        using StringEvaluator::StringEvaluator;
        virtual Parsnip::Data evaluateToken (ArgvCursor *cursor) const override;
        virtual bool operator== (const Evaluator &other) const override;
        virtual SchemaBase *getSchema() const override final;

        static RegExEvaluator *construct (EvaluatorRef &evaluator,
                                          const std::string &name,
                                          const StringType &expression,
                                          const bool case_blind);
    };

    /// Gather the rest of the command line as an untokenized string.
    class RawRemainderEvaluator : public StringEvaluator {
    public:
        using StringEvaluator::StringEvaluator;
        virtual Parsnip::Data evaluateToken (ArgvCursor *cursor) const override;
    };

    /// Evaluate and validate an integral value field.
    class IntegerEvaluator : public ValueEvaluator {
        long minimum;   ///< Minimum acceptable value.
        long maximum;   ///< Maximum acceptable value.
        int radix = 1;  ///< 0 = automatic, otherwise the base.

    public:
        using ValueEvaluator::ValueEvaluator;
        virtual Parsnip::Data evaluateToken (ArgvCursor *cursor) const override;
        virtual bool operator== (const Evaluator &other) const override;
        virtual SchemaBase *getSchema() const override final;

        static IntegerEvaluator *construct (EvaluatorRef &evaluator,
                                            const std::string &name,
                                            const std::string &min,
                                            const std::string &max);
    };

    /// Evaluate and validate a numeric value field.
    class RealEvaluator : public ValueEvaluator {
        double minimum;  ///< Minimum acceptable value.
        double maximum;  ///< Maximum acceptable value.

    public:
        using ValueEvaluator::ValueEvaluator;
        virtual Parsnip::Data evaluateToken (ArgvCursor *cursor) const override;
        virtual bool operator== (const Evaluator &other) const override;
        virtual SchemaBase *getSchema() const override final;

        static RealEvaluator *construct (EvaluatorRef &parser,
                                         const std::string &name,
                                         const std::string &min,
                                         const std::string &max);
    };

    /// Evaluate a keyword in a command line.
    class KeywordEvaluator : public Evaluator {
        friend class ValueEvaluator;
        friend class OptionEvaluator;

        EvaluatorRef numeric_evaluator;
        std::unordered_map<StringType, EvaluatorRef> tokens;
        bool numbers_present{false};  ///< True if some of the tokens are numeric.

    public:
        using Evaluator::Evaluator;

        inline static bool is_numeric (const std::string &value) {
            return ((!value.empty() && isdigit (value[0]))
                    || (value.size() >= 2 && value[0] == '-' && isdigit (value[1])));
        }

        inline static std::string tolower (std::string value) {
            for (auto &ch : value) {
                ch = ::tolower (ch);
            }
            return value;
        }

        virtual Parsnip::Data evaluateToken (ArgvCursor *cursor) const override;
        virtual Evaluator *getNextEvaluator (const StringType &token) const override;
        virtual bool operator== (const Evaluator &other) const override;
        virtual void convertToOptionEvaluator() override;

        /// Construct a keyword token handler
        static void construct (EvaluatorRef &parser,
                               const ArgvCursor &cursor,
                               CommandId id,
                               Parser *parent_parser,
                               const std::string &name,
                               std::string keywords);
        virtual void constructSchema (const DictionarySchema &scheme,
                                      const Schema::OptionSchemas &option_schemas,
                                      const SchemaIntegratorCallback &integrator) const override;
    };

    /// An evaluator that represents the rest of the command line.
    class RemainderEvaluator : public Evaluator {
        friend class Evaluator;
        friend class RemainingValuesEvaluator;
        friend class ValueEvaluator;

    protected:
        EvaluatorRef terminating_evaluator;

    public:
        using Evaluator::Evaluator;

        virtual Evaluator *getNextEvaluator (const StringType &value) const override;
        virtual bool operator== (const Evaluator &other) const override;
        virtual void convertToOptionEvaluator() override;
    };

    /// Gather the rest of the command line tokens in a list.
    class RemainingValuesEvaluator : public RemainderEvaluator {
        friend ValueEvaluator;

    public:
        using RemainderEvaluator::RemainderEvaluator;

        virtual Parsnip::Data evaluateToken (ArgvCursor *cursor) const override;
        virtual void constructSchema (const DictionarySchema &scheme,
                                      const Schema::OptionSchemas &option_schemas,
                                      const SchemaIntegratorCallback &integrator) const override;
    };

    /// Evaluate remainder with a different parser.
    class OptionEvaluator : public RemainderEvaluator {
        friend class OptionParser;
        OptionParserRef option_parser;
        /// Evaluator to use next when iterating; points to parent to allow
        /// multiple option evaluators to be used in parallel.
        bool regurgitate {false}; ///< If true, back up one token before starting evaluation.
        bool iterate {false}; ///< If true, repeat parsing; if false, parse only once.

    protected:
        virtual Parsnip::Data evaluateToken (class ArgvCursor *cursor) const override;

    public:
        using RemainderEvaluator::RemainderEvaluator;
        virtual bool operator== (const Evaluator &) const override;

        static std::vector<OptionEvaluator *> construct (EvaluatorRef &evaluator,
                                                         std::string option_parser_type,
                                                         bool iterative,
                                                         const Parser *parent_parser);
        virtual void constructSchema (const DictionarySchema &scheme,
                                      const Schema::OptionSchemas &option_schemas,
                                      const SchemaIntegratorCallback &integrator) const override;
    };

}  // namespace Parsnip
