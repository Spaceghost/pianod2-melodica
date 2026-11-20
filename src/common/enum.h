///
/// Enumeration iterator.
/// A class that wraps enumerations and provides an iterator that ranges
/// over the enumeration's values.
///	@file      enum.h - common utility
///	@author    Perette Barella
///	@date      2017-09-17
///	@copyright Copyright (c) 2017, 2020 Devious Fish. All rights reserved.
///


#pragma once

#include <assert.h>

/** Iteratate over the values of the enumeration.
    @tparam Enumeration The enumeration type.
    @tparam end_element The stop element.  As with all iterators, this is an
    "extra" value at the end of the enumeration, one above the last meaningful
    value of the enumeration.  May default if named "Count". */
template <typename Enumeration, Enumeration end_element = Enumeration::Count>
class Enum {
    static const Enumeration begin_element = static_cast <Enumeration> (0);
    using enumeration_type = Enumeration;
public:
    class Iterator {
        enumeration_type value = begin_element;
    public:
        /// Create a new iterator, starting at value 0.
        inline Iterator () { };

        /// Create a new iterator, starting at a specified value.
        inline Iterator (enumeration_type initial_value) :
        value (initial_value)
        { }


        /// Get the current value of the iterator.
        inline enumeration_type operator* (void) const
        {
            return value;
        }

        /// Move to the successor item (prefix form).
        inline enumeration_type operator++ (void)
        {
            value = enumeration_type (int (value) + 1);
            assert (value <= end_element);
            return value;
        }

        /// Move to the successor item (suffix form).
        inline enumeration_type operator++ (int)
        {
            enumeration_type old_value = value;
            value = enumeration_type (int (value) + 1);
            assert (value <= end_element);
            return old_value;
        }

        /// Compare two iterators.
        inline bool operator!=( Iterator compare_to )
        {
            return value != compare_to.value;
        }

        /// Compare two iterators.
        inline bool operator==( Iterator compare_to )
        {
            return value == compare_to.value;
        }
    };

    /// Get a new iterator for the first item of the enumeration.
    inline static Iterator begin () {
        return Iterator ();
    }

    /// Get a new iterator for the terminator item of the enumeration.
    inline static Iterator end () {
        return Iterator (end_element);
    }

#ifndef NDEBUG
    inline bool isValid (Enumeration value) {
        return (value < end_element);
    }
#endif
    
};

