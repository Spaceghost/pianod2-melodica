///
/// Collections for music thingie types.
/// @file       retainedlist.cpp - pianod
/// @author     Perette Barella
/// @date       2021-03-29
/// @copyright  Copyright (c) 2021 Devious Fish. All rights reserved.
///

#include <vector>
#include <algorithm>

#include "musictypes.h"
#include "retainer.h"
#include "retainedlist.h"

/// Release items when being destroyed.
ThingieList::~ThingieList (void) {
    clear();
}

/** Copy-construct items into this list.  Let the base constructor
    deal with copying, so it optimizes for efficiency; afterward we
    just need to increment the reference counts.
    @param list The list to construct with. */
ThingieList::ThingieList (const ThingieList &list) : list_type (list) {
    // Copy of elements done by initializer
    for (MusicThingie *item : *this) {
        item->retain();
    }
};

/** Move-construct items into this list.
    @param list The list to construct with. */
ThingieList::ThingieList (ThingieList &&list) : list_type (std::move (list)) {
    assert (list.empty());
};

/** Copy assign into the list.
    @param list The list to construct from. */
ThingieList &ThingieList::operator= (const ThingieList &list) {
    if (this != &list) {
        clear();
        std::copy (list.begin(), list.end(), back_inserter (*this));
    }
    return *this;
};

/** Move assign into the list.
    @param list The list to move from. */
ThingieList &ThingieList::operator= (ThingieList &&list) {
    if (this != &list) {
        swap (list);
    }
    return *this;
};

/// Clear out the contents
void ThingieList::clear() {
    while (!empty()) {
        pop_back();
    }
}

ThingieList::iterator ThingieList::erase (const_iterator target) {
    (*target)->release();
    return list_type::erase (target);
}

ThingieList::iterator ThingieList::erase(const_iterator first, const_iterator last) {
    for (const_iterator it = first; it != last; it++) {
        (*it)->release();
    }
    return list_type::erase (first, last);
}


ThingieList::iterator ThingieList::insert (const_iterator where, MusicThingie *const &add) {
    auto ret = list_type::insert (where, add);
    add->retain();
    return ret;
}

//ThingieList::iterator ThingieList::insert (const_iterator where, MusicThingie *&&add) {
//    auto ret = list_type::insert (where, std::move (add));
//    assert (!add);
//    return ret;
//}

void ThingieList::push_back (MusicThingie *add) {
    list_type::push_back (add);
    add->retain();
}

void ThingieList::push_front (MusicThingie *add) {
    insert (begin(), add);
}

void ThingieList::pop_back (void) {
    assert (!empty());
    back()->release();
    list_type::pop_back();
}

void ThingieList::pop_front (void) {
    assert (!empty());
    erase (begin());
}

/** Append another thingielist to this one.
    @param from The list to append. */
void ThingieList::join (const ThingieList &from) {
    reserve (size() + from.size());
    std::copy (from.begin(), from.end(), std::back_inserter (*this));
}

/** Append another thingielist to this one.
    @param from The list to append. */
void ThingieList::join (ThingieList &&from) {
    reserve (size() + from.size());
    list_type::insert (end(), from.begin(), from.end());
    from.list_type::clear();
}

/** Purge any items from a particular source from the list.
    @param source The source whose items to remove. */
bool ThingieList::purge (const Media::Source *const source) {
    bool changed = false;
    for (iterator it = begin(); it != end();) {
        if (source == (*it)->source()) {
            it = erase (it);
            changed = true;
        } else {
            it++;
        }
    }
    return changed;
}

/** Purge anything that isn't a certain type */
void ThingieList::limitTo (MusicThingie::Type type) {
    ThingieList new_list;
    for (MusicThingie *it : *this) {
        if (type == it->primaryType()) {
            new_list.push_back (it);
        }
    }
    swap (new_list);
}

/** Randomly merge a songlist into another.
    Randomly mix playlists together, but maintain sequence of each
    to accommodate sources like Pandora that (ostensibly) care about
    sequence. */
void SongList::mixedMerge (const SongList &adds) {
    SongList originals;
    swap (originals);
    reserve (originals.size() + adds.size());

    size_type orig_index = 0;
    size_type add_index = 0;
    for (SongList::size_type songs = originals.size() + adds.size(); songs; songs--) {
        SongList::size_type item = random() % songs;
        if (item < (originals.size() - orig_index)) {
            push_back (originals [orig_index++]);
        } else {
            push_back (adds [add_index++]);
        }
    }
}
