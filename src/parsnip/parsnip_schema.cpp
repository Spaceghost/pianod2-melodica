///
/// Parsnip serialization schemas.
/// Structures for describing the structure of data.
/// @file       parsnip_schema.h - Parsnip serialization & parsing
/// @author     Perette Barella
/// @date       2020-12-17
/// @copyright  Copyright 2020-2021 Devious Fish. All rights reserved.
///

#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <type_traits>
#include <cctype>
#include <cstring>
#include <algorithm>

#include "parsnip.h"
#include "parsnip_command.h"
#include "parsnip_schema.h"
#include "parsnip_evaluate.h"
#include "parsnip_helpers.h"

namespace Parsnip {

    /// Copy construction for the smart pointer for SchemaBase.
    SchemaBaseRef::SchemaBaseRef (const SchemaBaseRef &from) {
        this->reset (from ? from->createCopy() : nullptr);
    }

    /// Copy assignment for the smart pointer for SchemaBase.
    SchemaBaseRef &SchemaBaseRef::operator= (const SchemaBaseRef &from) {
        if (this != &from) {
            this->reset (from ? from->createCopy() : nullptr);
        }
        return *this;
    }

    /** Construct as schema conflict exception, and log details
        of the conflict to stderr.
        @param reason Details explaining the conflict.
        @param from The schema being merged from.
        @param into The schema being merged into. */
    SchemaConflict::SchemaConflict (const std::string &reason,
                                    const class SchemaBase &from,
                                    const class SchemaBase &into)
    : Exception ("schema conflict", reason) {
#ifndef NDEBUG
        std::cerr << "Failure merging: " << reason << ":\n  From: ";
        from.dump (std::cerr, 8, true);
        std::cerr << "  Into: ";
        into.dump (std::cerr, 8, true);
        std::cerr << '\n';
#else
        (void) from;
        (void) into;
#endif
    };

    static SchemaBase *construct_schema (const Parsnip::Data &schema_spec);

    void SchemaBaseDeleter::operator() (SchemaBase *free_me) {
        delete free_me;
    }

    /*
     *  Schema Base
     */

    /** Construct a schema from a JSON specification.
        @param schema_spec The schema specification. */
    SchemaBase::SchemaBase (const Parsnip::Data &schema_spec) : nullable (schema_spec.getOr ("nullable", false)) {
    }

    /** Compare two schemas to see if they determine validity in the same manner. */
    bool SchemaBase::operator== (const SchemaBase &other) const {
        return (typeid (*this) == typeid (other) && (nullable == other.nullable));
    }

    /** Merge elements managed by this class. */
    void SchemaBase::mergeSchemas (const SchemaBase &from) {
        if (typeid (*this) != typeid (from)) {
            throw SchemaConflict ("Incompatible schema elements", from, *this);
        }
        nullable = nullable || from.nullable;
    }

    /*
     *  UncheckedSchema
     */

    /// Construct a schema element that does no checking.
    void UncheckedSchema::validate (const Parsnip::Data &entry) const {
        // Do nothing.
    }

    UncheckedSchema *UncheckedSchema::createCopy() const {
        return new UncheckedSchema (*this);
    }

    std::ostream &UncheckedSchema::dump (std::ostream &target, int indent, bool suppress_indent) const {
        do_indent (target, suppress_indent ? 0 : indent);
        target << "Unchecked data\n";
        return target;
    }

    /*
     *  TypeSchema
     */

    /// Construct a type schema, specifying required type.
    TypeSchema::TypeSchema (const Data::Type require) : expected (require) {
    }

    /** Construct a type schema.
        @param require The required type.
        @param schema_spec The schema specification. */
    TypeSchema::TypeSchema (const Data::Type require, const Parsnip::Data &schema_spec)
    : SchemaBase (schema_spec), expected (require) {
    }

    void TypeSchema::validate (const Parsnip::Data &entry) const {
        checkType (expected, entry);
    }

    TypeSchema *TypeSchema::createCopy() const {
        return new TypeSchema (*this);
    }

    bool TypeSchema::operator== (const SchemaBase &other) const {
        const TypeSchema *against = static_cast<const TypeSchema *> (&other);
        return SchemaBase::operator== (other) && (expected == against->expected);
    }

    void TypeSchema::mergeSchemas (const SchemaBase &from) {
        SchemaBase::mergeSchemas (from);
        if (expected != static_cast<const TypeSchema *> (&from)->expected) {
            throw SchemaConflict ("Conflicting types", from, *this);
        }
    }

    std::ostream &TypeSchema::dump (std::ostream &target, int indent, bool suppress_indent) const {
        do_indent (target, suppress_indent ? 0 : indent);
        target << "Type=" << Data::type_name (expected) << (nullable ? ", nullable\n" : "\n");
        return target;
    }

    /*
     *  StringSchema
     */

    /** Construct a string schema.
        @param min Minimum length of valid strings.
        @param max Maximum length of valid strings. */
    StringSchema::StringSchema (size_type min, size_type max) : min_length (min), max_length (max) {
    }

    /// Construct a string validator from a schema specification.
    StringSchema::StringSchema (const Parsnip::Data &schema_spec)
    : SchemaBase (schema_spec),
      min_length (schema_spec.getOr ("minLength", 0)),
      max_length (schema_spec.getOr ("maxLength", std::numeric_limits<size_type>::max())) {
    }

    StringSchema *StringSchema::createCopy() const {
        return new StringSchema (*this);
    }

    void StringSchema::validate (const Parsnip::Data &entry) const {
        if (isValidlyNull (entry)) {
            return;
        }
        const std::string &value = entry.asString();
        if (value.size() < min_length || value.size() > max_length) {
            throw InvalidValue ("Wrong length (expected " + std::to_string (min_length) + "-"
                                + std::to_string (max_length) + " characters)");
        }
        for (char ch : value) {
            if (iscntrl (ch)) {
                throw InvalidValue ("Control characters in string");
            }
        }
    }

    bool StringSchema::operator== (const SchemaBase &other) const {
        const StringSchema *against = static_cast<const StringSchema *> (&other);
        return (SchemaBase::operator== (other) && (min_length == against->min_length)
                && (max_length == against->max_length));
    }

    std::ostream &StringSchema::dump (std::ostream &target, int indent, bool suppress_indent) const {
        do_indent (target, suppress_indent ? 0 : indent);
        target << "String (no control characters allowed)" << (nullable ? ", nullable\n" : "\n");
        return target;
    }

