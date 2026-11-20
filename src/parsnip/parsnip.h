///
/// Parsnip serialization.
/// A flexible data type for serializing data and parsing
/// messages into structures.
/// @file       parsnip.h - Parsnip serialization & parsing
/// @author     Perette Barella
/// @date       2019-05-01
/// @copyright  Copyright 2019-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <exception>
#include <limits>
#include <memory>
#include <istream>
#include <ostream>
#include <iostream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <set>
#include <functional>
#ifdef PARSNIP_DICTIONARY_SORTED
#include <map>
#endif

#include <cassert>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>

/// Serialization and parsing library.
namespace Parsnip {

    /// Class representing issues related to serialization & parsing.
    class Exception : public std::exception {
    private:
        std::string location; ///< Storage for the exception location.
    protected:
        std::string reason; ///< Storage for the exception explanation.


    public:
        virtual const char *what() const noexcept override {
            return reason.c_str();
        };
        /** Retrieve the exception location information.
            @return A string containing location information, or
            a nullptr if none is available. */
        inline const char *where () const noexcept {
            return location.empty() ? nullptr : location.c_str();
        }
        void addBacktraceLocation (const std::string &);
    protected:
        /** Construct an exception. */
        Exception (const std::string &why) : reason (why){};

        /** Construct an exception.
            @param why The primary explanation for the error.
            @param detail Specifics about the error, such as the problem location or data. */
        Exception (const std::string &why, const std::string &detail) : reason (why) {
            reason += ": ";
            reason += detail;
        };
    };

    /// DataRangeError indicates a value received in a stream can't be represented.
    class DataRangeError : public Exception {
    public:
        inline DataRangeError() : Exception ("Value out of representable range"){};
        inline DataRangeError (const std::string &value) : Exception ("Value exceeds datatype range", value){};
    };

    /** Generic data type.
        A datatype that can hold strings, numbers, booleans, lists, or dictionaries. */
    class Data {
        friend class IncorrectDataType;
        friend class SchemaBase;
        friend class DictionarySchema;
        friend class ConjunctionSchema;

    public:
        static const struct dictionary_t {
        } Dictionary;  ///< Dictionary flag type/value.
        static const struct list_t {
        } List;                             ///< List flag type/value.
        static constexpr bool Flexible{false};  ///< Flag to make strings flexible.
        static constexpr int NoIndent{-32767};  ///< Prevent indentation

        using StringType = std::string;  // As long as everything is UTF-8 encoded anyway...
        using ListType = std::vector<Data>;

        /// Types that may be stored in the data.
        enum class Type {
            Null,            ///< Nothing is present.
            Dictionary,      /// Dictionary keyed by string
            List,
            String,          ///< The value is string, even if the string contains a number.
            FlexibleString,  ///< The value is a string, but may convert to a number if the value is numeric.
            Integer,         ///< Value is an integer but may also be accessed as a real.
            Real,
            Boolean
        };

    protected:
#ifdef PARSNIP_DICTIONARY_SORTED
        using DictionaryType = std::map<std::string, Data>;
#else
        using DictionaryType = std::unordered_map<std::string, Data>;
#endif

        Type datatype = Type::Null; ///< Type presently stored in object.
        union data_t {
            DictionaryType *dictionary;
            ListType *list;
            StringType *str;
            long integer;
            double real;
            bool boolean;
        } data;

        void release();

    private:
        Data (Type kind);
        void mandateType (Type type) const;

        inline void loadDictionary(){};

        /// Dictionary constructor helper function, inserting a moved Data.
        template <typename... More>
        void loadDictionary (const char *name, Data &&value, More &&... remainder) {
            (*(data.dictionary))[name] = std::move (value);
            loadDictionary (std::forward<More> (remainder)...);
        }

        inline void loadArray(){};

        /// List constructor helper function, inserting a moved Data.
        template <typename... More>
        void loadArray (Data &&value, More... remainder) {
            data.list->push_back (std::move (value));
            loadArray (std::forward<More> (remainder)...);
        }

    public:
        using size_type = ListType::size_type;

