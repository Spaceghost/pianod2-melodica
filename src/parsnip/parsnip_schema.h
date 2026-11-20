///
/// Parsnip serialization schemas.
/// Structures for describing the structure of data.
/// @file       parsnip_schema.h - Parsnip serialization & parsing
/// @author     Perette Barella
/// @date       2020-12-17
/// @copyright  Copyright 2020-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <map>
#include <unordered_map>
#include <string>
#include <set>
#include <limits>
#include <regex>

#include "parsnip.h"
#include "parsnip_command.h"

namespace Parsnip {
    using SchemaIntegratorCallback = std::function <void (const int, const class DictionarySchema &)>;

    // Base class for schema validation components.
    class SchemaBase {
        friend class ListSchema;
    protected:
        using StringType = Data::StringType;
        using SchemaRegex = std::basic_regex <StringType::value_type>;

        bool nullable = false; ///< If true, element may be `null`.

        SchemaBase () = default;
        SchemaBase (const SchemaBase &) = default;
        SchemaBase (SchemaBase &&) = default;
        SchemaBase &operator =(const SchemaBase &) = default;
        SchemaBase &operator =(SchemaBase &&) = default;
        SchemaBase (const Parsnip::Data &schema_spec);

        /** Check that Data item is an allowed type.  If nullable, it must be
            the expected type _or_ null.  If not nullable it _must_ be the expected type.
            @param expected The expected datatype.
            @param entry The Data item to check. */
        inline void checkType (Data::Type expected, const Parsnip::Data &entry) const {
            if (!nullable || entry.datatype != Data::Type::Null) {
                entry.mandateType (expected);
            }
        }
        
        /** Determine if the data is Null and if that is allowed.
            @param entry The data to validate.
            @return True if the data is null, and the schema allows it. */
        inline bool isValidlyNull (const Parsnip::Data &entry) const {
            return (nullable && entry.datatype == Data::Type::Null);
        }

        /** Check if a data object is a particular type.
            @param expected The type to compare to.
            @param entry The data item to check.
            @return True if entry's datatype is expected, false otherwise. */
        inline bool isType (Data::Type expected, const Parsnip::Data &entry) const {
            return (entry.datatype == expected);
        }

    public:
        /** Create an identical, deep copy of the schema.
            Caller is responsible for deleting it. */
        virtual SchemaBase *createCopy () const = 0;
        virtual ~SchemaBase() = default;

        /** Verify that Data matches the schema.
            @param entry The data to validate.
            @throw Various exceptions. */
        virtual void validate (const Parsnip::Data &entry) const = 0;

        /** Check if two schemas perform the same validation.
            @param other The schema to compare to.
            @return True if the schemas match, false otherwise. */
        virtual bool operator== (const SchemaBase &other) const;
        inline bool operator!= (const SchemaBase &other) const {
            return !operator== (other);
        }
        
        /** Merge elements into the current schema.
            - Elements seen every time are mandatory.
            - Elements seen only sometime are optional.
            - Elements always seen together are codependent.
            - Elements must always expect similar types, _except_
            they may vary as element and list-of-same-element.
            @param from A schema for a valid command command line.
            @throw Various exceptions. */
        virtual void mergeSchemas (const SchemaBase &from);
        
        /** Render the schema in human-friendly text.
            @param target Where to write the schema.
            @param indent Amount of indent to use.
            @param suppress_indent Flag indicating indent was already performed. */
        virtual std::ostream &dump (std::ostream &target, int indent, bool suppress_indent = false) const = 0;
    };

    /*
     *              Schema Elements
     */

    /// Schema component requiring data be a particular type.
    class UncheckedSchema : public SchemaBase {
    public:
        UncheckedSchema() = default;
        UncheckedSchema (const UncheckedSchema &) = default;
        UncheckedSchema (UncheckedSchema &&) = default;
        UncheckedSchema &operator= (const UncheckedSchema &) = default;
        UncheckedSchema &operator= (UncheckedSchema &&) = default;
        virtual UncheckedSchema *createCopy () const override;

