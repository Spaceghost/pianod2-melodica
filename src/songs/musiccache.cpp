///
/// Caches for music thingie types.
/// @file       musiccache.cpp - pianod
/// @author     Perette Barella
/// @date       2020-03-19
/// @copyright  Copyright (c) 2016-2023 Devious Fish. All rights reserved.
///

#include <parsnip/parsnip.h>

#include "musictypes.h"
#include "retainedlist.h"
#include "musiccache.h"
#include "filter.h"
#include "mediaunit.h"
#include "encapmusic.h"

namespace Key {
    static const char *Expiration = "expiration";
    static const char *Type = "type";
    static const char *Item = "item";

}  // namespace Key

/** Extend the life of a cache object.
    @param seconds Number of seconds to guarantee the object's life. */
void ThingieCache::extend (int seconds) const {
    time_t when = time (nullptr) + seconds;
    if (expiration < when)
        expiration = when;
}

/** Initialize a thingie storage cache/pool. */
ThingiePool::ThingiePool() {
    next_purge = time (nullptr) + settings.purge_interval;
};

/** Initialize a thingie storage cache/pool.
    @param params Parameters describing the cache's retention
    and purge behavior. */
ThingiePool::ThingiePool (const ThingiePoolParameters &params) : settings (params) {
    next_purge = time (nullptr) + settings.purge_interval;
};

/** Update the cache's retention parameters. */
void ThingiePool::setParameters (const ThingiePoolParameters &params) {
    settings = params;
    next_purge = 0;
    purge();
}

/** Add (or update) an item to the cache.  If duplicate,
    will replace the item with the one from the cache,
    or vice-versa, depending on preferNew setting at
    cache instantiation.
    @param thing The item to put in the cache.
    @param replace If true, existing cache items will be replaced.
    If false, existing cached items are retained unless expired. */
void ThingiePool::add (MusicThingie *thing, bool replace) {
    assert (thing);
    if (size() >= settings.maximum_retained_items) {
        purge();
    }
    ThingieCache &cached = (*this) [thing->id()];
    if (thing == cached.item.get()) {
        cached.extend (settings.initial_duration);
        flog (LOG_WHERE (Log::WARNING), "Cache item re-added, ", (*thing)());
        return;
    }
    if (cached.item) {
        // Item was already in the cache
        bool identical = (*thing == *(cached.item));
        replace = replace || cached.expiration < time (nullptr);
        flog (LOG_WHERE (identical ? Log::GENERAL : Log::WARNING),
              "Cache item ",
              replace ? "not " : "",
              "replaced with ",
              identical ? "same " : "different ",
              (*thing)());
        if (!replace) {
            cached.extend (settings.initial_duration);
            return;
        }
    }
    cached.item = thing;
    cached.extend (settings.initial_duration);
}

/** Add several things to the cache.  For any that already exist, keep
    the prior cached entry unless expiredp.
    @param list A list of things to add to the cache. */
void ThingiePool::add (const SongList &list) {
    for (auto item : list) {
        add (item);
    }
}

/** Add several things to the cache.   For any that already exist, keep
    the prior cached entry unless expired.
    @param list A list of things to add to the cache. */
void ThingiePool::add (const ThingieList &list) {
    for (auto item : list) {
        add (item);
    }
}

/** Add or update several things to the cache.
    @param list A list of things to put in the cache. */
void ThingiePool::update (const SongList &list) {
    for (auto item : list) {
        add (item, true);
    }
}

/** Add or update several things to the cache.
    @param list A list of things to put in the cache. */
void ThingiePool::update (const ThingieList &list) {
    for (auto item : list) {
        add (item, true);
    }
}

/** Get an item from the cache.
    @param id The identifier of the thing to get.
    @return The thingie, or nullptr if it's not in the cache. */
MusicThingie *ThingiePool::get (const std::string &id) {
    auto const it = find (id);
    if (it == end())
        return nullptr;
    assert (it->second.item);
    // If it's getting used, keep it around a little longer
    it->second.extend (settings.reprieve_duration);
    return it->second.item.get();
}

/** Get matching items from the cache.
    @param filter A filter to select things to get.
    @return The thingies, or an empty set if no matches. */
ThingieList ThingiePool::get (const Filter &filter) {
    ThingieList matches;
    for (auto const &item : *this) {
        assert (item.second.item);
        if (item.second.item->matches (filter)) {
            matches.push_back (item.second.item);
            // If it's getting used, keep it around a little longer
            item.second.extend (settings.reprieve_duration);
        }
    }
    return matches;
}

/// Remove a thing from the cache.
void ThingiePool::erase (MusicThingie *thing) {
    auto it = find (thing->id());
    if (it != end()) {
        map_type::erase (it);
    }
}

