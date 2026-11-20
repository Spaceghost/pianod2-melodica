///
/// Smart pointers for music thingie types.
/// @file       retainer.h - pianod
/// @author     Perette Barella
/// @date       2020-03-19
/// @copyright  Copyright (c) 2020 Devious Fish. All rights reserved.
///

#pragma once

/** Smart containers to help manage reference counting the MusicThingies. */
template <typename ptr_type>
class Retainer {
    static_assert (std::is_pointer <ptr_type>::value, "Retained type must be a pointer.");
    using value_type = typename std::remove_pointer <ptr_type>::type;
    static_assert (std::is_base_of <MusicThingie, value_type>::value, "Retained value type must derive from MusicThingie");
    using this_type = Retainer <ptr_type>;
private:
    value_type *value {nullptr};
public:
    /// Default constructor.
    inline Retainer () {};
    
    /// Direct construction of new item via pointer.
    inline Retainer (value_type *item) : value (item) {
        if (value) {
            value->retain();
        }
    }
    
    /// Copy constructor
    inline Retainer (const this_type &item) : value (item.value) {
        if (value) {
            value->retain();
        }
    }
    
    /// Copy construct from a different retained type
    template <typename OtherType>
    inline Retainer (const Retainer<OtherType> &item) : value (item.get()) {
        if (value) {
            value->retain();
        }
    }

    /// Move construction.
    inline Retainer (this_type &&item) : value (item.value) {
        item.value = nullptr;
    }
        
    /// Move construction from a different retained type.
    template <typename OtherType>
    inline Retainer (Retainer <OtherType> &&item) : value (item.value) {
        item.value = nullptr;
    }
    
    /// Direct assignment of new pointer.
    inline this_type &operator= (value_type *item) {
        if (value != item) {
            if (value) {
                value->release();
            }
            value = item;
            if (value) {
                value->retain();
            }
        }
        return *this;
    }
    
    /// Copy assignment from another retainer of same type.
    inline value_type &operator= (const this_type &item) {
        *this = item.value;
        return *this;
    }

    /// Copy assignment from a retainer of a different type.
    template <typename OtherType>
    inline this_type &operator= (const Retainer <OtherType> &item) {
        *this = item.value;
        return *this;
    }

    /// Move assignment from another retainer of same type.
    inline this_type &operator= (this_type &&item) {
        if (value != item.value) {
            if (value) {
                value->release();
            }
            value = item.value;
            item.value = nullptr;
        }
        return *this;
    }

    /// Move assignment from another retainer of a different type.
    template <typename OtherType>
    inline this_type &operator= (Retainer <OtherType> &&item) {
        if (value != item.value) {
            if (value) {
                value->release();
            }
            value = item.value;
            item.value = nullptr;
        }
        return *this;
    }

    /// On destruction, release any contained pointer.
    inline ~Retainer () {
        if (value) {
            value->release();
        }
    }

    /// Allow arrow use of contained pointer.
    inline value_type *operator->() const {
        assert (value);
        return value;
    }

    /// Dereference contained pointer via operator *.
    inline value_type &operator*() const {
        return *value;
    }

    /// Get the pointer itself.
    inline value_type *get() const {
        return value;
    }

    /** Check for null.
        @return True if retainer contains non-null pointer. */
    inline operator bool() const {
        return (value != nullptr);
    }
};