        virtual void validate (const Parsnip::Data &) const override;
        virtual std::ostream &dump (std::ostream &target, int indent, bool suppress_ident = false) const override;
    };

    /// Schema component requiring data be a particular type.
    class TypeSchema : public SchemaBase {
    private:
        Data::Type expected = Data::Type::Null;

    public:
        TypeSchema() = default;
        TypeSchema (const TypeSchema &) = default;
        TypeSchema (TypeSchema &&) = default;
        TypeSchema &operator= (const TypeSchema &) = default;
        TypeSchema &operator= (TypeSchema &&) = default;
        TypeSchema (const Data::Type require);
        TypeSchema (const Data::Type require, const Parsnip::Data &schema_spec);
        virtual TypeSchema *createCopy () const override;

        virtual void validate (const Parsnip::Data &entry) const override;
        virtual bool operator== (const SchemaBase &other) const override;
        virtual void mergeSchemas (const SchemaBase &from) override;
        virtual std::ostream &dump (std::ostream &target, int indent, bool suppress_ident = false) const override;
    };

    /// Schema component requiring data be a string of characters
    /// (no control characters).  If no character checking is desired,
    /// use a TypeSchema instead.
    class StringSchema : public SchemaBase {
    public:
        using size_type = StringType::size_type;
    protected:
        size_type min_length;
        size_type max_length;
    public:
        /// Validate a type schema: ensure the datatype is as expected.
        StringSchema (size_type min = 0, size_type max = std::numeric_limits<size_type>::max());
        StringSchema (const StringSchema &) = default;
        StringSchema (StringSchema &&) = default;
        StringSchema &operator= (const StringSchema &) = default;
        StringSchema &operator= (StringSchema &&) = default;
        StringSchema (const Parsnip::Data &schema_spec);
        virtual StringSchema *createCopy () const override;

        virtual void validate (const Parsnip::Data &entry) const override;
        virtual bool operator== (const SchemaBase &other) const override;
        virtual std::ostream &dump (std::ostream &target, int indent, bool suppress_ident = false) const override;
    };


    /// Schema component requiring data be a string of characters
    /// matching a regular expression.
    class RegExSchema : public StringSchema {
    public:
        using size_type = StringType::size_type;
    private:
        StringType expression;
        SchemaRegex regex;
    public:
        RegExSchema (const StringType &express, bool case_blind = false, size_type min = 0, size_type max = std::numeric_limits<size_type>::max());
        RegExSchema (const RegExSchema &) = default;
        RegExSchema (RegExSchema &&) = default;
        RegExSchema &operator= (const RegExSchema &) = default;
        RegExSchema &operator= (RegExSchema &&) = default;
        RegExSchema (const Parsnip::Data &schema_spec);
        virtual RegExSchema *createCopy () const override;

        virtual void validate (const Parsnip::Data &entry) const override;
        virtual bool operator== (const SchemaBase &other) const override;
        virtual std::ostream &dump (std::ostream &target, int indent, bool suppress_ident = false) const override;
    };


    /// Schema component accepting strings with certain values.
    class KeywordSchema : public SchemaBase {
    private:
        std::set<StringType> valid_values;
        bool ignore_case = true;
    public:
        KeywordSchema() = default;
        KeywordSchema (const KeywordSchema &) = default;
        KeywordSchema (KeywordSchema &&) = default;
        KeywordSchema &operator= (const KeywordSchema &) = default;
        KeywordSchema &operator= (KeywordSchema &&) = default;
        KeywordSchema (const std::string &word);
        KeywordSchema (const Parsnip::Data &schema_spec);
        virtual KeywordSchema *createCopy () const override ;
        void addKeyword (const std::string &word);
        virtual void validate (const Parsnip::Data &entry) const override;
        virtual bool operator== (const SchemaBase &other) const override;
        virtual void mergeSchemas (const SchemaBase &from) override;
        virtual std::ostream &dump (std::ostream &target, int indent, bool suppress_ident = false) const override;
    };