    /*
     *  RegExSchema
     */

    /** Construct a regular expression validator.
        @param express The regular expression.
        @param case_blind If true, the expression is applied case insensitive.
        @param min Minimum length of valid strings.
        @param max Maximum length of valid strings. */
    RegExSchema::RegExSchema (const StringType &express, bool case_blind, size_type min, size_type max)
    : StringSchema (min, max),
      expression (express),
      regex (expression,
             std::regex::flag_type (std::regex_constants::ECMAScript
                                    | (case_blind ? std::regex_constants::icase : 0))) {
    }

    /// Construct a string regular expression validator from a schema spec.
    RegExSchema::RegExSchema (const Parsnip::Data &schema_spec)
    : StringSchema (schema_spec),
      expression (schema_spec ["pattern"].asString()),
      regex (expression,
             std::regex::flag_type (std::regex_constants::ECMAScript
                                    | (schema_spec.getOr ("ignoreCase", false) ? std::regex_constants::icase : 0))) {
    }

    RegExSchema *RegExSchema::createCopy() const {
        return new RegExSchema (*this);
    }

    void RegExSchema::validate (const Parsnip::Data &entry) const {
        if (isValidlyNull (entry)) {
            return;
        }
        const StringType &value = entry.asString();
        if (value.size() < min_length || value.size() > max_length) {
            throw InvalidValue ("Wrong length (expected " + std::to_string (min_length) + "-"
                                + std::to_string (max_length) + " characters)");
        }
        std::match_results<StringType::const_iterator> matches;
        if (!regex_match (value, matches, regex)) {
            throw InvalidValue (value + ": does not match pattern.");
        }
    }

    bool RegExSchema::operator== (const SchemaBase &other) const {
        const RegExSchema *against = static_cast<const RegExSchema *> (&other);
        return (StringSchema::operator== (other) && (expression == against->expression)
                && regex.flags() == against->regex.flags());
    }

    std::ostream &RegExSchema::dump (std::ostream &target, int indent, bool suppress_indent) const {
        do_indent (target, suppress_indent ? 0 : indent);
        target << "String (" + expression + ")" << (nullable ? ", nullable\n" : "\n");
        return target;
    }

    /*
     *  Keyword Schema
     */

    /// Construct a keyword schema, starting with one initial keyword.
    KeywordSchema::KeywordSchema (const std::string &word) : valid_values{ { word } } {
    }

    KeywordSchema *KeywordSchema::createCopy() const {
        return new KeywordSchema (*this);
    }

    /** Add an additional keyword that the schema will allow.
        @param word The keyword to allow. */
    void KeywordSchema::addKeyword (const std::string &word) {
        valid_values.insert (word);
    }

    /// Construct a keyword (string enumeration) schema from a specification.
    KeywordSchema::KeywordSchema (const Parsnip::Data &schema_spec)
    : SchemaBase (schema_spec), ignore_case (schema_spec.getOr ("ignoreCase", false)) {
        if (schema_spec.contains ("enum")) {
            if (schema_spec.contains ("const")) {
                throw DataFormatError ("'const' and 'enum' cannot be specified together.");
            }
            for (const auto &word : schema_spec ["enum"].as<std::vector<std::string>>()) {
                valid_values.insert (word);
            }
        } else {
            valid_values.insert (schema_spec ["const"].asString());
        }
    }

    bool KeywordSchema::operator== (const SchemaBase &other) const {
        const KeywordSchema *against = static_cast<const KeywordSchema *> (&other);
        return SchemaBase::operator== (other) && (valid_values == against->valid_values);
    }

    /** Validate a Keyword: ensure the datatype is a string, and that
        it matches one of the allowed values. */
    void KeywordSchema::validate (const Parsnip::Data &entry) const {
        if (isValidlyNull (entry)) {
            return;
        }
        const StringType &data = entry.asString();
        for (const auto &value : valid_values) {
            if ((!ignore_case && data == value) || (ignore_case && strcasecmp (data.c_str(), value.c_str()) == 0)) {
                return;
            }
        }
        throw InvalidValue (data);
    }

    void KeywordSchema::mergeSchemas (const SchemaBase &from) {
        SchemaBase::mergeSchemas (from);
        const KeywordSchema *from_ks = static_cast<const KeywordSchema *> (&from);
        for (const std::string &word : from_ks->valid_values) {
            valid_values.insert (word);
        }
    }

    std::ostream &KeywordSchema::dump (std::ostream &target, int indent, bool suppress_indent) const {
        do_indent (target, suppress_indent ? 0 : indent);
        target << "String " << (nullable ? "(nullable) " : EmptyString) << "with possible values:\n";
        do_indent (target, indent + 1);
        for (const auto &value : valid_values) {
            target << ' ' << value;
        }
        target << '\n';
        return target;
    }

    /*
     *  Range Schema
     */

    /** Construct a numeric schema from a schema definition.
        @tparam NumericType Either double for `number` or long for `integer`.
        @param min The minimum acceptable value.
        @param max The maximum allowed value. */
    template <typename NumericType>
    RangeSchema<NumericType>::RangeSchema (const NumericType min, const NumericType max)
    : minimum (min), maximum (max) {
    }

    template <typename NumericType>
    RangeSchema<NumericType> *RangeSchema<NumericType>::createCopy() const {
        return new RangeSchema<NumericType> (*this);
    }

    /** Construct a numeric schema from a schema definition.
        @tparam NumericType Either double for `number` or long for `integer`.
        @param schema_spec The schema specification. */
    template <typename NumericType>
    RangeSchema<NumericType>::RangeSchema (const Parsnip::Data &schema_spec)
    : SchemaBase (schema_spec),
      minimumIsInclusive (!schema_spec.contains ("exclusiveMinimum")),
      minimum (schema_spec.getOr (minimumIsInclusive ? "minimum" : "exclusiveMinimum",
                                  std::numeric_limits<value_type>::lowest())),
      maximumIsInclusive (!schema_spec.contains ("exclusiveMaximum")),
      maximum (schema_spec.getOr (maximumIsInclusive ? "maximum" : "exclusiveMaximum",
                                  std::numeric_limits<value_type>::max())) {
        if (schema_spec.contains ("minimum") && schema_spec.contains ("exclusiveMinimum")) {
            throw DataFormatError ("Both minimum and exclusiveMinimum specified.");
        }
        if (schema_spec.contains ("maximum") && schema_spec.contains ("exclusiveMaximum")) {
            throw DataFormatError ("Both maximum and exclusiveMaximum specified.");
        }
    }

