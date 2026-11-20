///
/// Template method implementations for music library hash tables.
/// @file       musiclibraryhash.h - pianod
/// @author     Perette Barella
/// @date       2014-12-09
/// @copyright  Copyright 2014-2021 Devious Fish.  All rights reserved.
///

#pragma once

#include <string>
#include <unordered_map>
#include <functional>

#include "utility.h"
#include "musiclibrary.h"

namespace MusicLibrary {

    /*
     *              Supporting functions
     */

    /** Use an item's type and a string key generate a id string.
        @return ID, consisting of a letter for the type, then a hashed value from the string. */
    inline std::string persistentId (MusicThingie::Type item_type, const std::string &key) {
        const char type_code[2] = {static_cast<char> (item_type), '\0'};
        return type_code + std::to_string (create_key_from_string (key));
    }



    /// Constructor: Construct a new ThingieContainer and assign it an allocator.
    template <class TThing, class TParent>
    ThingieContainer<TThing, TParent>::ThingieContainer (const Allocator &alloc)
    : allocate (alloc) {
    }

    /// Destructor: Release all contents prior to destruction.
    template <class TThing, class TParent>
    ThingieContainer<TThing, TParent>::~ThingieContainer() {
#ifndef NDEBUG
        for (auto &item : *this) {
            assert (item.second->getUseCount() == 1);
        }
#endif
        clear();
    }

    /** Remove items according to the predicate.
        @param pred Predicate, which returns true to indicate removal. */
    template <class TThing, class TParent>
    void ThingieContainer<TThing, TParent>::purge (bool pred (const TThing *)) {
        auto it = this->begin();
        while (it != this->end()) {
            auto element = it++;
            if (pred (element->second)) {
                element->second->release();
                this->erase (element);
            }
        }
    }

    /** Remove all items from the hash table. */
    template <class TThing, class TParent>
    void ThingieContainer<TThing, TParent>::clear() {
        while (this->begin() != this->end()) {
            this->begin()->second->release();
            this->erase (this->begin());
        }
    }

    /** Get a thing by its id. */
    template <class TThing, class TParent>
    TThing *ThingieContainer<TThing, TParent>::getById (const std::string &key) const {
        const auto iterator = this->find (key);
        return (iterator == this->end() ? nullptr : iterator->second);
    }

    /** Get a thing by its id, getting the ID from JSON data. */
    template <class TThing, class TParent>
    TThing *ThingieContainer<TThing, TParent>::getById (const Parsnip::Data &data, const char *field) {
        return getById (data[field].asString());
    }

    /** Search the things looking for a name and parent match. */
    template <class TThing, class TParent>
    TThing *ThingieContainer<TThing, TParent>::getByName (const std::string &name, TParent *parent) const {
        for (auto item : *this) {
            if (item.second->parent() == parent && *(item.second) == name) {
                return item.second;
            }
        }
        return nullptr;
    }

    /** Construct unique, random ID for a new item.
        @param item_type The type code for the item.
        @return The unique ID string. */
    template <class TThing, class TParent>
    std::string ThingieContainer<TThing, TParent>::getNewId (MusicThingie::Type item_type) const {
        const char type[2] = {static_cast<char> (item_type), '\0'};
        while (true) {
            long candidate = random();
            std::string newid = type + std::to_string (candidate);
            if (!getById (newid))
                return newid;
        }
    }

    /** Construct a new item instance and add it to the hash by its ID.
        @param name The new item's name.
        @param id The item's ID, if known, or the empty string.
        @param parent The parent object to which the new item will be linked. */
    template <class TThing, class TParent>
    TThing *ThingieContainer<TThing, TParent>::addItem (const std::string &name, std::string id, TParent *parent) {
        std::string newid (id.empty() ? getNewId (TThing::typetype()) : id);
        TThing *thing = allocate (parent, newid, name);
        assert (this->find (newid) == this->end());
        (*this)[newid] = thing;
        thing->retain();
        return thing;
    }

    /** Retrieve an item by ID or by name.
        - If ID is given, that is used.
        - Otherwise, a "persistent ID" based on hashing the name is tried;
          if a match is found, use that.
        - Otherwise, try searching by name and parent.  If found, use that.
        - Otherwise, create a new item with the name and a new ID assigned.
        @param name The name to search for.
        @param id The ID to search for.
        @param parent The parent item. */
    template <class TThing, class TParent>
    TThing *ThingieContainer<TThing, TParent>::addOrGetItem (const std::string &name, std::string id, TParent *parent) {
        assert (parent);
        TThing *thing;
        if (id.empty()) {
            // Hash the name for a candidate ID
            id = persistentId (TThing::typetype(), name);
            thing = getById (id);
            if (thing && thing->parent() == parent && *thing == name) {
                return thing;
            }
            // If there was no match, leave ID set to the candidate ID.
            if (thing)
                id = "";
            thing = getByName (name, parent);
        } else {
            thing = getById (id);
        }
        if (thing)
            return thing;
        return addItem (name, id, parent);
    }

    /** Reconstitute an item from a persisted file.
        @param data The JSON node with data to be restored.
        @param parent The parent to which the new record will be attached.
        @param namefield The name of the name field within `data`.
        @param idfield The name of the ID field within `data`.
        @return A newly constructed item, attached to its parent. */
    template <class TThing, class TParent>
    TThing *ThingieContainer<TThing, TParent>::addOrGetItem (const Parsnip::Data &data,
                                                             TParent *parent,
                                                             const std::string &namefield,
                                                             const std::string &idfield) {
        const std::string &name = data[namefield].asString();
        const std::string &id = data[idfield].asString();
        TThing *item = addOrGetItem (name, id, parent);
        assert (item);
        item->restore (data);
        return item;
    }

}  // namespace MusicLibrary
