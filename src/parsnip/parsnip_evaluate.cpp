///
/// Evaluators for parsnip command-line parsing.
/// @file       parsnip_evaluate.cpp - Parsnip serialization & parsing
/// @author     Perette Barella
/// @date       2020-05-06
/// @copyright  Copyright 2012-2021 Devious Fish. All rights reserved.
/// history     This parser is based on the Football parser from 2012.
///

#include <string>

#include <cctype>

#include "parsnip.h"
#include "parsnip_argv.h"
#include "parsnip_command.h"
#include "parsnip_evaluate.h"

namespace Parsnip {

    /** Convert a terminator evaluator to an end-of-options evaluator.
        @param evaluator A pointer container for the existing evaluator. */
    static inline void options_evaluator_convert (EvaluatorRef &evaluator) {
        auto eval = evaluator.get();
        assert (eval);
        if (typeid (*eval) == typeid (TerminatorEvaluator)) {
            evaluator.reset (new EndOfOptionEvaluator (evaluator.get()));
        }
    }

    void EvaluatorDeleter::operator() (Evaluator *free_me) {
        delete (free_me);
    }

    /*
     *              Evaluator class implementation
     */

    /** Upgrade an evaluator.  The original *must* be a TerminatorEvaluator;
        other type should never be upgraded.
        Herein we initialize the new evaluator with the relevant bits. */
    Evaluator::Evaluator (Evaluator *original) {
        if (original) {
            parser_assert (dynamic_cast<TerminatorEvaluator *> (original), "Conflicting patterns");
            name = original->name;
            command_id = original->command_id;
        }
    }

    /** Assign a name by which to store values for value evaluators.
        There must be no prior name, or the same as the prior name.
        @param target The evalutor whose name to set.  If necessary,
        the target is instantiated.
        @param nam The name.  If empty the assignment is ignored. */
    void Evaluator::set_name (EvaluatorRef &target, const std::string &nam) {
        if (!target) {
            target.reset (new TerminatorEvaluator);
        }
        if (target->name.empty()) {
            target->name = nam;
        } else if (!nam.empty()) {
            parser_assert (target->name == nam, "Conflicting value names", nam + "/" + target->name);
        }
    }

    /** Set the command ID in an evaluator.  The ID must not be redefined/changed.
        @param id The ID being set. */
    void Evaluator::setCommandId (CommandId id) {
        parser_assert (command_id == NoCommand, "Pattern redefined", std::to_string (int (id)));
        command_id = id;
    }

    /** Check if two evaluators are the same.  Subclasses should override this to check their details.
        @param other Another evaluator to which to compare. */
    bool Evaluator::operator== (const Evaluator &other) const {
        return (typeid (*this) == typeid (other) && this->name == other.name && this->command_id == other.command_id);
    }

    /*
     *              Terminal Evalutors
     */

    /// An evaluator representing abrupt end of the command pattern.
    Evaluator *TerminatorEvaluator::getNextEvaluator (const StringType &value) const {
        throw RunOnCommand (value);
    }

    Parsnip::Data TerminatorEvaluator::evaluateToken (ArgvCursor *cursor) const {
        assert (!"Unreachable");
        throw std::runtime_error ("Evaluation of statement termination");
    }

    void TerminatorEvaluator::convertToOptionEvaluator() {
        assert (!"Unreachable");
    }

    void TerminatorEvaluator::constructSchema (const DictionarySchema &scheme,
                                               const Schema::OptionSchemas &,
                                               const SchemaIntegratorCallback &integrator) const {
        integrator (command_id, scheme);
    }

    /*
     *  Alternate terminator for options
     */

    /// An evaluator representing of one option phrase.
    Evaluator *EndOfOptionEvaluator::getNextEvaluator (const StringType &value) const {
        return nullptr;
    }

    Parsnip::Data EndOfOptionEvaluator::evaluateToken (ArgvCursor *cursor) const {
        assert (!"Unreachable");
        throw std::runtime_error ("Evaluation after end of option");
    }

    void EndOfOptionEvaluator::convertToOptionEvaluator() {
        // No-op.
    }

    void EndOfOptionEvaluator::constructSchema (const DictionarySchema &scheme,
                                                const Schema::OptionSchemas &,
                                                const SchemaIntegratorCallback &integrator) const {
        integrator (0, scheme);
    }

    /*
     *              Value Evaluators
     */