    /// Validate item range: ensure the datatype falls within the allowed range.
    template <typename NumericType>
    void RangeSchema<NumericType>::validate (const Parsnip::Data &entry) const {
        if (isValidlyNull (entry)) {
            return;
        }
        NumericType value = entry.as<NumericType>();
        if (value < minimum || (!minimumIsInclusive && value == minimum) || value > maximum
            || (!maximumIsInclusive && value == maximum)) {
            std::string range{ "-" };
            if (!minimumIsInclusive || !maximumIsInclusive) {
                // Need to express range mathematically for it to be accurate.
                range = std::string (minimumIsInclusive ? " <= value " : " < value ")
                        + (maximumIsInclusive ? "<= " : "< ");
            }
            throw NumberOutOfRange (std::to_string (value) + " (range: " + std::to_string (minimum) + range
                                    + std::to_string (maximum) + ")");
        }
    }

    template <typename NumericType>
    bool RangeSchema<NumericType>::operator== (const SchemaBase &other) const {
        const RangeSchema<NumericType> *against = static_cast<const RangeSchema *> (&other);
        return SchemaBase::operator== (other) && (minimum == against->minimum)
               && (minimumIsInclusive == against->minimumIsInclusive) && (maximum == against->maximum)
               && (maximumIsInclusive == against->maximumIsInclusive);
    }

    template <typename NumericType>
    void RangeSchema<NumericType>::mergeSchemas (const SchemaBase &from) {
        SchemaBase::mergeSchemas (from);
        auto *from_rs = static_cast<const RangeSchema<NumericType> *> (&from);
        if (minimum != from_rs->minimum || maximum != from_rs->maximum) {
            throw SchemaConflict ("Conflicting ranges", from, *this);
        }
    }

    template <typename NumericType>
    std::ostream &RangeSchema<NumericType>::dump (std::ostream &target, int indent, bool suppress_indent) const {
        if (!suppress_indent) {
            for (int i = indent; i > 0; i--) {
                target << ' ';
            }
        }
        NumericType test = (7.0 / 2.0);
        target << (test == 3 ? "Integer " : "Real number ") << (nullable ? "(nullable) " : EmptyString);
        if (minimum == std::numeric_limits<NumericType>::lowest()
            && maximum == std::numeric_limits<NumericType>::max()) {
            // essentially unlimited range";
        } else if (maximum == std::numeric_limits<NumericType>::max()) {
            target << "with value >= " << minimum;
        } else {
            target << "within range " << minimum << '-' << maximum;
        }
        target << std::endl;
        return target;
    }

    // Force instantiation of the two forms we need.
    template class RangeSchema<double>;
    template class RangeSchema<long>;

    /// Construct an integer schema from a schema specification.
    IntegerSchema::IntegerSchema (const Parsnip::Data &schema_spec)
    : range_type (schema_spec), multiple (schema_spec.getOr ("multiple", 0)) {
    }

    IntegerSchema *IntegerSchema::createCopy() const {
        return new IntegerSchema (*this);
    }

    bool IntegerSchema::operator== (const SchemaBase &other) const {
        const IntegerSchema *against = static_cast<const IntegerSchema *> (&other);
        return range_type::operator== (other) && (multiple == against->multiple);
    }

    void IntegerSchema::validate (const Parsnip::Data &data) const {
        if (isValidlyNull (data)) {
            return;
        }
        range_type::validate (data);
        if (multiple != 0) {
            value_type value = data.as<value_type>();
            if ((value % multiple) != 0) {
                throw NumberOutOfRange (std::to_string (value) + " (require a multiple of " + std::to_string (multiple)
                                        + ")");
            }
        }
    }

    /*
     *  Option Schema
     */

    /// Construct a new option schema.
    OptionSchema::OptionSchema (const SchemaRef &option_schema) : schema (option_schema) {
        assert (option_schema);
    }

    OptionSchema *OptionSchema::createCopy() const {
        return new OptionSchema (*this);
    }

    bool OptionSchema::operator== (const SchemaBase &other) const {
        const OptionSchema *against = static_cast<const OptionSchema *> (&other);
        return SchemaBase::operator== (other) && (schema == against->schema);
    }

    /// Validate the option's schema by calling that scheme's validator.
    void OptionSchema::validate (const Parsnip::Data &entry) const {
        assert (schema);
        schema->validate (entry);
    }

    void OptionSchema::mergeSchemas (const SchemaBase &from) {
        SchemaBase::mergeSchemas (from);
        const OptionSchema *from_os = static_cast<const OptionSchema *> (&from);
        if (*this != *from_os) {
            // Make sure we have a unique instance we're about to modify
            if (schema.use_count() > 1) {
                schema.reset (new Schema (schema->schema->createCopy()));
            }
            // Merge the option schemas
            schema->schema->mergeSchemas (*(from_os->schema->schema));
        }
    }

    std::ostream &OptionSchema::dump (std::ostream &target, int indent, bool suppress_indent) const {
        return schema->dump (target, indent, suppress_indent);
    }

    /*
     *  List Schema
     */

    /** Construct a list schema (move variant).
        @param from A schema to validate the list members.
        @param minimum The number of list members mandated.
        @param maximum The number of list members allowed. */
    ListSchema::ListSchema (SchemaBaseRef &&from, size_type minimum, size_type maximum)
    : member_schema (std::move (from)), minimum_required (minimum), maximum_allowed (maximum) {
    }

    /** Construct a list schema (cppy variant).
        @param member_schema A schema to validate the list members.
        @param minimum The number of list members mandated.
        @param maximum The number of list members allowed. */
    ListSchema::ListSchema (const SchemaBase &member_schema, size_type minimum, size_type maximum)
    : member_schema (member_schema.createCopy()), minimum_required (minimum), maximum_allowed (maximum) {
    }

