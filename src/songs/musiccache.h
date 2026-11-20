///
/// Caches for music thingie types.
/// @file       musiccache.h - pianod
/// @author     Perette Barella
/// @date       2020-03-19
/// @copyright  Copyright (c) 2016-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <retainer.h>

/** Structure for tracking pool objects and their status for ThingiePool.
    @see ThingiePool. */
struct ThingieCache {
    Retainer<MusicThingie *> item;
    mutable time_t expiration = 0;
    void extend (int seconds = 600) const;
};

struct ThingiePoolParameters {
    using size_type = unsigned long;
    time_t initial_duration{ 3600 };           ///< Retain new or reinserted items for at least this amount of times.
    time_t reprieve_duration{ 900 };           ///< Retain in-use objects at least this much longer.
    size_type minimum_retained_items{ 4000 };  ///< Don't purge if we have less than this many.
    size_type maximum_retained_items{ 4800 };  ///< If we hit this many items, try purging before adding.
    time_t purge_interval{ 307 };              ///< Schedule periodic purges at this interval.
};

/** A cache/pool that retains all music thingies, mixed,
    and allows them to be retrieved from a single place by ID. */
class ThingiePool : protected std::unordered_map<std::string, ThingieCache> {
private:
    using map_type = std::unordered_map<std::string, ThingieCache>;
    ThingiePoolParameters settings;
    time_t next_purge{ time (nullptr) + 4000 };
    time_t oldest_entry{ time (nullptr) };

    void purge (void);

public:
    ThingiePool();
    ThingiePool (const ThingiePoolParameters &params);
    void setParameters (const ThingiePoolParameters &params);
    void add (MusicThingie *thing, bool update = false);
    void add (const SongList &songs);
    void add (const ThingieList &things);
    inline void update (MusicThingie *thing) {
        add (thing, true);
    }
    void update (const SongList &songs);
    void update (const ThingieList &things);
    void erase (MusicThingie *thing);
    MusicThingie *get (const std::string &id);
    ThingieList get (const Filter &filter);
    /// Periodically pare down the cache.
    inline void periodic (void) {
        if (next_purge <= time (nullptr))
            purge();
    }
};

/** A ThingiePool that can persist & restore its data.
    When persisting, only items of a persistable type are stored:
    PersistentArtist, PersistentAlbum, PersistentSong, PersistentPlaylist,
    PersistentMetaPlaylist. */
class PersistentPool : public ThingiePool {
protected:
    Media::Source * const source{ nullptr }; ///< The source to which records will belong.
    mutable time_t write_time{ 0 }; ///< The time at which cached data should be saved.

public:
    PersistentPool (Media::Source * const src);
    PersistentPool (Media::Source * const src, const ThingiePoolParameters &params);

    /** Mark data for persistence.  If called frequently, flushing happens sooner. */
    inline void markDirty() {
        time_t now = time (nullptr);
        if (write_time == 0) {
            write_time = now + 3600 * 6;
        } else if (write_time > now) {
            write_time -= 5;
        }
    }
    
    /// Check if the pool's data has been modified.
    inline bool isDirty() const {
        return (write_time != 0);
    }
    
    /// Reset the pool-modified flag.
    inline void clearDirty() const {
        write_time = 0;
    }
    
    /// Check if the pool is due to be persisted.
    inline bool writeIsDue () const {
        return (write_time > time (nullptr));
    }

    Parsnip::Data persist() const;
    void restore (const Parsnip::Data &data);
    
    /** Reconstructs a cached record.
        @param type The record's `type()` value.
        @param data The persisted record data.
        @throw `Parsnip::Exception` if `data` is not in expected format,
        or `invalid_argument` If `type` is not supported. */
    virtual MusicThingie *reconstruct (MusicThingie::Type type, const Parsnip::Data &data) = 0;

    inline void add (MusicThingie *thing) {
        ThingiePool::add (thing);
        markDirty();
    }
    inline void add (const SongList &songs) {
        ThingiePool::add (songs);
        markDirty();
    }
    inline void add (const ThingieList &things) {
        ThingiePool::add (things);
        markDirty();
    }
};