        /** An iterator used to walk items in Data's lists. */
        struct iterator : std::iterator<std::forward_iterator_tag, Data> {
            const ListType *list = nullptr;
            ListType::size_type position = 0;
            iterator (const Data *l);
            iterator (const Data *l, size_type pos);
            const Data &operator*() const noexcept;
            iterator &operator++() noexcept;
            iterator operator++ (int) noexcept;
            bool operator!= (const iterator &compare) const noexcept;
        };

        /// Default constructor.
        Data (std::nullptr_t null = nullptr) {
            // Do nothing
        }

        /** String constructor.
            @param value The string value to assign.
            @param type_certainty True if known to be string, false if it may be a number or boolean in string form. */
        Data (const char *value, bool type_certainty = true) {
            data.str = new StringType (value);
            datatype = type_certainty ? Type::String : Type::FlexibleString;
        }

        /// String constructor, accepting C++ string.
        inline Data (const std::string &value, bool type_certainty = true) {
            data.str = new StringType (value);
            datatype = type_certainty ? Type::String : Type::FlexibleString;
        }

        /// Integer constructor.
        template <typename T,
                  typename
                  = typename std::enable_if<!std::is_same<T, bool>::value && std::is_integral<T>::value, int>::type>
        inline Data (T value) noexcept {
            datatype = Type::Integer;
            data.integer = value;
        }

        /// Floating point constructor.
        inline Data (double value) noexcept {
            datatype = Type::Real;
            data.real = value;
        }

        /// Boolean constructor.
        inline Data (bool value) noexcept {
            datatype = Type::Boolean;
            data.boolean = value;
        }

        /** Dictionary constructor.
            Use Dictionary flag variable as first parameter.
            Subsequent parameters are name, value ... pairs (function must have
            an odd number of arguments). */
        template <typename... T>
        Data (dictionary_t, T... params) : Data (Type::Dictionary) {
            loadDictionary (std::forward<T> (params)...);
        }

        /** List constructor.
            Use List flag variable as first parameter.
            Subsequent parameters are inserted in order as list members. */
        template <typename... T>
        Data (list_t, T... params) : Data (Type::List) {
            loadArray (std::forward<T> (params)...);
        }

        /// Move construction.
        inline Data (Data &&from) noexcept {
            datatype = from.datatype;
            data = from.data;
            from.datatype = Type::Null;
        }

        /// Move assignment
        inline Data &operator= (Data &&from) noexcept {
            if (this != &from) {
                release();
                datatype = from.datatype;
                data = from.data;
                from.datatype = Type::Null;
            }
            return *this;
        }

        // Copy constructor & assignment
        Data (const Data &from);
        Data &operator= (const Data &from);

        inline ~Data() {
            release();
        }

        bool operator== (const Data &compare) const;
        inline bool operator!= (const Data &compare) const {
            return !operator== (compare);
        }

        const Data &makeFlexible () const;

        /** Check if data contains a value.
        @return True if empty, false otherwise. */
        inline bool isNull (void) const noexcept {
            return (datatype == Type::Null);
        }

        // Retrieve values by asking for type directly.
        const StringType &asString() const;
        long asLong (int base = 0) const;
        int asInteger (int base = 0) const;
        double asDouble() const;
        bool asBoolean() const;
        ListType asList() const;

        /** Extract data as a string (templated version).
        @throw IncorrectDataType if value is not a string. */
        template <typename DataType,
                  typename = typename std::enable_if<std::is_same<DataType, std::string>::value>::type>
        inline const std::string &as() const {
            return asString();
        }

        /** Extract data as an integer type, except boolean.
            @tparam DataType the desired return type.
            @throw DataRangeError if value will not fit in `DataType`.
            @throw IncorrectDataType if value is not an integer. */
        template <typename DataType,
                  typename
                  = typename std::enable_if<!std::is_same<DataType, bool>::value && std::is_integral<DataType>::value>::type>
        inline DataType as (int base = 0) const {
            long result = asLong (base);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
            if (result < std::numeric_limits<DataType>::lowest() || result > std::numeric_limits<DataType>::max()) {
#pragma GCC diagnostic pop
                throw DataRangeError (std::to_string (result));
            }
            return result;
        }

