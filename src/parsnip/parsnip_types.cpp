///
/// Parship serialization.
/// @file       parsnip_types.cpp - A flexible data type for serialization and parsing.
/// @author     Perette Barella
/// @date       2019-05-01
/// @copyright  Copyright 2019-2021 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cctype>
#include <cstdlib>
#include <climits>

#include "parsnip.h"

namespace Parsnip {

    extern const char hex_digits[] = "0123456789abcdef";

    /** Insert a new portion of the exception location at the front.
        @param where The location portion. */
    void Exception::addBacktraceLocation (const std::string &where) {
        location.insert (0, "/" + where);
    }
    
    /** @internal Validate that the data type is that expected.
        @param type The expected type.
        @throws IncorrectDataType if the type is not as expected. */
    void Data::mandateType (Type type) const {
        if (datatype != type) {
            throw IncorrectDataType (type, datatype);
        }
    }

    /** IF a data item is a string, change it to a flexible string.
        Use of this function should be very rare; strings should be strings
        and numbers numbers.  But if a number gets stored in a string,
        this allows it to be accessed until it can be fixed (and it
        tolerates fixing, too). */
    const Data &Data::makeFlexible () const {
        if (datatype == Type::String) {
            const_cast <Data *> (this)->datatype = Type::FlexibleString;
        }
        return *this;
    }

    /** @internal Dictionary/List constructor/helper.
        Initializes the dictionary or list member in the data's union
        and sets the datatype.
        @param kind Indicates what to construct. */
    Data::Data (Type kind) {
        switch (kind) {
            case Type::Dictionary:
                data.dictionary = new DictionaryType;
                break;
            case Type::List:
                data.list = new ListType;
                break;
            default:
                assert (!"Can not initialize type this way.");
                break;
        }
        datatype = kind;
    }

    /// Copy constructor
    Data::Data (const Data &from) {
        switch (from.datatype) {
            case Type::List:
                data.list = new ListType{*(from.data.list)};
                break;
            case Type::Dictionary:
                data.dictionary = new DictionaryType{*(from.data.dictionary)};
                break;
            case Type::String:
            case Type::FlexibleString:
                data.str = new StringType{*(from.data.str)};
                break;
            default:
                data = from.data;
                break;
        }
        datatype = from.datatype;
    }

    /// Copy assignment
    Data &Data::operator= (const Data &from) {
        if (this != &from) {
            release();
            switch (from.datatype) {
                case Type::List:
                    data.list = new ListType{*(from.data.list)};
                    break;
                case Type::Dictionary:
                    data.dictionary = new DictionaryType{*(from.data.dictionary)};
                    break;
                case Type::String:
                case Type::FlexibleString:
                    data.str = new StringType{*(from.data.str)};
                    break;
                default:
                    data = from.data;
                    break;
            }
            datatype = from.datatype;
        }
        return *this;
    }

    /// Release any stored data and restore to an "empty" state.
    void Data::release() {
        switch (datatype) {
            case Type::Dictionary:
                delete (data.dictionary);
                break;
            case Type::List:
                delete (data.list);
                break;
            case Type::String:
            case Type::FlexibleString:
                delete (data.str);
                break;
            default:
                // Nothing to do.
                break;
        }
        datatype = Type::Null;
    }
    /** Retrieve string value.
        @return String value.
        @throws IncorrectDataType if value is not a string. */
    const Data::StringType &Data::asString() const {
        if (datatype != Type::String && datatype != Type::FlexibleString) {
            throw IncorrectDataType (Type::String, datatype);
        }
        return *(data.str);
    }

    /** Compare two structures.
        @param compare The structure to compare to.
        @return True if identical, false otherwise. */
    bool Data::operator== (const Data &compare) const {
        if (datatype != compare.datatype) {
            return false;
        }
        switch (datatype) {
            case Type::Null:
                return true;
            case Type::String:
            case Type::FlexibleString:
                return *(data.str) == *(compare.data.str);
            case Type::Integer:
                return data.integer == compare.data.integer;
            case Type::Real:
                return data.real == compare.data.real;
            case Type::Boolean:
                return data.boolean == compare.data.boolean;

            case Type::Dictionary: {
                if (data.dictionary->size() != compare.data.dictionary->size()) {
                    return false;
                }
                for (const auto &item : *(data.dictionary)) {
                    auto it = compare.data.dictionary->find (item.first);
                    if (it == compare.data.dictionary->end() || item.second != it->second) {
                        return false;
                    }
                }
                return true;
            }
            case Type::List: {
                if (size() != compare.size()) {
                    return false;
                }
                for (auto myit = data.list->begin(), theirit = compare.data.list->begin();
                     myit != data.list->end(); myit++, theirit++) {
                    if (*myit != *theirit) {
                        return false;
                    }
                }
                return true;
            }
        }
        assert (!"Unreachable.");
    }

    /// Return value of data as a double.  If the data is a string, parse it as a float.
    double Data::asDouble() const {
        double result;
        switch (datatype) {
            case Type::Real:
                result = data.real;
                break;
            case Type::Integer:
                result = data.integer;
                break;
            case Type::FlexibleString: {
                errno = 0;
                char *err;
                result = strtod (data.str->c_str(), &err);
                if (data.str->empty() || *err != '\0' || errno == EINVAL) {
                    throw DataFormatError (data.str->c_str());
                }
                break;
            }
            default:
                throw IncorrectDataType (Type::Real, datatype);
        }
        if (result == HUGE_VAL || result == -HUGE_VAL) {
            throw DataRangeError();
        }
        return result;
    };