    /// Schema component for numeric data, either Integer or Real.
    template <typename NumericType>
    class RangeSchema : public SchemaBase {
    public:
        using value_type = NumericType;

    private:
        bool minimumIsInclusive = true;
        NumericType minimum;
        bool maximumIsInclusive = true;
        NumericType maximum;

    public:
        RangeSchema (const NumericType min = std::numeric_limits<NumericType>::lowest(),
                     const NumericType max = std::numeric_limits<NumericType>::max());
        RangeSchema() = default;
        RangeSchema (const RangeSchema &) = default;
        RangeSchema (RangeSchema &&) = default;
        RangeSchema &operator= (const RangeSchema &) = default;
        RangeSchema &operator= (RangeSchema &&) = default;
        RangeSchema (const Parsnip::Data &schema_spec);
        virtual RangeSchema *createCopy () const override;

        virtual void validate (const Parsnip::Data &entry) const override;
        virtual bool operator== (const SchemaBase &other) const override;
        virtual void mergeSchemas (const SchemaBase &from) override;
        virtual std::ostream &dump (std::ostream &target, int indent, bool suppress_ident = false) const override;
    };
    
    using NumberSchema = RangeSchema <double>;
    
    // Schema class for accepting integers.
    class IntegerSchema : public RangeSchema <long> {
        using range_type = RangeSchema <long>;
    private:
        value_type multiple = 0; ///< If non-0, valid values are a multiple of this number.
    public:
        using range_type::RangeSchema;
        IntegerSchema (const Parsnip::Data &schema_spec);
        virtual IntegerSchema *createCopy () const override;

        virtual bool operator== (const SchemaBase &other) const override;
        virtual void validate (const Parsnip::Data &entry) const override;
    };

    /// Schema adapter component that allows use of schemas from option parsers.
    class OptionSchema : public SchemaBase {
    private:
        SchemaRef schema;

    public:
        /// Validate a type schema: ensure the datatype is as expected.
        OptionSchema() = default;
        OptionSchema (const OptionSchema &) = default;
        OptionSchema (OptionSchema &&) = default;
        OptionSchema &operator= (const OptionSchema &) = default;
        OptionSchema &operator= (OptionSchema &&) = default;
        OptionSchema (const SchemaRef &option_schema);
        virtual OptionSchema *createCopy () const override;

        virtual void validate (const Parsnip::Data &entry) const override;
        virtual bool operator== (const SchemaBase &other) const override;
        virtual void mergeSchemas (const SchemaBase &from) override;
        virtual std::ostream &dump (std::ostream &target, int indent, bool suppress_ident = false) const override;
    };

    /// Schema component for lists.  Note this expects list members to
    /// all be uniform element type.
    class ListSchema : public SchemaBase {
    public:
        using size_type = Data::ListType::size_type;
    private:
        SchemaBaseRef member_schema;
        size_type minimum_required = 0;
        size_type maximum_allowed = std::numeric_limits<int>::max();
        bool single_as_nonlist_allowed = false;

    public:
        ListSchema() = default;
        ListSchema (const ListSchema &from) = default;
        ListSchema (ListSchema &&) = default;
        ListSchema &operator= (const ListSchema &from) = default;
        ListSchema &operator= (ListSchema &&) = default;
        ListSchema (SchemaBaseRef &&, size_type minimum = 0, size_type maximum = std::numeric_limits<size_type>::max());
        ListSchema (const SchemaBase &, size_type minimum = 0, size_type maximum = std::numeric_limits<size_type>::max());
        ListSchema (const Parsnip::Data &schema_spec);
        virtual ListSchema *createCopy () const override;