    Evaluator *ValueEvaluator::getNextEvaluator (const StringType &value) const {
        return next_evaluator.get();
    }

    bool ValueEvaluator::operator== (const Evaluator &other) const {
        if (this->Evaluator::operator== (other)) {
            auto far = static_cast<const ValueEvaluator *> (&other);
            return *next_evaluator == *(far->next_evaluator);
        }
        return false;
    }

    void ValueEvaluator::convertToOptionEvaluator() {
        if (next_evaluator) {
            options_evaluator_convert (next_evaluator);
            next_evaluator->convertToOptionEvaluator();
        }
    }

    void ValueEvaluator::constructSchema (const DictionarySchema &scheme,
                                          const Schema::OptionSchemas &option_schemas,
                                          const SchemaIntegratorCallback &integrator) const {
        assert (!next_evaluator->name.empty());
        DictionarySchema copy {scheme};
        copy.members[next_evaluator->name].member_schema.reset (getSchema ());
        next_evaluator->constructSchema (copy, option_schemas, integrator);
        
        if (command_id != NoCommand) {
            integrator (command_id, scheme);
        }
    }

    /// @return the current token as string data.
    Parsnip::Data StringEvaluator::evaluateToken (ArgvCursor *cursor) const {
        return Parsnip::Data ((*cursor)++.value());
    }

    SchemaBase *StringEvaluator::getSchema() const {
        return new StringSchema();
    }

    /// @return the current token as string data, after validating it against a regular expression.
    Parsnip::Data RegExEvaluator::evaluateToken (ArgvCursor *cursor) const {
        std::match_results<StringType::const_iterator> matches;
        StringType value = (*cursor)++.value();
        if (!regex_match (value, matches, regex)) {
            throw InvalidValue (value + ": does not match pattern.");
        }
        return Parsnip::Data (value);
    }

    bool RegExEvaluator::operator== (const Evaluator &other) const {
        if (StringEvaluator::operator==(other)) {
            const auto far = static_cast <const RegExEvaluator *> (&other);
            return (expression == far->expression && regex.flags() == far->regex.flags());
        }
        return false;
    }

    SchemaBase *RegExEvaluator::getSchema() const {
        return new RegExSchema (expression, (regex.flags() & std::regex_constants::icase) != 0);
    }
    
    /** Construct an evaluator (or validate the existing validator) for
        accepting an integer with a limited range. */
    RegExEvaluator *RegExEvaluator::construct (EvaluatorRef &parser,
                                               const std::string &name,
                                               const StringType &express,
                                               bool case_blind) {
        bool existed;
        RegExEvaluator *rep = uptype_construct<RegExEvaluator> (parser, &existed);
        Evaluator::set_name (rep->next_evaluator, name);

        parser_assert (!express.empty(), "Expression must not be empty string.");
        // Check first digit, skipping a leading +/-
        parser_assert (!existed || rep->expression == express, "Inconsistent regex");
        std::regex::flag_type flags = std::regex::flag_type (std::regex_constants::ECMAScript | (case_blind ? std::regex_constants::icase : 0));
        parser_assert (!existed || rep->regex.flags() == flags, "Inconsistent regex case sensitivity");
        
        if (!existed) {
            rep->expression = express;
            rep->regex = RegexType { express, flags };
        }
        return rep;
    }


    /// @return the original, untokenized, unmodified string.
    Parsnip::Data RawRemainderEvaluator::evaluateToken (ArgvCursor *cursor) const {
        Parsnip::Data result = cursor->remainingString();
        while (!cursor->isEnd()) {
            (*cursor)++;
        }
        return (result);
    }

    /// @return the current token as a long integer
    /// @throw If value is not in range.
    Parsnip::Data IntegerEvaluator::evaluateToken (ArgvCursor *cursor) const {
        const std::string &value = (*cursor)++.value();
        if (value.empty()) {
            throw NotNumeric ("(empty value)");
        }
        char *err;
        errno = 0;
        long int result = strtol (value.c_str(), &err, radix);
        if (*err != '\0' || errno != 0) {
            throw NotNumeric ('\'' + value + '\'');
        }
        if (result < minimum || result > maximum) {
            throw NumberOutOfRange (value + " (range: " + std::to_string (minimum) + "-" + std::to_string (maximum)
                                    + ")");
        }
        return Parsnip::Data (result);
    }

    SchemaBase *IntegerEvaluator::getSchema() const {
        return new RangeSchema<long> (minimum, maximum);
    }