    /// Construct a list schema from a schema specification.
    ListSchema::ListSchema (const Parsnip::Data &schema_spec)
    : SchemaBase (schema_spec),
      minimum_required (schema_spec.getOr ("minItems", 0)),
      maximum_allowed (schema_spec.getOr ("maxItems", std::numeric_limits<size_type>::max())) {
        if (schema_spec.contains ("uniqueItems") && schema_spec ["uniqueItems"].asBoolean()) {
            throw std::runtime_error ("Schema list->uniqueItems is unimplemented.");
        }
        if (schema_spec.contains ("contains")) {
            throw std::runtime_error ("Schema list->contains not implemented");
        }
        try {
            schema_spec.size();  // Check to see if it's a list.
            throw std::runtime_error ("Schema list-tuples not implemented");
        } catch (IncorrectDataType &ex) {
            /// Do nothing.
        }
        try {
            member_schema.reset (construct_schema (schema_spec ["items"]));
        } catch (Exception &ex) {
            ex.addBacktraceLocation ("[list]");
            throw;
        }
    }

    ListSchema *ListSchema::createCopy() const {
        return new ListSchema (*this);
    }

    bool ListSchema::operator== (const SchemaBase &other) const {
        assert (member_schema);
        const ListSchema *against = static_cast<const ListSchema *> (&other);
        assert (against->member_schema);
        return (SchemaBase::operator== (other) && (single_as_nonlist_allowed == against->single_as_nonlist_allowed)
                && (minimum_required == against->minimum_required) && (maximum_allowed == against->maximum_allowed)
                && (*member_schema == *(against->member_schema)));
    }

    /** Validate a list: ensure all elements conform to a single schema. */
    void ListSchema::validate (const Parsnip::Data &entry) const {
        assert (member_schema);
        if (isValidlyNull (entry)) {
            return;
        }
        if (single_as_nonlist_allowed && isType (Data::Type::List, entry)) {
            member_schema->validate (entry);
            return;
        }
        if (entry.size() < minimum_required || entry.size() > maximum_allowed) {
            throw IncorrectListSize (entry.size(), minimum_required, maximum_allowed);
        }
        for (Parsnip::Data::size_type i = 0; i < entry.size(); i++) {
            try {
                member_schema->validate (entry [i]);
            } catch (Exception &ex) {
                ex.addBacktraceLocation ("[" + std::to_string (i) + "]");
                throw;
            }
        }
    }

    void ListSchema::mergeSchemas (const SchemaBase &from) {
        nullable = nullable || from.nullable;
        const ListSchema *from_list = dynamic_cast<const ListSchema *> (&from);
        if (from_list) {
            member_schema->mergeSchemas (*(static_cast<const ListSchema *> (&from)->member_schema));
        } else {
            single_as_nonlist_allowed = true;
            // Merge the single value into our list values schema.
            member_schema->mergeSchemas (from);
        }
    }

    std::ostream &ListSchema::dump (std::ostream &target, int indent, bool suppress_indent) const {
        do_indent (target, suppress_indent ? 0 : indent);
        target << "List (" << minimum_required;
        if (maximum_allowed == std::numeric_limits<size_type>::max()) {
            target << " or more";
        } else {
            target << "-" << maximum_allowed;
        }
        target << " members) " << (nullable ? "(nullable) " : EmptyString) << "of ";
        member_schema->dump (target, indent + 2, true);
        return target;
    }

    /*
     *  Dictionary Schema
     */

    /// Determine if two dictionary members are equivalent to each other, deep comparing the schema.
    bool DictionarySchema::DictionaryMember::operator== (const DictionaryMember &other) const {
        assert (member_schema);
        assert (other.member_schema);
        return ((mandatory == other.mandatory) && (expects == other.expects)
                && (*member_schema == *(other.member_schema)));
    }

    /** Construct a dictionary (object) schema from a schema specificiation.
        @param schema_spec The specification. */
    DictionarySchema::DictionarySchema (const Parsnip::Data &schema_spec)
    : SchemaBase (schema_spec),
      minimum_members (schema_spec.getOr ("minProperties", 0)),
      maximum_members (schema_spec.getOr ("maxProperties", std::numeric_limits<size_type>::max())),
      tolerance (AdditionalMemberNames::ANY) {
        if (schema_spec.contains ("properties")) {
            std::function<void (const std::string &, const Parsnip::Data &)> construct_member{
                [this] (const std::string &property_name, const Parsnip::Data &member_schema_spec) -> void {
                    try {
                        this->members [property_name].member_schema.reset (construct_schema (member_schema_spec));
                    } catch (Parsnip::Exception &ex) {
                        ex.addBacktraceLocation (property_name);
                        throw;
                    }
                }
            };
            schema_spec ["properties"].foreach (construct_member);
        }
        if (schema_spec.contains ("required")) {
            for (const auto &name : schema_spec ["required"].as<std::vector<std::string>>()) {
                auto it = members.find (name);
                if (it == members.end()) {
                    throw InvalidSchema ("Required property '" + name + "' is not defined.");
                }
                members.at (name).mandatory = true;
            }
        }
        if (schema_spec.contains ("dependencies")) {
            std::function<void (const std::string &, const Parsnip::Data &)> insert_dependencies{
                [this] (const std::string &key, const Parsnip::Data &value) -> void {
                    auto target = this->members.find (key);
                    if (target == this->members.end()) {
                        throw InvalidSchema ("Dependencies declared on non-existent property '" + key + ".");
                    }
                    for (const auto &name : value.as<std::vector<std::string>>()) {
                        if (this->members.find (name) == this->members.end()) {
                            throw InvalidSchema ("Property '" + key + "' depends on '" + name
                                                 + "', which doesn't exist.");
                        }
                        target->second.expects.insert (name);
                    }
                }
            };
            schema_spec ["dependencies"].foreach (insert_dependencies);
        }
        // Handle additionalProperties being true or false.
        try {
            tolerance = schema_spec.getOr ("additionalProperties", true) ? AdditionalMemberNames::ANY
                                                                         : AdditionalMemberNames::NONE;
            if (tolerance == AdditionalMemberNames::ANY) {
                additional_member_schema.reset (new UncheckedSchema());
            }
        } catch (IncorrectDataType &) {
            // Handle additionalProperties specifying a type.
            try {
                additional_member_schema.reset (construct_schema (schema_spec ["additionalProperties"]));
            } catch (Exception &ex) {
                ex.addBacktraceLocation ("(additionalProperties)");
            }
            // Seems like "propertyNames" should be in additionalProperties, but it's not.
            tolerance = schema_spec.contains ("propertyNames") ? AdditionalMemberNames::PATTERN
                                                               : AdditionalMemberNames::ANY;
            if (tolerance == AdditionalMemberNames::PATTERN) {
                const Parsnip::Data &prop_names = schema_spec ["propertyNames"];
                additional_member_regex = SchemaRegex (
                        prop_names.getOr ("pattern", ".*"),
                        std::regex::flag_type (
                                std::regex_constants::ECMAScript
                                | (prop_names.getOr ("ignoreCase", false) ? std::regex_constants::icase : 0)));
            }
        }
    }

