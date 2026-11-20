///
/// Collections for music thingie types.
/// @file       retainedlist.h - pianod
/// @author     Perette Barella
/// @date       2021-03-29
/// @copyright  Copyright (c) 2021 Devious Fish. All rights reserved.
///

#pragma once

#include <vector>
#include <type_traits>

#include "musictypes.h"
#include "retainer.h"

/** Base class for storing lists of thingies, which need to be
    reference counted accurately.  All operations automatically
    retain & release the objects stored herein. */
class ThingieList : public std::vector<MusicThingie *> {
    using list_type = std::vector<MusicThingie *>;

public:
    ~ThingieList();
    ThingieList() = default;
    ThingieList (const ThingieList &list);
    ThingieList (ThingieList &&list);
    ThingieList &operator= (const ThingieList &list);
    ThingieList &operator= (ThingieList &&list);

    // Get rid of forms we will not use
    template <class... Args>
    iterator emplace (const_iterator pos, Args &&...args) = delete;
    template <class... Args>
    void emplace_back (Args &&...args) = delete;
    iterator insert (const_iterator pos, std::initializer_list<MusicThingie *>) = delete;

    /// Replace all the usual methods with ones that manage object retention.
    void clear (void);
    iterator erase (const_iterator target);
    iterator erase (const_iterator first, const_iterator last);
    iterator insert (const_iterator where, MusicThingie *const &add);
    iterator insert (const_iterator where, MusicThingie *&&add);
    template <class InputIterator>
    iterator insert (const_iterator where, InputIterator first, InputIterator last) {
        difference_type offset = where - cbegin();
        for (const_iterator insloc = where; first != last; insloc++, first++) {
            insert (insloc, *first);
        }
        return begin() + offset;
    }

    void push_back (MusicThingie *add);
    template <typename InsertionType>
    void push_back (const Retainer<InsertionType> &add) {
        push_back (add.get());
    }

    void push_front (MusicThingie *add);
    template <typename InsertionType>
    inline void push_front (const Retainer<InsertionType> &add) {
        push_front (add.get());
    }

    void pop_back (void);
    void pop_front (void);
    void resize (size_type) = delete;
    // Added stuff
    void join (const ThingieList &from);
    void join (ThingieList &&from);
    void limitTo (MusicThingie::Type type);
    bool purge (const Media::Source *const source);
};

/** Container class for storing reference-counted music thingies.
    This is a thin wrapper around ThingieList that uses that class to do all the work,
    but can restrict inserted data to a subclass, and automatically cast retrieved data
    to that subclass, providing type safety. */
template <typename ActualType, class ValueType = typename std::remove_pointer<ActualType>::type>
class RetainedList : public ThingieList {
    static_assert (std::is_pointer<ActualType>::value, "First template parameter must be pointer type");
    static_assert (std::is_base_of<MusicThingie, ValueType>::value, "Can only store MusicThingie and descendents.");
    using this_type = RetainedList<ActualType, ValueType>;

public:
    using music_type = ValueType;
    using ThingieList::size_type;
    using ThingieList::empty;
    using ThingieList::size;
    using ThingieList::reserve;

    /// Default constructor
    inline RetainedList() = default;

    /// Copy construct from list of same type.
    inline RetainedList (const this_type &list) : ThingieList (*static_cast<const ThingieList *> (&list)) {
    }

    /// Move construct from a list of the same type.
    inline RetainedList (this_type &&list) : ThingieList (std::move (*static_cast<ThingieList *> (&list))){};

    /// Copy assign from a list of the same type.
    inline this_type &operator= (const this_type &list) {
        ThingieList::operator= (*static_cast<const ThingieList *> (&list));
        return *this;
    }

    /// Move assign from a list of the same type.
    inline this_type &operator= (this_type &&list) {
        ThingieList::operator= (std::move (*static_cast<ThingieList *> (&list)));
        return *this;
    }

    /// Copy construct from another RetainedList whose value type is a subclass of our value type.
    template <typename FromPointerType,
              class FromValueType = typename std::remove_pointer<FromPointerType>::type,
              typename = typename std::enable_if<std::is_base_of<ValueType, FromValueType>::value>::type>
    inline RetainedList (const RetainedList<FromPointerType> &list)
    : ThingieList (*static_cast<const ThingieList *> (&list)) {
    }