    bool IntegerEvaluator::operator== (const Evaluator &other) const {
        if (this->ValueEvaluator::operator== (other)) {
            auto far = static_cast<const IntegerEvaluator *> (&other);
            return (minimum == far->minimum && maximum == far->maximum && radix == far->radix);
        }
        return false;
    }

    /** Construct an evaluator (or validate the existing validator) for
        accepting an integer with a limited range. */
    IntegerEvaluator *IntegerEvaluator::construct (EvaluatorRef &parser,
                                                   const std::string &name,
                                                   const std::string &min,
                                                   const std::string &max) {
        bool existed;
        IntegerEvaluator *ip = uptype_construct<IntegerEvaluator> (parser, &existed);
        Evaluator::set_name (ip->next_evaluator, name);

        parser_assert (!min.empty() && !max.empty(), "Min or max empty");
        // Check first digit, skipping a leading +/-
        bool min_adaptive = ((min[0] == '0' || min.substr (0, 2) == "-0") && min != "0");
        bool max_adaptive = ((max[0] == '0' || max.substr (0, 2) == "-0") && max != "0");
        int rad = ((min_adaptive || max_adaptive) ? 0 : 10);
        parser_assert (ip->radix == 1 || rad == ip->radix, "Inconsistent radix");
        ip->radix = rad;

        long prior = ip->minimum;
        ip->minimum = std::stol (min, nullptr, ip->radix);
        parser_assert (!existed || prior == ip->minimum, "Inconsistent minimum");

        prior = ip->maximum;
        ip->maximum = std::stol (max, nullptr, ip->radix);
        parser_assert (!existed || prior == ip->maximum, "Inconsistent maximum");
        parser_assert (ip->minimum < ip->maximum, "Minimum must be less than maximum");
        return ip;
    }

    bool RealEvaluator::operator== (const Evaluator &other) const {
        if (this->ValueEvaluator::operator== (other)) {
            auto far = static_cast<const RealEvaluator *> (&other);
            return (minimum == far->minimum && maximum == far->maximum);
        }
        return false;
    }

    /// Interpret the token as double, range check it, and store it.
    Parsnip::Data RealEvaluator::evaluateToken (ArgvCursor *cursor) const {
        const std::string &value = (*cursor)++.value();
        if (value.empty()) {
            throw NotNumeric ("(empty value)");
        }
        errno = 0;
        char *err;
        double result = strtod (value.c_str(), &err);
        if (*err != '\0' || errno != 0) {
            throw NotNumeric ('\'' + value + '\'');
        }
        if (result < minimum || result > maximum) {
            throw NumberOutOfRange (value + " (range: " + std::to_string (minimum) + "-" + std::to_string (maximum)
                                    + ")");
        }
        return Parsnip::Data (result);
    }

    SchemaBase *RealEvaluator::getSchema() const {
        return new RangeSchema<double> (minimum, maximum);
    }

    /** Construct an evaluator (or validate the existing validator) for
        accepting real number with a limited range. */
    RealEvaluator *RealEvaluator::construct (EvaluatorRef &parser,
                                             const std::string &name,
                                             const std::string &min,
                                             const std::string &max) {
        bool existed;
        RealEvaluator *rp = uptype_construct<RealEvaluator> (parser, &existed);
        Evaluator::set_name (rp->next_evaluator, name);

        double prior = rp->minimum;
        rp->minimum = std::stod (min);
        parser_assert (!existed || prior == rp->minimum, "Inconsistent minimum");

        prior = rp->maximum;
        rp->maximum = std::stod (max);
        parser_assert (!existed || prior == rp->maximum, "Inconsistent minimum");
        parser_assert (rp->minimum < rp->maximum, "Minimum must be less than maximum");
        return rp;
    }

    /*
     *              Keyword Evaluator
     */

    /// Store the current keyword as a string, if it's marked for recording.
    Parsnip::Data KeywordEvaluator::evaluateToken (ArgvCursor *cursor) const {
        if (numeric_evaluator && is_numeric (cursor->value())) {
            return numeric_evaluator->evaluateToken (cursor);
        }
        return (*cursor)++.value();
    }

    /// Select an evaluator for the next element based on current token.
    Evaluator *KeywordEvaluator::getNextEvaluator (const StringType &token) const {
        if (numeric_evaluator && is_numeric (token)) {
            return numeric_evaluator->getNextEvaluator (token);
        }
        auto it = tokens.find (tolower (token));
        if (it == tokens.end()) {
            if (command_id == EndOfOption) {
                return nullptr;
            }
            throw InvalidValue (token);  // Invalid command
        }
        return it->second.get();
    }