        /** Extract data as a floating point type.
            @tparam DataType the desired return type.
            @throw DataRangeError if value will not fit `DataType`.
            @throw IncorrectDataType if value is not a number. */
        template <typename DataType, typename = typename std::enable_if<std::is_floating_point<DataType>::value>::type>
        inline DataType as() const {
            double result = asDouble();
            if (result < std::numeric_limits<DataType>::lowest() || result > std::numeric_limits<DataType>::max()) {
                throw DataRangeError (std::to_string (result));
            }
            return result;
        }

        /** Extract data as a boolean.
            @throw IncorrectDataType if value is not `true` or `false`. */
        template <typename DataType, typename = typename std::enable_if<std::is_same<DataType, bool>::value>::type>
        inline bool as() const {
            return asBoolean();
        }

        /** Retrieve data as the object itself.
            Seems stupid on the surface, but useful for templates. */
        template <typename DataType, typename = typename std::enable_if<std::is_same<DataType, Data>::value>::type>
        inline const Data &as() const {
            return *this;
        }
        
        /** Return contents of a list as a specific datatype.
            @tparam Container A standard (-like) container, such
            as `std::vector <std::string>`, `std::list <int>`,
            or even `std::vector<std::vector<long>>` to extract
            a contents of a 2-dimensional array.
            Member `typedef Container::value_type` determines the
            inner type.
            @throw IncorrectDataType if the item is not a list, or
            any of the list's members are not the expected type. */
        template <typename Container,
                  typename = typename std::enable_if<std::is_class<Container>::value && !std::is_same<Container, Data>::value
                                                     && !std::is_same<Container, std::string>::value>::type>
        inline Container as(std::nullptr_t null = nullptr) const {
            mandateType (Type::List);
            Container result;
            for (const auto &item : *(data.list)) {
                result.push_back (item.as <typename Container::value_type>());
            }
            return result;
        }
        
        /** Extract a value if a member exists, otherwise use a default.
            @tparam Datatype The desired return type of the data.
            @param name The name of the member to retrieve.
            @param defvalue The default to use if the member is not present.
            @throw IncorrectDataType if the object is not a dictionary,
            or the item is present but not the desired type. */
        template <typename Datatype>
        inline Datatype getOr (const std::string &name, const Datatype &defvalue) const {
            return (contains (name) ? (*this)[name].as<Datatype>() : defvalue);
        }
        
        /** Specialization of 'getOr' that accepts a const char * default; otherwise,
            overload resolution chooses implicit instantiation of a Data and uses
            that as a default rather than implicitly instantiating the string type. */
        inline StringType getOr (const std::string &name, const char *defvalue) const {
            return (contains (name) ? (*this)[name].asString() : StringType (defvalue));
        }

        /** Specialization of `getOr` for `Data`.
            @tparam Datatype The desired return type of the data.
            @param name The name of the member to retrieve.
            @param defvalue The default to use if the member is not present.
            @throw IncorrectDataType if the object is not a dictionary. */
        inline Data getOr (const std::string &name, const Data &defvalue) const {
            return (contains (name) ? (*this)[name] : defvalue);
        }
        
        /** Return a list or a single item in a container.
            @tparam Container A standard (-like) container, such
            as `std::vector <std::string>`.
            Member `typedef Container::value_type` determines the
            inner type.  If this object is a list, the container is
            populated with the list contents; otherwise, it is
            populated with the single data item.
            @throw IncorrectDataType if this object is neither a list
            nor the expected type, or if it is a list but some members
            are not the expected type. */
        template <typename Datatype>
        inline std::vector<Datatype> toList () const {
            if (datatype == Type::List) {
                return as<std::vector <Datatype>> ();
            } else if (datatype == Type::Dictionary) {
                mandateType (Type::List);
            }
            std::vector<Datatype> results;
            results.push_back (as <Datatype>());
            return results;
        }

        // Dictionary accessors
        const Data &operator[] (const std::string &word) const;
        Data &operator[] (const std::string &word);
        bool contains (const std::string &word) const;
        void remove (const std::string &word);
        inline const Data &at (const std::string &word) const {
            return (*this) [word];
        }