    /** Add a member to a dictionary schema.
        @param name The name of the member to add.
        @param schema The schema for the new member.
        @param mandatory If true, the member must be present for
        the dictionary to be valid.
        @param dependencies Any members that must be present when
        the new member is present. */
    void DictionarySchema::addMember (const char *name,
                                      const SchemaBase &schema,
                                      bool mandatory,
                                      const Dependencies &dependencies) {
        DictionaryMember &member = members [name];
        member.member_schema.reset (schema.createCopy());
        member.mandatory = mandatory;
        member.expects = dependencies;
    }

    /** Replace a member in the schema, retaining original mandatory status
        and dependencies.
        @param name The member to revise.
        @param schema The replacement schema for the member. */
    void DictionarySchema::replaceMember (const char *name, const SchemaBase &schema) {
        members [name].member_schema.reset (schema.createCopy());
    }

    /** Remove a member from the schema.
        @param name The member to remove. */
    void DictionarySchema::removeMember (const char *name) {
        auto it = members.find (name);
        if (it != members.end()) {
            members.erase (it);
        }
    }

    std::ostream &DictionarySchema::dump (std::ostream &target, int indent, bool suppress_indent) const {
        do_indent (target, suppress_indent ? 0 : indent);
        target << "Dictionary" << (nullable ? " (nullable)" : EmptyString)
               << (tolerance == AdditionalMemberNames::NONE ? "" : ", extra properties ok,")
               << (members.empty() ? " (empty).\n" : " containing:\n");
        for (const auto &member : members) {
            do_indent (target, indent + 2);
            target << '\'' << member.first << '\'' << (member.second.mandatory ? " (mandatory)\n" : " (optional)\n");
            if (!member.second.expects.empty()) {
                do_indent (target, indent + 4);
                target << "Requires also:";
                for (const auto &value : member.second.expects) {
                    target << ' ' << value;
                }
                target << '\n';
            }
            member.second.member_schema->dump (target, indent + 4);
        }
        if (additional_member_schema) {
            do_indent (target, indent + 2);
            target << "Additional properties schema:\n";
            additional_member_schema->dump (target, indent + 4);
        }
        return target;
    }

    DictionarySchema *DictionarySchema::createCopy() const {
        return new DictionarySchema (*this);
    }

    bool DictionarySchema::operator== (const SchemaBase &other) const {
        const DictionarySchema *against = static_cast<const DictionarySchema *> (&other);
        return (SchemaBase::operator== (other) && (minimum_members == against->minimum_members)
                && (maximum_members == against->maximum_members) && (members == against->members));
    }

    /** Validate a dictionary:
        - Check all elements conform to their respected schema.
        - Ensure all mandatory elements are present.
        - Ensure there are no unknown elements present.
        - Make sure any co-dependent keys are present. */
    void DictionarySchema::validate (const Parsnip::Data &entry) const {
        if (isValidlyNull (entry)) {
            return;
        }
        entry.mandateType (Data::Type::Dictionary);
        if (entry.data.dictionary->size() < minimum_members) {
            throw InvalidValue ("too few properties");
        } else if (entry.data.dictionary->size() > maximum_members) {
            throw InvalidValue ("too many properties");
        }
        // Check that members comply
        for (const auto &member : members) {
            if (entry.contains (member.first)) {
                for (const auto &expect : member.second.expects) {
                    if (!entry.contains (expect)) {
                        throw NoSuchKey (expect + " (expected by " + member.first + ")");
                    }
                }
                try {
                    member.second.member_schema->validate (entry [member.first]);
                } catch (Exception &ex) {
                    ex.addBacktraceLocation (member.first);
                    throw;
                }
            } else if (member.second.mandatory) {
                throw NoSuchKey (member.first);
            }
        }
        if (tolerance != AdditionalMemberNames::ANY || additional_member_schema) {
            // Check for extra crap that shouldn't be in there
            std::function<void (const std::string &, const Parsnip::Data &)> lambda{
                [this] (const std::string &key, const Parsnip::Data &value) -> void {
                    try {
                        auto it = this->members.find (key);
                        if (tolerance != AdditionalMemberNames::ANY && it == this->members.end()) {
                            if (tolerance == AdditionalMemberNames::NONE) {
                                throw InvalidKey (key);
                            }
                            std::match_results<StringType::const_iterator> matches;
                            if (!regex_match (key, matches, additional_member_regex)) {
                                throw InvalidKey (key + ": does not match key pattern.");
                            }
                        }
                        if (this->additional_member_schema) {
                            additional_member_schema->validate (value);
                        }
                    } catch (Exception &ex) {
                        ex.addBacktraceLocation (key);
                        throw;
                    }
                }
            };
            entry.foreach (lambda);
        }
    }

    void DictionarySchema::mergeSchemas (const SchemaBase &from_base) {
        SchemaBase::mergeSchemas (from_base);
        const DictionarySchema *from = static_cast<const DictionarySchema *> (&from_base);

        // Copy new elements from 'from' into 'into'.
        bool fresh = members.empty();
        for (const auto &new_el : from->members) {
            const std::string &key = new_el.first;
            auto it = members.find (key);
            if (it == members.end()) {
                // New member: add it as-is.
                DictionaryMember &created = this->members [key];
                created.member_schema.reset (new_el.second.member_schema->createCopy());
                // New elements start mandatory only if this is the first merge.
                created.mandatory = fresh;
                // Start by expecting all other elements seen at same time
                for (const auto &el : from->members) {
                    if (key != el.first) {
                        created.expects.insert (el.first);
                    }
                }
            } else {
                // Merge list into/overwriting single item
                if (!dynamic_cast<ListSchema *> (it->second.member_schema.get())) {
                    if (dynamic_cast<const ListSchema *> (new_el.second.member_schema.get())) {
                        // Move our existing schema into a list
                        ListSchema *upgrade = new ListSchema (std::move (it->second.member_schema));
                        it->second.member_schema.reset (upgrade);
                    }
                }
                try {
                    it->second.member_schema->mergeSchemas (*(new_el.second.member_schema));
                } catch (const SchemaConflict &reason) {
                    throw SchemaConflict (new_el.first + ": " + reason.what());
                } catch (const NoSuchKey &reason) {
                    throw NoSuchKey (new_el.first + ": " + reason.what());
                }
            }
        }
        if (fresh) {
            return;
        }
        // Deal with missing elements
        for (auto &existing : members) {
            if (from->members.find (existing.first) == from->members.end()) {
                // Demote existing elements as not mandatory
                existing.second.mandatory = false;
            }
        }
        // Trim element codependencies
        // For each element in the new schema...
        for (auto &el : from->members) {
            auto it = members.find (el.first);
            assert (it != members.end());
            // ...go through its list of expectations.
            for (auto trimmer = it->second.expects.begin(); trimmer != it->second.expects.end();) {
                if (from->members.find (*trimmer) == from->members.end()) {
                    // Expected element isn't in new schema, so remove expectation.
                    trimmer = it->second.expects.erase (trimmer);
                } else {
                    trimmer++;
                }
            }
        }
    }