    /// Return value of data as a boolean.  If the data is a string, translate "true" and "false".
    bool Data::asBoolean() const {
        switch (datatype) {
            case Type::Boolean:
                return data.boolean;
            case Type::FlexibleString: {
                const char *text = data.str->c_str();
                if (strcasecmp (text, "true") != 0 && strcasecmp (text, "false") != 0) {
                    throw DataFormatError (data.str->c_str());
                }
                return (strcasecmp (text, "true") == 0);
            }
            default:
                throw IncorrectDataType (Type::Boolean, datatype);
        }
    };

    /** Return value of data as a long integer.
        If the data is a string, parse it as an integer.
        @param base The base for interpreting strings.  Default is 10.
        @return The long integer.
        @throws DataRangeError if the value is out of representable range. */
    long Data::asLong (int base) const {
        long result;
        switch (datatype) {
            case Type::Integer:
                result = data.integer;
                break;
            case Type::FlexibleString: {
                errno = 0;
                char *err;
                result = strtol (data.str->c_str(), &err, base);
                if (data.str->empty() || *err != '\0' || errno == EINVAL) {
                    throw DataFormatError (data.str->c_str());
                }
                break;
            }
            default:
                throw IncorrectDataType (Type::Integer, datatype);
        }
        if (result == LONG_MIN || result == LONG_MAX) {
            throw DataRangeError();
        }
        return result;
    };

    /** Return value of data as an integer.
        @see asLong().
        @param base The base for interpreting strings.  Default is 10.
        @return The integer.
        @throws DataRangeError if the value is out of representable range. */
    int Data::asInteger (int base) const {
        long result = asLong (base);
        if (result < INT_MIN || result > LONG_MAX) {
            throw DataRangeError();
        }
        return result;
    };

    /** Return data as a list.
       - Null data is returned as an empty list.
       - Lists return a copy of their elements.
       - Individual elements are wrapped in a 1-element list.
       @throws IncorrectDataType for dictionaries. */
    Data::ListType Data::asList() const {
        switch (datatype) {
            case Type::Null:
                return ListType{};
            case Type::Dictionary:
                throw IncorrectDataType (Type::List, Type::Dictionary);
            case Type::List:
                return *(this->data.list);
            default: {
                // Return a list with the one item in it.
                ListType list;
                list.push_back (*this);
                return list;
            }
        }
    }

    /** Dictionary member accessor.
        If dictionary is constant, avoid automatic insertion,
        and throw error if key is not present in dictionary. */
    const Data &Data::operator[] (const std::string &key) const {
        mandateType (Type::Dictionary);
        auto result = data.dictionary->find (key);
        if (result == data.dictionary->end())
            throw NoSuchKey (key);
        return result->second;
    }

    /** Dictionary member accessor.
        If dictionary is NOT constant, accessing non-existent
        key creates it. */
    Data &Data::operator[] (const std::string &key) {
        mandateType (Type::Dictionary);
        return (*(data.dictionary))[key];
    }
    
    /** Remove key from dictionary. */
    void Data::remove (const std::string &key) {
        mandateType (Type::Dictionary);
        auto it = data.dictionary->find (key);
        if (it != data.dictionary->end()) {
            data.dictionary->erase (it);
        }
    }

    /** Check if a key exists in a dictionary.
        @param key The key to check.
        @return True if present in dictionary, false otherwise. */
    bool Data::contains (const std::string &key) const {
        mandateType (Type::Dictionary);
        return (data.dictionary->find (key) != data.dictionary->end());
    }

    /** List member accessor.
        If list is constant, avoid automatic insertion,
        and throw error if index is out of range. */
    const Data &Data::operator[] (size_type index) const {
        mandateType (Type::List);
        if (index < 0 || index >= data.list->size()) {
            throw std::out_of_range ("index out of range: 0 <= " + std::to_string (index) + " < "
                                     + std::to_string (data.list->size()));
        }
        return (*(data.list))[index];
    }

    /** List member accessor.
        If list is NOT constant, accessing outside of range expands list size */
    Data &Data::operator[] (size_type index) {
        mandateType (Type::List);
        return (*(data.list))[index];
    }

    /** Remove entry from position in list. */
    void Data::remove (size_type index) {
        mandateType (Type::List);
        assert (index >= 0 && index < data.list->size());
        if (index >= 0 && index < data.list->size()) {
            auto it = data.list->begin() + index;
            data.list->erase (it);
        }
    }

    /// List back-inserter
    void Data::push_back (const Data &value) {
        mandateType (Type::List);
        data.list->push_back (value);
    }

    /// List back-inserter
    void Data::push_back (Data &&value) {
        mandateType (Type::List);
        data.list->push_back (std::move (value));
    }

    /** Retrieve number of entries in a list.
        @return Count of values in list.
        @throws IncorrectDataType if not a list. */
    Data::size_type Data::size() const {
        mandateType (Type::List);
        return data.list->size();
    }

    /** Return the name corresponding to a type.
        @param ty The type whose name to retrieve.
        @return The name of the type. */
    const char *Data::type_name (Type ty) {
        switch (ty) {
            case Type::Dictionary:
                return "Dictionary";
            case Type::List:
                return "List";
            case Type::String:
                return "String";
            case Type::FlexibleString:
                return "String/other";
            case Type::Integer:
                return "Integer";
            case Type::Real:
                return "Real";
            case Type::Boolean:
                return "Boolean";
            case Type::Null:
                return "Null";
        }
        return "Unknown type";
    }
}  // namespace Parsnip