        /** Iterate over all dictionary members, calling a function
            with each key and value.  This implementation covers simple types.
            @tparam DataType The datatype expected to be in the dictionary.
            @param func The function to invoke with each member.
            @throw IncorrectDataType if the dictionary contains values
            of inapplicable type.  This occurs when encountered; some
            invokations may happen before throwing. */
        template <typename DataType, typename = typename std::enable_if<std::is_trivial<DataType>::value>::type>
        void foreach (std::function<void (const std::string &, DataType)> func) const {
            mandateType (Type::Dictionary);
            for (const auto &it : *(data.dictionary)) {
                func (it.first, it.second.as<DataType>());
            }
        }

        /// Specialization of dictionary iterator for strings and `Data`.
        template <typename DataType>
        void foreach (std::function<void (const std::string &, const DataType &)> func) const {
            mandateType (Type::Dictionary);
            for (const auto &it : *(data.dictionary)) {
                func (it.first, it.second.as<DataType>());
            }
        }

        // List accessors
        const Data &operator[] (size_type index) const;
        Data &operator[] (size_type index);
        void remove (size_type index);
        void push_back (const Data &value);
        void push_back (Data &&value);
        size_type size() const;

        /** Empty check.  Use `isNull` to check specifically for that
            condition.
            @return True for null type, or lists or dictionaries that are
            empty. */
        inline bool empty () const {
            return (datatype == Type::Null ||
                    (datatype == Type::List && size() == 0) ||
                    (datatype == Type::Dictionary && (*data.dictionary).empty()));

        }

        /** Iterate over all list members, calling a function
            with each value.  This implementation covers simple types.
            @tparam DataType The datatype expected to be in the dictionary.
            @param func The function to invoke with each member.
            @throw IncorrectDataType if the list contains values
            of inapplicable type.  This occurs when encountered; some
            invokations may happen before throwing. */
        template <typename DataType, typename = typename std::enable_if<std::is_trivial<Type>::value>::type>
        void foreach (std::function<void (const DataType)> func) const {
            mandateType (Type::List);
            for (const auto &it : *(data.list)) {
                func (it.as<DataType>());
            }
        }

        /// Specialization of list iterator for strings and `Data`.
        template <typename DataType>
        void foreach (std::function<void (const DataType &)> func) const {
            mandateType (Type::List);
            for (const auto &it : *(data.list)) {
                func (it.as<DataType>());
            }
        }

        /** Retrieve forward iterator.
            @return Forward iterator positioned at first element.
            @throws IncorrectDataType if not a list. */
        inline Data::iterator begin() const {
            return iterator{this};
        }

        /** Retrieve end iterator.
            @return Forward iterator positioned after last element.
            @throws IncorrectDataType if not a list. */
        inline Data::iterator end() const {
            iterator it{this, size()};
            return it;
        }

        static const char *type_name (Type ty);

        /** Dictionary factory.
            @param initial_data Name-value pairs to be inserted into the dictionary.
            @warning Initializer lists cannot use rvalue references; hence,
            elements are copied.  This is fine for simple items, but should be
            avoided when inserting complicatated structures.  Prefer move
            assignment or Parsnip::Data (Parsnip::Data::Dictionary, ...).*/
        template <typename T = Data>
        static Data make_dictionary (std::initializer_list <std::pair <const char *, const T>> initial_data) {
            Data data (Type::Dictionary);
            for (auto &it : initial_data) {
                data [it.first] = it.second;
            }
            return data;
        }

        /** List factory.
            Parameters are inserted in order as list members. */
        template <typename... T>
        static Data make_list(T&&... params) {
            Data data (Type::List);
            data.loadArray (std::forward<T> (params)...);
            return data;
        }

    public:
        std::string toJson (int indent = NoIndent) const;
        std::ostream &toJson (std::ostream &target, int indent = NoIndent, bool suppress = false) const;
        std::ostream &dumpJson (const std::string &intro, std::ostream &target = std::clog) const;
    };

    Data parse_json (std::istream &from, bool check_termination = true);
    Data parse_json (const std::string &from);


#ifdef PARSNIP_JSON_TRACK_POSITION
    class StreamPositionCounter;
    using InputStream = StreamPositionCounter;
#else
    using InputStream = std::istream;
#endif