    /*
     *  ConjunctionSchema
     */

    /// Name-to-enumeration mappings for the various conjunctions.
    const std::unordered_map<std::string, ConjunctionSchema::Action> ConjunctionSchema::key_names{
        { "allOf", Action::All },
        { "anyOf", Action::Any },
        { "oneOf", Action::ExactlyOne },
        { "not", Action::Not }
    };

    /** Checks a schema specification for a conjunction.
        @return True if a conjunction is present, false otherwise. */
    bool ConjunctionSchema::is_conjunction (const Parsnip::Data &schema_spec) {
        for (const auto &action : key_names) {
            if (schema_spec.contains (action.first)) {
                return true;
            }
        }
        return false;
    }

    /** Retrieve the conjunction action name from a dictionary.
        @param schema_spec The schema specification.
        @return The action name found.
        @throw SchemaConflict if the schema specifies multiple conjoining actions. */
    std::string ConjunctionSchema::get_action_name (const Parsnip::Data &schema_spec) {
        std::string act;
        for (const auto &possible_action : key_names) {
            if (schema_spec.contains (possible_action.first)) {
                if (!act.empty()) {
                    throw SchemaConflict ("Schema element defined as both " + possible_action.first + " and " + act);
                }
                act = possible_action.first;
            }
        }
        assert (!act.empty());
        if (act.empty()) {
            throw NoSuchKey ("type or combiner");
        }
        return act;
    }

    Parsnip::Data ConjunctionSchema::merge_dictionaries (const Parsnip::Data &baseline,
                                                         const Parsnip::Data &overrides) {
        overrides.mandateType (Data::Type::Dictionary);
        Parsnip::Data merged = baseline;
        std::copy (overrides.data.dictionary->begin(),
                   overrides.data.dictionary->end(),
                   std::inserter (*(merged.data.dictionary), merged.data.dictionary->end()));
        return merged;
    }

    ConjunctionSchema::ConjunctionSchema (const Parsnip::Data &conjunction_spec)
    :  // Do not call the base constructor; conjunction can't be nullable.
      action_name (get_action_name (conjunction_spec)),
      action (key_names.at (action_name)) {
        // The spec describes "factoring out" common elements into the conjunction itself.
        // The conjunction and the schemas of the things conjoined will be dictionaries
        // (whether or not they describe dictionaries, at least the description will be).
        // This necessitates some real inefficiencies.

        // Make a copy of the dictionary, less the conjunction.
        conjunction_spec.mandateType (Data::Type::Dictionary);
        Parsnip::Data baseline{ Data::Dictionary };
        for (const auto &item : *(conjunction_spec.data.dictionary)) {
            auto it = key_names.find (item.first);
            if (it == key_names.end()) {
                baseline [item.first] = item.second;  // Copy construction!
            }
        }
        // Merge the baseline & the individual specs, and add them as children.
        const auto &sub_spec = conjunction_spec [action_name];
        try {
            if (action == Action::Not) {
                children.push_back (SchemaBaseRef{ construct_schema (merge_dictionaries (baseline, sub_spec)) });
            } else {
                for (const auto &item : sub_spec) {
                    children.push_back (SchemaBaseRef{construct_schema (merge_dictionaries (baseline, item))});
                }
            }
        } catch (Exception &ex) {
            ex.addBacktraceLocation ("[" + action_name + "]");
        }
    }

    ConjunctionSchema *ConjunctionSchema::createCopy() const {
        return new ConjunctionSchema (*this);
    }

    void ConjunctionSchema::validate (const Parsnip::Data &entry) const {
        int valid_count = 0;
        for (const auto &child : children) {
            try {
                child->validate (entry);
                valid_count++;
                if (action == Action::Any) {
                    return;
                }
            } catch (Exception &ex) {
                if (action == Action::All) {
                    throw;
                }
                if (action == Action::Not) {
                    return;
                }
                continue;
            }
            if (action == Action::ExactlyOne && valid_count > 1) {
                throw InvalidValue ("Invalid due to 'oneOf' where valid schemas > 1");
            }
            if (action == Action::Not) {
                throw InvalidValue ("Invalid due to 'not' of valid schema");
            }
        }
        if (action == Action::Any) {
            throw InvalidValue ("anyOf: No valid schemas");
        } else if (action == Action::ExactlyOne && valid_count == 0) {
            throw InvalidValue ("oneOf: No valid schemas");
        }
        return;
    }

    bool ConjunctionSchema::operator== (const SchemaBase &other) const {
        const ConjunctionSchema *against = static_cast<const ConjunctionSchema *> (&other);
        bool same = SchemaBase::operator== (other) && (action == against->action)
                    && (children.size() == against->children.size());
        for (unsigned child = 0; same && child < children.size(); child++) {
            same = (*(children [child]) == *(against->children [child]));
        }
        return same;
    }

    std::ostream &ConjunctionSchema::dump (std::ostream &target, int indent, bool suppress_indent) const {
        do_indent (target, suppress_indent ? 0 : indent);
        target << "Conjoining Schema" << (nullable ? "(nullable)" : "") << " accepting " << action_name << ":\n";
        for (ChildrenList::size_type i = 0; i < children.size(); i++) {
            do_indent (target, indent + 2);
            target << '(' << (i + 1) << "): ";
            children [i]->dump (target, indent + 2, true);
        }
        return target;
    }

    /*
     *              Schemas and schema construction
     */