    /// Move construct from another RetainedList whose value type is a subclass of our value type.
    template <typename FromPointerType,
              class FromValueType = typename std::remove_pointer<FromPointerType>::type,
              typename = typename std::enable_if<std::is_base_of<ValueType, FromValueType>::value>::type>
    inline RetainedList (RetainedList<FromPointerType> &&list)
    : ThingieList (std::move (*static_cast<ThingieList *> (&list))){};

    /// Copy assign from another RetainedList whose value type is a subclass of our value type.
    template <typename FromPointerType,
              class FromValueType = typename std::remove_pointer<FromPointerType>::type,
              typename = typename std::enable_if<std::is_base_of<ValueType, FromValueType>::value>::type>
    inline ThingieList &operator= (const RetainedList<FromPointerType> &list) {
        ThingieList::operator= (*static_cast<const ThingieList *> (&list));
        return *this;
    }

    /// Move assign from another RetainedList whose value type is a subclass of our value type.
    template <typename FromPointerType,
              class FromValueType = typename std::remove_pointer<FromPointerType>::type,
              typename = typename std::enable_if<std::is_base_of<ValueType, FromValueType>::value>::type>
    inline ThingieList &operator= (const RetainedList<FromPointerType> &&list) {
        ThingieList::operator= (std::move (*static_cast<ThingieList *> (&list)));
        return *this;
    }

    /// Iterators: bastardize ThingieList's iterator's dereference operators to return known type.
    class iterator : public ThingieList::iterator {
    public:
        inline ActualType operator*() const noexcept {
            return static_cast<ActualType> (ThingieList::iterator::operator*());
        }
    };
    class const_iterator : public ThingieList::const_iterator {
    public:
        using ThingieList::const_iterator::const_iterator;
        inline const_iterator (const iterator &from)
        : ThingieList::const_iterator (*static_cast<const ThingieList::iterator *> (&from)){};

        inline ActualType operator*() const noexcept {
            return static_cast<const ActualType> (ThingieList::const_iterator::operator*());
        }
    };
    class reverse_iterator : public ThingieList::reverse_iterator {
    public:
        inline ActualType operator*() const noexcept {
            return static_cast<ActualType> (ThingieList::reverse_iterator::operator*());
        }
    };
    class const_reverse_iterator : public ThingieList::const_reverse_iterator {
    public:
        inline ActualType operator*() const noexcept {
            return static_cast<const ActualType> (ThingieList::const_reverse_iterator::operator*());
        }
    };

    /// Iterators: Return our iterators with correct dereference operator instead of the ThingieList ones.
    inline iterator begin() noexcept {
        ThingieList::iterator temp = ThingieList::begin();
        return *static_cast<iterator *> (&temp);
    }
    inline const_iterator begin() const noexcept {
        ThingieList::const_iterator temp = ThingieList::begin();
        return *static_cast<const_iterator *> (&temp);
    }
    inline const_iterator cbegin() const noexcept {
        ThingieList::const_iterator temp = ThingieList::cbegin();
        return *static_cast<const_iterator *> (&temp);
    }
    inline iterator end() noexcept {
        ThingieList::iterator temp = ThingieList::end();
        return *static_cast<iterator *> (&temp);
    }
    inline const_iterator end() const noexcept {
        ThingieList::const_iterator temp = ThingieList::end();
        return *static_cast<const_iterator *> (&temp);
    }
    inline const_iterator cend() const noexcept {
        ThingieList::const_iterator temp = ThingieList::cend();
        return *static_cast<const_iterator *> (&temp);
    }

    inline reverse_iterator rbegin() noexcept {
        ThingieList::reverse_iterator temp = ThingieList::rbegin();
        return *static_cast<reverse_iterator *> (&temp);
    }
    inline const_reverse_iterator rbegin() const noexcept {
        ThingieList::const_reverse_iterator temp = ThingieList::rbegin();
        return *static_cast<const_reverse_iterator *> (&temp);
    }
    inline const_reverse_iterator crbegin() const noexcept {
        ThingieList::const_reverse_iterator temp = ThingieList::crbegin();
        return *static_cast<const_reverse_iterator *> (&temp);
    }
    inline reverse_iterator rend() noexcept {
        ThingieList::reverse_iterator temp = ThingieList::rend();
        return *static_cast<reverse_iterator *> (&temp);
    }
    inline const_reverse_iterator rend() const noexcept {
        ThingieList::const_reverse_iterator temp = ThingieList::rend();
        return *static_cast<const_reverse_iterator *> (&temp);
    }
    inline const_reverse_iterator crend() const noexcept {
        ThingieList::const_reverse_iterator temp = ThingieList::crend();
        return *static_cast<const_reverse_iterator *> (&temp);
    }

