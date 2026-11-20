///
/// User data storage mechanism.
/// @file       datastore.h - pianod
/// @author     Perette Barella
/// @date       2015-01-08
/// @copyright  Copyright (c) 2015-2020 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <ctime>

#include <string>
#include <stdexcept>
#include <unordered_map>
#include <functional>

#include <parsnip/parsnip.h>

#include "ratings.h"

/// User data storage.
namespace UserData {
    namespace Key {
        // Keys for type/origin and name/identity
        extern const char *DataOrigin;
        extern const char *DataIdentity;
        extern const char *TheData;

        // Access keys
        extern const char *PlaylistRatings;
        extern const char *TrackRatings;
        extern const char *OverplayedTracks;
    }  // namespace Key

    // For handling ratings/string conversions.
    template <typename T>
    inline T type_convert (const T value, const T) {
        return value;
    };
    inline std::string type_convert (const Rating value, const std::string &) {
        return RATINGS[value];
    }
    inline Rating type_convert (const std::string &value, Rating) {
        return RATINGS[value];
    };

    /** Storage mechanism for user data.
        A user may have multiple containers.  Each container is identified
        by an origin-identity pair, such as `pandora-perette@deviousfish.com` or
        `localfiles-/Users/perette/Music`; thus, there may be multiple
        data containers for different kinds of data ("origin"), each with
        a unique identity. */
    class DataStore {
    public:
        virtual ~DataStore (void){};
        /// Retrieve the origin (type of data) stored in this storage unit.
        virtual const std::string &origin (void) const = 0;
        /// Retrieve the identity (primary identifier) of the data in this storage unit.
        virtual const std::string &identity (void) const = 0;
        /// Restore data from a JSON source.
        /// @param data Data to restore from.
        /// @return True on success, false otherwise.
        virtual bool reconstitute (const Parsnip::Data &data) = 0;
        /// Persist data using Parsnip.
        /// @return A Parsnip Data object suitable for serialization.
        virtual Parsnip::Data persist() const = 0;
        bool isSourceData (void) const;
        static bool isSourceData (const std::string &origin);
    };

    /** Store key-value pairs of string keys & mixed types using
        Parsnip::Data.  */
    class JSONData : public DataStore, public Parsnip::Data {
        const std::string originid;
        const std::string id;

    public:
        JSONData (const std::string &, const std::string &);
        JSONData (const Parsnip::Data &);
        virtual const std::string &origin (void) const override;
        virtual const std::string &identity (void) const override;
        virtual bool reconstitute (const Parsnip::Data &data) override;
        virtual Parsnip::Data persist() const override;
        void rename (const std::string new_id);
    };

    /** Store key-value pairs of arbitrary key and value types.
        @tparam TKey The type of the key.  Always string, at the moment.
        @tparam TValue The type of the data.
        @tparam TStorage The type of the data when serialized. */
    template <class TKey = std::string, class TValue = std::string, class TStorage = TValue>
    class Dictionary : public DataStore, public std::unordered_map<TKey, TValue> {
        const std::string originid;
        const std::string id;

    public:
        Dictionary (const std::string &_origin, const std::string &_id) : originid (_origin), id (_id) {
        }
        virtual const std::string &origin (void) const override {
            return originid;
        };
        virtual const std::string &identity (void) const override {
            return id;
        };
        void rename (const std::string new_id) {
            const_cast<std::string &> (id) = new_id;
        }
        bool add (const TKey &key, const TValue &value) {
            try {
                std::pair<TKey, TValue> p (key, value);
                std::pair<typename std::unordered_map<TKey, TValue>::iterator, bool> result = this->insert (p);
                return result.second;
            } catch (...) {
                return false;
            }
        }
        const TValue &get (const TKey &key, const TValue &def) const {
            auto it = this->find (key);
            return (it == this->end() ? def : it->second);
        }

        Dictionary (const Parsnip::Data &data)
        : originid (data[Key::DataOrigin].asString()), id (data[Key::DataIdentity].asString()) {
            reconstitute (data[Key::TheData]);
        }

        virtual bool reconstitute (const Parsnip::Data &data) override {
            std::function<void (const std::string &, const TStorage &)> handler
                    = [this] (const std::string &key, const TStorage &value) {
                          (*this)[key] = type_convert (value, TValue{});
                      };
            data.foreach (handler);
            return true;
        }

        virtual Parsnip::Data persist() const override {
            Parsnip::Data data{Parsnip::Data::Dictionary};
            for (auto const &item : *this) {
                data[item.first] = type_convert (item.second, TStorage{});
            }
            return Parsnip::Data{Parsnip::Data::Dictionary,
                                       Key::DataOrigin,
                                       this->origin(),
                                       Key::DataIdentity,
                                       this->identity(),
                                       Key::TheData,
                                       std::move (data)};
        };
    };

    /** Container for user ratings values (playlists & songs).
        Playlist ratings are stored in one container per user.
        Each sources storing song ratings uses one container for each user.
        Values are stored in key-value pairs, with an ID specific to the item. */
    class Ratings : public Dictionary<std::string, Rating, std::string> {
    public:
        using Dictionary::Dictionary;
        static Ratings *retrieve (const User *user, const std::string &ratingKind, const std::string &key);
        static Ratings *retrieve (User *user, const std::string &ratingKind, const std::string &key);
    };

    /** Container for lists of user overplayed songs.
        Each source storing overplayed ratings uses one container for each user.
        Values are stored in key-value pairs, with an ID specific to the item
        and the time the overplayed ban expires. */
    class OverplayedList : public Dictionary<std::string, time_t> {
    public:
        OverplayedList (const std::string &ratingGroup)
        : Dictionary<std::string, time_t> (Key::OverplayedTracks, ratingGroup){};
        OverplayedList (const Parsnip::Data &json) : Dictionary (json) {
        }
        bool isExpired (const std::string &song_id);
        static OverplayedList *retrieve (const User *user, const std::string &key);
        static OverplayedList *retrieve (User *user, const std::string &key);
    };

}  // namespace UserData