    void KeywordEvaluator::constructSchema (const DictionarySchema &scheme,
                                            const Schema::OptionSchemas &option_schemas,
                                            const SchemaIntegratorCallback &integrator) const {
        if (numeric_evaluator) {
            DictionarySchema copy {scheme};
            copy.members[numeric_evaluator->name].member_schema.reset (static_cast <ValueEvaluator *> (numeric_evaluator.get())->getSchema());
            numeric_evaluator->getNextEvaluator(EmptyString)->constructSchema (scheme, option_schemas, integrator);
        }
        for (const auto &token : tokens) {
            if (token.second->name.empty()) {
                token.second->constructSchema (scheme, option_schemas, integrator);
            } else {
                DictionarySchema copy {scheme};
                copy.members[token.second->name].member_schema.reset (new KeywordSchema (token.first));
                token.second->constructSchema (copy, option_schemas, integrator);
            }
        }
        
        if (command_id != NoCommand) {
            integrator (command_id, scheme);
        }
    }

    bool KeywordEvaluator::operator== (const Evaluator &other) const {
        if (!this->Evaluator::operator== (other)) {
            return false;
        }
        auto far = static_cast<const KeywordEvaluator *> (&other);
        if (tokens.size() == far->tokens.size() && numbers_present == far->numbers_present
            && (numeric_evaluator == far->numeric_evaluator || *numeric_evaluator == *(far->numeric_evaluator))) {
            for (auto &token : tokens) {
                auto it = far->tokens.find (token.first);
                if (it == far->tokens.end()) {
                    return false;
                }
                if (*(token.second) != *(it->second.get())) {
                    return false;
                }
            }
            return true;
        }
        return false;
    };

    void KeywordEvaluator::convertToOptionEvaluator() {
        if (numeric_evaluator) {
            numeric_evaluator->convertToOptionEvaluator();
        }
        for (auto &token : tokens) {
            options_evaluator_convert (token.second);
            token.second->convertToOptionEvaluator();
        }
    }

    /*
     *              Remainder Evaluators
     */

    /// An evaluator that represents the rest of the command line.
    Evaluator *RemainderEvaluator::getNextEvaluator (const StringType &value) const {
        return terminating_evaluator.get();
    }

    bool RemainderEvaluator::operator== (const Evaluator &other) const {
        if (this->Evaluator::operator== (other)) {
            auto far = static_cast<const RemainderEvaluator *> (&other);
            return *terminating_evaluator == *(far->terminating_evaluator);
        }
        return false;
    }

    void RemainderEvaluator::convertToOptionEvaluator() {
        options_evaluator_convert (terminating_evaluator);
        terminating_evaluator->convertToOptionEvaluator();
    }

    /// @return any remaining tokens in a list
    Parsnip::Data RemainingValuesEvaluator::evaluateToken (ArgvCursor *cursor) const {
        Parsnip::Data list{Parsnip::Data::List};
        while (!cursor->isEnd()) {
            list.push_back (terminating_evaluator->evaluateToken (cursor));
        }
        return list;
    }

    void RemainingValuesEvaluator::constructSchema (const DictionarySchema &scheme,
                                                    const Schema::OptionSchemas &option_schemas,
                                                    const SchemaIntegratorCallback &integrator) const {
        assert (!terminating_evaluator->name.empty());
        const ValueEvaluator *evaluator = dynamic_cast<const ValueEvaluator *> (terminating_evaluator.get());
        assert (evaluator);
        DictionarySchema copy {scheme};
        copy.members[terminating_evaluator->name].member_schema.reset (new ListSchema (SchemaBaseRef (evaluator->getSchema()),
                                                                       command_id == NoCommand ? 1 : 0));
        integrator (terminating_evaluator->command_id, copy);

        if (command_id != NoCommand) {
            integrator (command_id, scheme);
        }
    }

    Parsnip::Data OptionEvaluator::evaluateToken (ArgvCursor *cursor) const {
        Parsnip::Data results{Parsnip::Data::Dictionary};
        if (regurgitate) {
            assert (!cursor->isStart());
            *cursor = *cursor - 1;
        }
        if (iterate) {
            while (!cursor->isEnd()) {
                option_parser->evaluator->evaluate (cursor, results);
            }
        } else {
            option_parser->evaluator->evaluate (cursor, results);
        }
        return results;
    }