    /// DataFormatError represents a syntax error in a datastream.
    class DataFormatError : public Exception {
#ifdef PARSNIP_JSON_TRACK_POSITION
    public:
        struct Location {
            unsigned line = 0;
            unsigned character = 0;
            Location () = default;
            Location (const Location &) = default;
            Location &operator =(const Location &) = default;
            Location (unsigned l, unsigned c): line (l), character (c) {};
        };
    private:
        const Location error_location {};
    public:
        /** Retrieve the error location (line & character number).
            If unknown, <0,0> will be returned. */
        inline Location where() const {
            return error_location;
        }
#endif
    public:
        inline DataFormatError (const std::string &detail): Exception ("Invalid data format", detail) {};
        DataFormatError (const InputStream &loc, const std::string &detail);
    };

    /// An element requested from a constant dictionary does not exist.
    class NoSuchKey : public Exception {
    public:
        inline NoSuchKey (const std::string &key) : Exception ("No such key", key){};
    };

    /// The data's type is inconsistent with the use requested of it.
    class IncorrectDataType : public Exception {
    public:
        IncorrectDataType (Data::Type expected, Data::Type actual) : Exception ("Incorrect data type", "Expected: ") {
            reason += Data::type_name (expected);
            reason += ", Actual: ";
            reason += Data::type_name (actual);
        }
    };

    inline std::ostream &operator<< (std::ostream &out, const Parsnip::Data &data) {
        data.toJson (out);
        return out;
    }

    /// The list size does not meet schema requirements.
    class IncorrectListSize : public Exception {
    public:
        inline IncorrectListSize () : Exception ("Wrong list size") {};
        inline IncorrectListSize (int size, int min, int max) :
        Exception ("wrong list size", std::to_string (size) + " (expected " + std::to_string (min) + "-" + std::to_string (max) + "") {};
    };

    /// They key is not specified in the schema.
    class InvalidKey : public Exception {
    public:
        inline InvalidKey (const std::string &key) : Exception ("invalid key", key) {};
    };

    /// Parsing/command patterns produce schema inconsistency.
    class SchemaConflict : public Exception {
    public:
        inline SchemaConflict (const std::string &reason) : Exception ("schema conflict", reason) {};
        SchemaConflict (const std::string &reason, const class SchemaBase &from, const class SchemaBase &into);
    };

    /// Helper functor for schema smart pointers
    struct SchemaBaseDeleter {
        void operator () (class SchemaBase *free_me);
    };

    /// Smart pointer for schemas, with unusual ability to be copied.
    class SchemaBaseRef: public std::unique_ptr<class SchemaBase, SchemaBaseDeleter> {
    public:
        using std::unique_ptr<class SchemaBase, SchemaBaseDeleter>::unique_ptr;
        SchemaBaseRef() = default;
        SchemaBaseRef (const SchemaBaseRef &);
        SchemaBaseRef &operator =(const SchemaBaseRef &);
    };

    using SchemaRef = std::shared_ptr <class Schema>;

    /// Schema for one JSON object.
    class Schema {
        friend class OptionSchema;
    public:
        using OptionSchemas = std::unordered_map <class OptionParser *, SchemaRef>;
        using Dependencies = std::set <std::string>;
        static const Dependencies NoDependencies;

    private:
        SchemaBaseRef schema;
        void integrateSchema (const int, const class DictionarySchema &);
        std::ostream &dump (std::ostream &target, int indent, bool suppress_indent) const;

        Schema (SchemaBase *from);
    public:
        Schema () = default;
        Schema (const class OptionParser &from, const OptionSchemas &option_schemas);
        Schema (Schema &&) = default;
        Schema &operator =(Schema &&) = default;
        Schema (const Parsnip::Data &schema_spec);

        void addMember (const char *name, const SchemaBase &new_schema, bool mandatory = false,
                        const Dependencies &dependencies = NoDependencies);
        void replaceMember (const char *name, const SchemaBase &new_schema);
        void removeMember (const char *name);

        void validate (const Data &) const;
        std::ostream &dump (const std::string &intro, std::ostream &target = std::clog) const;
    };

}  // namespace Parsnip