    /// Member accessors: return data as known type, instead of ThingieList.
    inline ActualType operator[] (size_type item) {
        return static_cast<ActualType> (ThingieList::operator[] (item));
    }
    inline ActualType const &operator[] (size_type item) const {
        MusicThingie *const *temp = &ThingieList::operator[] (item);
        return *reinterpret_cast<ActualType const *> (temp);
    }
    inline ActualType &front() {
        MusicThingie **temp = &ThingieList::front();
        return *reinterpret_cast<ActualType *> (temp);
    }
    inline const ActualType &front() const {
        MusicThingie *const *temp = &ThingieList::front();
        return *reinterpret_cast<ActualType const *> (temp);
    }
    inline ActualType &back() {
        MusicThingie **temp = &ThingieList::back();
        return *reinterpret_cast<ActualType *> (temp);
    }
    inline const ActualType &back() const {
        MusicThingie *const *temp = &ThingieList::back();
        return *reinterpret_cast<ActualType const *> (temp);
    }

    // Correct returned iterator type.
    inline iterator erase (const_iterator target) {
        ThingieList::iterator temp = ThingieList::erase (target);
        return *static_cast<iterator *> (&temp);
    }
    iterator erase (const_iterator first, const_iterator last) {
        ThingieList::iterator temp = ThingieList::erase (first, last);
        return *static_cast<iterator *> (&temp);
    }

    // Override push_* functions, using our type as the parameter to prevent unwanted cruft getting in.
    inline void push_back (ActualType add) {
        ThingieList::push_back (add);
    }
    template <typename InsertionType>
    inline void push_back (const Retainer<InsertionType> &add) {
        push_back (add.get());
    }
    inline void push_front (ActualType add) {
        ThingieList::push_front (add);
    }
    template <typename InsertionType>
    inline void push_front (const Retainer<InsertionType> &add) {
        push_front (add.get());
    }

    iterator insert (const_iterator where, const ActualType add) {
        ThingieList::iterator temp = ThingieList::insert (where, add);
        return *static_cast<iterator *> (&temp);
    }

    template <class InputIterator>
    iterator insert (const_iterator where, InputIterator first, InputIterator last) {
        for (const_iterator insloc = where; first != last; insloc++, first++) {
            insert (insloc, *first);
        }
        return begin() + (where - cbegin());
    }

    inline void join (const this_type &from) {
        ThingieList::join (*static_cast<const ThingieList *> (&from));
    }
    inline void join (this_type &&from) {
        ThingieList::join (std::move (*static_cast<ThingieList *> (&from)));
    }
};

/** static typecast for thingie lists:
    Typecasts a list of a given type to a list of a derived type.
    @tparam ToValueType The 
    @warning Like all typecasts, this is a footgun. */
template <typename ToValueType, typename FromListType>
constexpr const RetainedList<ToValueType> &retained_list_cast (FromListType &from) {
    static_assert (
            std::is_base_of<typename FromListType::music_type, typename std::remove_pointer<ToValueType>::type>::value,
            "List element types are not related.");
    return *reinterpret_cast<RetainedList<ToValueType> *> (&from);
}

/** static typecast for constant thingie lists:
    Typecasts a consetant list of a given type to a constant list of a derived type.
    @warning Like all typecasts, this is a footgun. */
template <typename ToValueType, typename FromListType>
constexpr const RetainedList<ToValueType> &retained_list_cast (const FromListType &from) {
    static_assert (
            std::is_base_of<typename FromListType::music_type, typename std::remove_pointer<ToValueType>::type>::value,
            "List element types are not related.");
    return *reinterpret_cast<const RetainedList<ToValueType> *> (&from);
}

/// A container for lists of playlists
class PlaylistList : public RetainedList<PianodPlaylist *> {};

/** A container for holding songs.  Used for queues, history,
    some search results and other cases where only songs apply. */
class SongList : public RetainedList<PianodSong *> {
public:
    using RetainedList::RetainedList;
    void mixedMerge (const SongList &adds);
};