        virtual void validate (const Parsnip::Data &entry) const override;
        virtual bool operator== (const SchemaBase &other) const override;
        virtual void mergeSchemas (const SchemaBase &from) override;
        virtual std::ostream &dump (std::ostream &target, int indent, bool suppress_ident = false) const override;
    };

    /// Schema component for validating dictionaries.
    class DictionarySchema : public SchemaBase {
        friend class ValueEvaluator;
        friend class RemainingValuesEvaluator;
        friend class KeywordEvaluator;
        friend class OptionEvaluator;
    public:
        using Dependencies = std::set<std::string>;
        using size_type = unsigned int;
        static const Dependencies NoDependencies;
    private:
        size_type minimum_members {0};
        size_type maximum_members = std::numeric_limits<size_type>::max();
    protected:
        class DictionaryMember {
        public:
            SchemaBaseRef member_schema;    ///< Schema for this dictionary member
            Dependencies expects;  ///< Other elements that must be present when this one is
            bool mandatory = false;         ///< Whether element must be present

            bool operator== (const DictionaryMember &other) const;
        };

        enum AdditionalMemberNames { NONE, ANY, PATTERN };
        std::map <std::string, DictionaryMember> members;
        AdditionalMemberNames tolerance = AdditionalMemberNames::NONE; ///< Allow unrecognized members in the dictionary?
        SchemaRegex additional_member_regex;
        SchemaBaseRef additional_member_schema;

    public:
        DictionarySchema() = default;
        DictionarySchema (const DictionarySchema &) = default;
        DictionarySchema (DictionarySchema &&) = default;
        DictionarySchema &operator= (const DictionarySchema &) = default;
        DictionarySchema &operator= (DictionarySchema &&) = default;
        DictionarySchema (const Parsnip::Data &schema_spec);
        virtual DictionarySchema *createCopy () const override;
        
        void addMember (const char *name, const SchemaBase &schema, bool mandatory = false,
                        const Dependencies &dependencies = NoDependencies);
        void replaceMember (const char *name, const SchemaBase &schema);
        void removeMember (const char *name);

        virtual void validate (const Parsnip::Data &entry) const override;
        virtual bool operator== (const SchemaBase &other) const override;
        virtual void mergeSchemas (const SchemaBase &from) override;
        virtual std::ostream &dump (std::ostream &target, int indent, bool suppress_ident = false) const override;
    };


    /// A meta-schema that determines if any, all, or exactly one or none
    /// of its subschemas are valid.  Also provides "not".
    class ConjunctionSchema : public SchemaBase {
    public:
        enum class Action {
            All,
            Any,
            ExactlyOne,
            Not
        };
        static bool is_conjunction (const Parsnip::Data &schema_spec);
    private:
        static const std::unordered_map<std::string, Action> key_names;
        std::string action_name;
        Action action = Action::Any;
        using ChildrenList = std::vector<SchemaBaseRef>;
        ChildrenList children;
        static std::string get_action_name (const Parsnip::Data &schema_spec);
        Parsnip::Data merge_dictionaries (const Parsnip::Data &baseline, const Parsnip::Data &overrides);
    public:
        ConjunctionSchema (const ConjunctionSchema &) = default;
        ConjunctionSchema (ConjunctionSchema &&) = default;
        ConjunctionSchema &operator= (const ConjunctionSchema &) = default;
        ConjunctionSchema &operator= (ConjunctionSchema &&) = default;
        ConjunctionSchema (const Parsnip::Data &schema_spec);
        virtual ConjunctionSchema *createCopy () const override;

        virtual void validate (const Parsnip::Data &entry) const override;
        virtual bool operator== (const SchemaBase &other) const override;
        virtual std::ostream &dump (std::ostream &target, int indent, bool suppress_ident = false) const override;
    };

    /// Parsing/command patterns produce schema inconsistency.
    class InvalidSchema : public Exception {
    public:
        inline InvalidSchema (const std::string &reason) : Exception ("invalid schema", reason) {};
    };

}