/// Pare down the cache
void ThingiePool::purge() {
    std::string source_name = "purge";
    if (!empty()) {
        source_name = begin()->second.item->source()->key();
    }
    next_purge = time (nullptr) + settings.purge_interval;
    if (size() < settings.minimum_retained_items) {
        flog (LOG_WHERE (Log::CACHES), source_name, ": Size ", size(), " < Minimum retention of ", settings.minimum_retained_items);
        return;
    }
    if (time (nullptr) < oldest_entry) {
        flog (LOG_WHERE (Log::CACHES), source_name, ": No old enough candidates for purge, size=", size());
        next_purge = oldest_entry;
        return;
    }
    flog (LOG_WHERE (Log::CACHES), source_name, ": Pre-purge, size= ", size());

    time_t now = time (nullptr);
    time_t cutoff;
    if (now > oldest_entry + settings.purge_interval) {
        cutoff = ((now / 2) + (oldest_entry) / 2);
    } else {
        cutoff = now;
    }
    time_t oldest_entry = FAR_FUTURE;

    for (iterator it = begin(); it != end();) {
        iterator target = it++;
        if (target->second.item->getUseCount() > 1) {
            // Item is in use, so extend its life.
            target->second.extend (settings.reprieve_duration);
        }
        if (target->second.expiration <= cutoff && size() > settings.minimum_retained_items) {
            it = map_type::erase (it);
        } else if (target->second.expiration < oldest_entry) {
            oldest_entry = target->second.expiration;
        }
    }
    flog (LOG_WHERE (Log::CACHES), source_name, ": Post-purge, size= ", size());
}

/*
 *              Persistent pool
 */

/** Initialize the cache/pool. */
PersistentPool::PersistentPool (Media::Source * const src) : ThingiePool(), source (src) {
};

/** Initialize the cache/pool.
    @param src The source to which records will belong.
    @param params Parameters describing the cache's retention and purge behavior. */
PersistentPool::PersistentPool (Media::Source * const src, const ThingiePoolParameters &params)
: ThingiePool (params),
  source (src) {
};

/** Assemble the contents of the pool for serialization.
    @return An array of serializable records. */
Parsnip::Data PersistentPool::persist() const {
    Parsnip::Data data{ Parsnip::Data::List };

    Parsnip::Data serial;
    for (const auto &item : *this) {
        PersistentSong *song = dynamic_cast<PersistentSong *> (item.second.item.get());
        if (song) {
            serial = song->persist();
        } else {
            PersistentArtist *artist = dynamic_cast<PersistentArtist *> (item.second.item.get());
            if (artist) {
                serial = artist->persist();
            } else {
                PersistentAlbum *album = dynamic_cast<PersistentAlbum *> (item.second.item.get());
                if (album) {
                    serial = album->persist();
                } else {
                    PersistentPlaylist *playlist = dynamic_cast<PersistentPlaylist *> (item.second.item.get());
                    if (playlist) {
                        serial = playlist->persist();
                    } else {
                        auto *metaplaylist = dynamic_cast <PersistentMetaPlaylist *> (item.second.item.get());
                        if (metaplaylist) {
                            serial = metaplaylist->persist();
                        }
                    }
                }
            }
        }
        if (!serial.isNull()) {
            // clang-format off
            data.push_back (Parsnip::Data {Parsnip::Data::Dictionary,
                Key::Item, std::move (serial),
                Key::Type, MusicThingie::TypeName (item.second.item->type()),
                Key::Expiration, item.second.expiration
            });
            // clang-format on
            serial = nullptr; // To quiet the static analyzer about use of moved-from object
        }
    }
    // Adjust write time so if something fails we don't churn.
    write_time = (write_time ? write_time + 900 : 0);
    return (data);
}

/** Restore cache contents from serialized data.
    @param data The data to be restored*/
void PersistentPool::restore (const Parsnip::Data &data) {
    for (const Parsnip::Data &record : data) {
        try {
            MusicThingie::Type type = MusicThingie::TypeFromName (record [Key::Type].asString());
            time_t expiration = record [Key::Expiration].as<time_t>();
            MusicThingie *thing = reconstruct (type, record [Key::Item]);
            if (thing) {
                mapped_type &cache_record = (*this) [thing->id()];
                cache_record.item = thing;
                cache_record.expiration = expiration;
                continue;
            }
            flog (Log::WARNING, "Non-restorable type");
        } catch (const Parsnip::Exception &ex) {
            flog (Log::WARNING, "Could not restore cache record");
        } catch (const std::invalid_argument &ex) {
            flog (Log::WARNING, "Invalid record type");
        }
        if (logging_enabled (Log::WARNING)) {
            record.dumpJson ("Invalid record", std::clog);
        }
    }
}
