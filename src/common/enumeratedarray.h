///
/// Enumerated array type.
/// Array is defined by and access by an enumeration, including enum class,
/// rather than numeric values.
/// @file       enumeratedarray.h - pianod
/// @author     Perette Barella
/// @date       2016-04-01
/// @copyright  Copyright (c) 2016, 2020 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <cassert>

#include <type_traits>

/** An array indexed by an enumeration rather than an integer of some sort.
    @warning Underlying this template is a plain-old array.  Keep in mind:
    - Reused enumeration values will collide.
    - Sparse enumerations will allocate space for unused values.
    @tparam Enumeration The enumeration type to use as array index.
    @tparam ValueType The value type of which the array is composed.
    @tparam size_element An element representing the number of values, i.e.,
    one larger than the last meaningful slot.
    The default is a member of the enumeration named "Count".
    @tparam enum_size The size of the array.  Like normal arrays, the largest
    index is one less than this item. */
template <typename Enumeration, typename ValueType,
          Enumeration size_element = Enumeration::Count,
          int enum_size = static_cast <int> (size_element)>
class EnumeratedArray {
    using needs_initialization = std::conditional <std::is_pod<ValueType>::value,std::true_type,std::false_type>;
    ValueType underlying [enum_size];
public:
    using value_type = ValueType;
    using enumeration_type = Enumeration;
    /// Initialize an enumerated array.  Set values to value type's default
    EnumeratedArray () {
        /* If the value type is a class, it will initialize itself.  Use template
           magic to avoid reinitializing it, but do initialize other data types. */
        if (needs_initialization::type::value) {
            for (int i = 0; i < enum_size; i++) {
                underlying [i] = ValueType {};
            }
        }
    }
    /// Initialize an enumerated array.  Set values to a specific initial value.
    explicit EnumeratedArray (ValueType initial) {
        for (int i = 0; i < enum_size; i++) {
            underlying [i] = initial;
        }
    }
    /// Access an element of an enumerated array.
    inline ValueType &operator[] (const Enumeration index) noexcept {
        assert (static_cast <int> (index) >= 0 && static_cast <int> (index) < enum_size);
        return underlying [static_cast <const int> (index)];
    }
    /// Access an element of a constant enumerated array.
    inline const ValueType &operator[] (const Enumeration index) const noexcept {
        assert (static_cast <int> (index) >= 0 && static_cast <int> (index) < enum_size);
        return underlying [static_cast <const int> (index)];
    }

    template <class IteratedType>
    class eaiterator {
        friend class EnumeratedArray;
        IteratedType *start = nullptr;
        IteratedType *item = nullptr;
    public:
        inline eaiterator (IteratedType *s, IteratedType *i) noexcept: start (s), item (i) {};
    public:
        eaiterator() noexcept = default;
        eaiterator (const eaiterator<IteratedType> &) noexcept = default;
        eaiterator &operator=(const eaiterator<IteratedType> &) noexcept = default;
        inline IteratedType &operator*() const noexcept {
            assert (item);
            return *item;
        }
        inline IteratedType *operator->() const noexcept {
            assert (item);
            return item;
        }
        inline eaiterator<IteratedType> &operator++ () noexcept {
            assert (item);
            item++;
            return *this;
        }
        inline eaiterator<IteratedType> operator++ (int) noexcept {
            assert (item);
            eaiterator<IteratedType> prior = *this;
            item++;
            return prior;
        }
        inline bool operator!=(const eaiterator<IteratedType> &compare) const noexcept {
            assert (item);
            return item != compare.item;
        }
        inline bool operator==(const eaiterator<IteratedType> &compare) const noexcept {
            assert (item);
            return item == compare.item;
        }
        /// Return the enumeration value for the current element.
        inline Enumeration key() const noexcept {
            return static_cast <Enumeration> (item - start);
        }
    };
    using iterator = eaiterator<ValueType>;
    using const_iterator = eaiterator <const ValueType>;
    
    inline iterator begin () noexcept {
        return iterator (underlying, &underlying [0]);
    }
    inline iterator end () noexcept {
        return iterator (underlying, &underlying [enum_size]);
    }

    inline const_iterator begin () const noexcept {
        return const_iterator (&underlying [0], &underlying [0]);
    }
    inline const_iterator end () const noexcept {
        return const_iterator (&underlying [0], &underlying [enum_size]);
    }
};