    void OptionEvaluator::constructSchema (const DictionarySchema &scheme,
                                           const Schema::OptionSchemas &option_schemas,
                                           const SchemaIntegratorCallback &integrator) const {
        assert (!terminating_evaluator->name.empty());
        auto it = option_schemas.find (option_parser.get());
        if (it == option_schemas.end()) {
            throw NoSuchKey ("No schema for option parser");
        }
        DictionarySchema copy {scheme};
        copy.members[terminating_evaluator->name].member_schema.reset (new OptionSchema (it->second));
        terminating_evaluator->constructSchema (copy, option_schemas, integrator);

        if (command_id != NoCommand) {
            integrator (command_id, scheme);
        }
    }

    bool OptionEvaluator::operator== (const Evaluator &other) const {
        if (RemainderEvaluator::operator== (other)) {
            auto far = static_cast<const OptionEvaluator *> (&other);
            return (*option_parser == *(far->option_parser) && iterate == far->iterate
                    && regurgitate == far->regurgitate);
        }
        return false;
    }

    /** Construct an evaluator (or validate the existing validator)
        that parsers the remainder with an option parser.
        @param evaluator The evaluator being constructed.
        @param option_parser_type The name of the option parser to use.
        @param iterative If true, option parser should iterate.
        @param parent_parser The containing parser, with previously-defined option parsers to utilize. */
    std::vector<OptionEvaluator *> OptionEvaluator::construct (EvaluatorRef &evaluator,
                                                               std::string option_parser_type,
                                                               bool iterative,
                                                               const Parser *parent_parser) {
        assert (!option_parser_type.empty());
        bool use_safety = (option_parser_type[0] == '<');
        if (use_safety) {
            assert (option_parser_type[option_parser_type.size() - 1] == '>');
            option_parser_type.erase (0, 1);
            option_parser_type.erase (option_parser_type.size() - 1, 1);
        }
        auto option_parser = parent_parser->option_parsers.at (option_parser_type);

        KeywordEvaluator *keyparser = dynamic_cast<KeywordEvaluator *> (evaluator.get());
        ValueEvaluator *value_fillin = dynamic_cast<ValueEvaluator *> (evaluator.get());

        if (use_safety || keyparser || value_fillin) {
            // Ensure there's a keyword evaluator before the option evaluator,
            // to allow other choices in parallel.
            if (value_fillin) {
                // If existing evaluator was a numeric input, make it our numeric value handler
                parser_assert (!dynamic_cast<StringEvaluator *> (value_fillin),
                               "String fill-in cannot parallel keywords");
                keyparser = new KeywordEvaluator();
                keyparser->numeric_evaluator = std::move (evaluator);
                evaluator.reset (keyparser);
            } else {
                keyparser = uptype_construct<KeywordEvaluator> (evaluator);
            }
            auto options = dynamic_cast<KeywordEvaluator *> (option_parser->evaluator.get());
            parser_assert (options, "Options should start with keywords");
            std::vector<OptionEvaluator *> evaluators;
            for (const auto &option : options->tokens) {
                std::string keyword = option.first;
                keyparser->numbers_present = keyparser->numbers_present || KeywordEvaluator::is_numeric (keyword);
                if (keyparser->numeric_evaluator) {
                    parser_assert (!keyparser->numbers_present, "Numeric keywords cannot parallel numeric fill-in");
                }
                EvaluatorRef &option_eval = keyparser->tokens[keyword];
                auto oe = uptype_construct<OptionEvaluator> (option_eval);
                if (oe->option_parser) {
                    parser_assert (
                            oe->option_parser == option_parser && oe->iterate == iterative && oe->regurgitate == true,
                            "Option parser mismatch/overlap");
                } else {
                    oe->option_parser = option_parser;
                    oe->regurgitate = true;
                    oe->iterate = iterative;
                }
                evaluators.push_back (oe);
            }
            return evaluators;
        }
        auto oe = uptype_construct<OptionEvaluator> (evaluator);
        if (oe->option_parser) {
            parser_assert (oe->option_parser == option_parser && oe->iterate == iterative && oe->regurgitate == false,
                           "Option parser mismatch/overlap");
        } else {
            oe->option_parser = option_parser;
            oe->iterate = iterative;
        }
        return std::vector<OptionEvaluator *>{oe};
    }
}  // namespace Parsnip