    /** Verify the schema is the dictionary we expect, and return it typecast
        as such.  Throw if it's not what we expected. */
    static inline DictionarySchema *asDictionary (const SchemaBaseRef &schema) {
        DictionarySchema *dict = dynamic_cast<DictionarySchema *> (schema.get());
        if (dict) {
            return (dict);
        }
        throw std::runtime_error ("Schema is not a DictionarySchema");
    }

    /*
     *  Schema
     */

#define REGEX_DATE "[0-9]{4}-(0[1-9]|1[0-2])-[0-4][0-9]"
#define REGEX_TIME "([01][0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9]\\+(0[0-9]|1[0-3]):[0-5][0-9]"

    /** Regular expressions for JSON strings that specify a `format`. */
    static const std::unordered_map<std::string, std::string> RegExForFormat{
        { "date-time", REGEX_DATE "T" REGEX_TIME },
        { "time", REGEX_TIME },
        { "date", REGEX_DATE },
        { "email", "([a-zA-Z0-9_\\-\\.]+)@([a-zA-Z0-9_\\-\\.]+)\\.([a-zA-Z]{2,63})" },
        { "hostname",
          "(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-"
          "9])" },
        { "macaddr", "([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})" },
        { "ipv4", "((25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9]{1,2})" },
        { "ipv6",
          "fe80:(:[0-9a-fA-F]{0,4}){0,4}(%[0-9a-zA-Z]{1,})?|"
          "::(ffff(:0{1,4}){0,1}:){0,1}((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){"
          "0,1}[0-9])|"
          "(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|"
          "([0-9a-fA-F]{1,4}:){1,7}:|"
          "([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|"
          "([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|"
          "([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|"
          "([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|"
          "([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|"
          "[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|"
          ":((:[0-9a-fA-F]{1,4}){1,7}|:)|"
          "([0-9a-fA-F]{1,4}:){1,4}:((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){"
          "0,1}[0-9]))" },
        { "uri",
          "([a-z0-9+.-]+):(?://"
          "(?:((?:[a-z0-9-._~!$&'()*+,;=:]|%[0-9A-F]{2})*)@)?((?:[a-z0-9-._~!$&'()*+,;=]|%[0-9A-F]{2})*)(?::([0-9]*)"
          ")?(/(?:[a-z0-9-._~!$&'()*+,;=:@/]|%[0-9A-F]{2})*)?|(/"
          "?(?:[a-z0-9-._~!$&'()*+,;=:@]|%[0-9A-F]{2})+(?:[a-z0-9-._~!$&'()*+,;=:@/"
          "]|%[0-9A-F]{2})*)?)(?:\\?((?:[a-z0-9-._~!$&'()*+,;=:/"
          "?@]|%[0-9A-F]{2})*))?(?:#((?:[a-z0-9-._~!$&'()*+,;=:/?@]|%[0-9A-F]{2})*))?" }
    };  // Thanks, various folks on Stack Overflow.

    /** Assemble a schema from a specification.
        @param schema_spec The specification. */
    static SchemaBase *construct_schema (const Parsnip::Data &schema_spec) {
        if (ConjunctionSchema::is_conjunction (schema_spec)) {
            return new ConjunctionSchema (schema_spec);
        }
        std::string type = schema_spec ["type"].asString();
        bool is_enum = (schema_spec.contains ("enum") || schema_spec.contains ("const"));
        if (type == "string") {
            if (is_enum) {
                return new KeywordSchema (schema_spec);
            } else if (schema_spec.contains ("pattern")) {
                return new RegExSchema (schema_spec);
            } else if (schema_spec.contains ("format")) {
                auto it = RegExForFormat.find (schema_spec ["format"].asString());
                if (it == RegExForFormat.end()) {
                    throw InvalidSchema ("No such format: " + schema_spec ["format"].asString());
                }
                return new RegExSchema (it->second);
            }
            return new StringSchema (schema_spec);
        }
        if (is_enum) {
            throw std::runtime_error ("Enum supported only for strings.");
        }
        if (type == "object") {
            return new DictionarySchema (schema_spec);
        } else if (type == "integer") {
            return new IntegerSchema (schema_spec);
        } else if (type == "number") {
            return new NumberSchema (schema_spec);
        } else if (type == "array") {
            return new ListSchema (schema_spec);
        } else if (type == "boolean") {
            return new TypeSchema (Data::Type::Boolean, schema_spec);
        } else if (type == "null") {
            return new TypeSchema (Data::Type::Null);
        } else if (type == "any") {
            return new UncheckedSchema();
        }
        throw DataFormatError ("Unknown schema element type: " + type);
    }

    const Schema::Dependencies Schema::NoDependencies;

    /** Construct a schema manually.
        @param from Manually assembled schema components. */
    Schema::Schema (SchemaBase *from) : schema (from) {
    }

    /** Construct a schema from an option parser.
        @param from The option parser from which to construct the parser.
        @param option_schemas Previously-constructed schemas for option parsers on
        which the current one depends. */
    Schema::Schema (const OptionParser &from, const OptionSchemas &option_schemas) : schema (new DictionarySchema) {
        // Weird initialization works around a clang 8/spec bug
        // See: https://stackoverflow.com/questions/7411515
        const DictionarySchema empty_schema (DictionarySchema{});
        from.evaluator->constructSchema (
                empty_schema,
                option_schemas,
                std::bind (&Schema::integrateSchema, this, std::placeholders::_1, std::placeholders::_2));
    }

    /** Construct a schema that has been defined in JSON.
        @see: http://json-schema.org
        @param schema_spec The schema specification. */
    Schema::Schema (const Parsnip::Data &schema_spec) {
        if (schema_spec.contains ("$schema") || schema_spec.contains ("schema")) {
            schema.reset (construct_schema (schema_spec));
        } else {
            throw DataFormatError ("Missing $schema keyword");
        }
    }

    /** Callback function, invoked when generating schemas from an OptionParser.
        @param command A dataset a valid command would generate. */
    void Schema::integrateSchema (const int, const class DictionarySchema &command) {
        schema->mergeSchemas (command);
    }

    /** Validate data against the schema.
        @param data The data to be validated.
    `   @throws Exceptions representing the manner of data non-conformance. */
    void Schema::validate (const Data &data) const {
        assert (schema);
        schema->validate (data);
    }

    /** @internal Dump the schema in human-readable format.
        @param target A stream to which to render.
        @param indent The amount of indentation to use.
        @param suppress_indent If true, indentation is not done on the first line. */
    std::ostream &Schema::dump (std::ostream &target, int indent, bool suppress_indent) const {
        return schema->dump (target, indent, suppress_indent);
    }

