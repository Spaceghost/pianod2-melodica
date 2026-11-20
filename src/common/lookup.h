///
/// Lookup tables.
/// Lookup table template and common definitions.
/// @file       lookup.h - pianod2
/// @author     Perette Barella
/// @date       2015-10-11
/// @copyright  Copyright (c) 2015-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <cassert>

#include <string>
#include <initializer_list>

#include "fundamentals.h"

/// A standard form of enumeration to text value lookup tables.
template <class EnumClass> class StandardLookupValues {
public:
    const char *name;
    EnumClass value;
};

/// Class to map between enumerations and their text values.
template <class EnumClass,
class LookupTableType = StandardLookupValues <EnumClass> > class LookupTable {
    const LookupTableType *table = NULL;
    const int table_size = 0;
    bool release = false; // True when table was allocated from free store.
public:
    LookupTable () = delete;
    LookupTable (const LookupTable <EnumClass, LookupTableType> &) = delete;
    LookupTable (const LookupTable <EnumClass, LookupTableType> &&) = delete;
    /** Initialize the lookup table from an array.  Requires a NULL entry at the end. */
    LookupTable (const LookupTableType *tab) {
        table = tab;
        int i = 0;
        for (i = 0; tab [i].name; i++) /* just looking */;
        *const_cast <int *> (&this->table_size) = i;
    }
    /** Initialize the lookup table from an initializer list.  No NULL entry needed. */
    LookupTable (const std::initializer_list<LookupTableType> tab) :
    table_size (tab.size()) {
        LookupTableType *iniz = new LookupTableType [tab.size() + 1];
        int i = 0;
        for (auto item : tab) {
            assert (item.name);
            iniz [i].name = item.name;
            iniz [i++].value = item.value;
        }
        iniz [i].name = NULL;
        table = iniz;
        release = true;
    }
    ~LookupTable () {
        if (release) {
            delete[] table;
        }
    };


    /** Retrieve a table entry by the text name of an entry.
        @param s the text name
        @return Pointer to table entry, or NULL if not found. */
    virtual const LookupTableType *get (const char *s) const noexcept {
        for (const LookupTableType *l = table; l->name != NULL; l++) {
            if (strcasecmp (s, l->name) == 0) {
                return l;
            }
        }
        return NULL;
    }

    /** Retrieve the enumerator corresponding to text name.
        @throw CommandError if the text name is invalid.
        @param s The text name
        @return Enumerator corresponding to text. */
    const EnumClass operator[] (const char *s) const {
        const LookupTableType *item = get (s);
        if (!item) {
            throw CommandError (E_INVALID, s);
        }
        return item->value;
    }

    /** Retrieve the enumerator corresponding to text name.
        @throw CommandError if the text name is invalid.
        @param s The text name
        @return Enumerator corresponding to text. */
    inline const EnumClass operator[] (const std::string &s) const {
        return (*this) [s.c_str()];
    }
    /** Retrieve the enumerator corresponding to a text value.
        @param s The text name
        @param value The destination for the enumerator.
        Existing value is unchanged if text name is invalid.
        @return true if successful, false if text name was invalid. */
    const bool tryGetValue (const char *s, EnumClass *value) const {
        const LookupTableType *item = get (s);
        if (!item) return false;
        *value = item->value;
        return true;
    }
    inline const bool tryGetValue (const std::string &s, EnumClass *value) const {
        return tryGetValue (s.c_str(), value);
    }

    /** Retrieve a table entry by the enumerator of an entry.
        @param id The enumerator
        @return Pointer to table entry, or NULL if not found. */
    const LookupTableType *get (const EnumClass id) const {
        for (const LookupTableType *l = table; l->name != NULL; l++) {
            if (id == l->value) {
                return l;
            }
        }
        assert (0);
        return NULL;
    }
    /** Retrieve the text name of an enumerator.
        @param id The enumerator
        @return The text name, or NULL if not found. */
    const char *operator[] (const EnumClass id) const {
        const LookupTableType *item = get (id);
        if (!item) return nullptr;
        return item->name;
    }

    const LookupTableType *begin () const {
        return &table [0];
    }

    const LookupTableType *end () const {
        return &table [table_size];
    }
};