    /** Dump the schema in human-readable format.
        @param intro A label for the schema.
        @param target A stream to which to render. */
    std::ostream &Schema::dump (const std::string &intro, std::ostream &target) const {
        target << intro << ": ";
        schema->dump (target, intro.size() + 2, true);
        return target;
    }

    /** Modify a schema by adding a new dictionary member.
        @param name The dictionary member to add.
        @param new_schema The schema for the new member.
        @param mandatory If true, the member must be present for dictionaries to be valid.
        @param dependencies The names of other properties that must also be present when
        this one is.*/
    void Schema::addMember (const char *name,
                            const SchemaBase &new_schema,
                            bool mandatory,
                            const Dependencies &dependencies) {
        asDictionary (schema)->addMember (name, new_schema, mandatory, dependencies);
    }

    /** Modify a schema by replacing dictionary member.  Mandatory status
        and dependencies are unchanged.
        @param name The dictionary member to replace.
        @param new_schema The replacement schema. */
    void Schema::replaceMember (const char *name, const SchemaBase &new_schema) {
        asDictionary (schema)->replaceMember (name, new_schema);
    }

    /** Modify a schema by removing a dictionary member.
        @param name The dictionary member to remove. */
    void Schema::removeMember (const char *name) {
        asDictionary (schema)->removeMember (name);
    }

    /*
     * Schemaset
     */

    /** Construct schemas from a parser.  This first constructs
        schemas for all its option parsers, then generates the
        schema for the parser itself.
        @param from The parser for which schemas are being generated. */
    SchemaSet::SchemaSet (const Parser &from) {
        // Build the schemas for the option validators.
        OptionSchemas::size_type ready = 0;
        while (ready < from.option_parsers.size()) {
            OptionSchemas::size_type last_ready = ready;
            ready = 0;
            for (const auto &parser : from.option_parsers) {
                auto it = option_schemas.find (parser.second.get());
                if (it != option_schemas.end()) {
                    ready++;
                } else {
                    try {
                        SchemaRef construct{ new Schema (*(parser.second), option_schemas) };
                        option_schemas [parser.second.get()] = std::move (construct);
                        ready++;
                    } catch (NoSuchKey &) {
                        // Do nothing;
                    }
                }
            }
            assert (ready > last_ready);
        }

        // Build the schemas for the command lines
        // Weird initialization works around a clang 8/spec bug
        // See: https://stackoverflow.com/questions/7411515
        const DictionarySchema empty_schema (DictionarySchema{});
        from.evaluator->constructSchema (
                empty_schema,
                option_schemas,
                std::bind (&SchemaSet::integrateSchema, this, std::placeholders::_1, std::placeholders::_2));
    }

    /** Get a schema by ID.
        @param command_id The command ID.
        @return The schema.
        @throw NoSchemaDefined if there is no schema for `command`. */
    SchemaBaseRef &SchemaSet::getSchemaForCommand (CommandId command_id) {
        auto it = schemas.find (command_id);
        if (it == schemas.end()) {
            throw NoSchemaDefined (command_id);
        }
        assert (it->second);
        return it->second;
    }

    /** Constipated variant of this function. */
    const SchemaBaseRef &SchemaSet::getSchemaForCommand (CommandId command_id) const {
        auto it = schemas.find (command_id);
        if (it == schemas.end()) {
            throw NoSchemaDefined (command_id);
        }
        assert (it->second);
        return it->second;
    }

    /** Callback function, invoked when generating schemas from a Parser.
        @param command_id The command ID the dataset represents.
        @param command A dataset a valid command would generate. */
    void SchemaSet::integrateSchema (const int command_id, const class DictionarySchema &command) {
        SchemaBaseRef &target = schemas [command_id];
        if (!target) {
            target.reset (new DictionarySchema);
        }
        target->mergeSchemas (command);
    }

    /** Validate data against a command's schema.
        @param command_id The command the data represents.
        @param data The data to be validated.
    `   @throws Exceptions representing the manner of data non-conformance. */
    void SchemaSet::validate (const CommandId command_id, const Data &data) const {
        getSchemaForCommand (command_id)->validate (data);
    }

    /** Dump the schema in human-readable format.
        @param intro A label for the schema.
        @param command_id The ID of the command to render.
        @param target A stream to which to render. */
    std::ostream &SchemaSet::dump (const std::string &intro, const CommandId command_id, std::ostream &target) const {
        const SchemaBaseRef &sch = getSchemaForCommand (command_id);
        target << intro << ": ";
        sch->dump (target, intro.size() + 2, true);
        return target;
    }

    /** Modify a schema by adding a new dictionary member.
        @param command_id The command number to modify.
        @param name The dictionary member to add.
        @param schema The schema for the new member.
        @param mandatory If true, the member must be present for dictionaries to be valid.
        @param dependencies The names of other properties that must also be present when
        this one is.*/
    void SchemaSet::addMember (const CommandId command_id,
                               const char *name,
                               const SchemaBase &schema,
                               bool mandatory,
                               const Dependencies &dependencies) {
        asDictionary (getSchemaForCommand (command_id))->addMember (name, schema, mandatory, dependencies);
    }

    /** Add member to every schema in the schema set.
        @param name The dictionary member to add.
        @param schema The schema for the new member.
        @param mandatory If true, the member must be present for dictionaries to be valid.
        @param dependencies The names of other properties that must also be present when
        this one is.*/
    void SchemaSet::addMember (const char *name,
                               const SchemaBase &schema,
                               bool mandatory,
                               const Dependencies &dependencies) {
        for (auto &command : schemas) {
            asDictionary (command.second)->addMember (name, schema, mandatory, dependencies);
        }
    }

    /** Modify a schema by replacing dictionary member.  Mandatory status
        and dependencies are unchanged.
        @param command_id The command number to modify.
        @param name The dictionary member to replace.
        @param schema The replacement schema. */
    void SchemaSet::replaceMember (const CommandId command_id, const char *name, const SchemaBase &schema) {
        asDictionary (getSchemaForCommand (command_id))->replaceMember (name, schema);
    }

    /** Modify a schema by removing a dictionary member.
        @param command_id The command number to modify.
        @param name The dictionary member to remove. */
    void SchemaSet::removeMember (const CommandId command_id, const char *name) {
        asDictionary (getSchemaForCommand (command_id))->removeMember (name);
    }

}  // namespace Parsnip
