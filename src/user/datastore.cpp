///
/// User data storage mechanism.
/// @file       datastore.cpp - pianod
/// @author     Perette Barella
/// @date       2015-01-08
/// @copyright  Copyright 2016-2020 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <stdexcept>
#include <string>

#include "utility.h"
#include "datastore.h"
#include "sources.h"
#include "user.h"

namespace UserData {
    namespace Key {
        const char *DataOrigin = "type";
        const char *DataIdentity = "identity";
        const char *TheData = "data";

        const char *PlaylistRatings = "playlistRatings";
        const char *TrackRatings = "songRatings";
        const char *OverplayedTracks = "overplays";
    }  // namespace Key

    /*
     *                  Base storage type
     */
    bool DataStore::isSourceData (const std::string &origin) {
        return (Sources::isSource (origin));
    };

    bool DataStore::isSourceData (void) const {
        return isSourceData (origin());
    }

    /*
     *                  JSON Data Store
     */

    JSONData::JSONData (const std::string &_origin, const std::string &_id)
    : Parsnip::Data (Parsnip::Data::Dictionary), originid (_origin), id (_id) {
    }

    JSONData::JSONData (const Parsnip::Data &data)
    : Parsnip::Data (Parsnip::Data::Dictionary), originid (data[Key::DataOrigin].asString()), id (data[Key::DataIdentity].asString()) {
        reconstitute (data[Key::TheData]);
    }

    const std::string &JSONData::origin (void) const {
        return originid;
    };

    const std::string &JSONData::identity (void) const {
        return id;
    };

    void JSONData::rename (const std::string new_id) {
        const_cast<std::string &> (id) = new_id;
    }

    bool JSONData::reconstitute (const Parsnip::Data &data) {
        Parsnip::Data::operator= (data);
        return true;
    }

    Parsnip::Data JSONData::persist() const {
        return Parsnip::Data::make_dictionary ({
            {Key::DataOrigin, this->origin()},
            {Key::DataIdentity, this->identity()},
            {Key::TheData, *this}
        });
    }

    /*
     *                  Ratings
     */

    /** Retrieve a ratings group for a user.
        If the user doesn't have the requested group, try to create one.
        @param user The user for whom to retrieve ratings.
        @param ratingKind The type of ratings, either playlist ratings or song ratings key.
        @param key The source's type/name of the collection to retrieve.
        @return The user's ratings, or a nullptr if it couldn't be found or created. */
    Ratings *Ratings::retrieve (User *user, const std::string &ratingKind, const std::string &key) {
        Ratings *ratings = dynamic_cast<Ratings *> (user->getData (ratingKind, key));
        if (!ratings) {
            ratings = new (std::nothrow) Ratings (ratingKind, key);
            if (ratings && !user->attachData (ratings)) {
                delete ratings;
                ratings = nullptr;
            }
        }
        return ratings;
    }

    Ratings *Ratings::retrieve (const User *user, const std::string &ratingKind, const std::string &key) {
        return dynamic_cast<Ratings *> (user->getData (ratingKind, key));
    }

    /*
     *                  Overplayed Ratings
     */

    bool OverplayedList::isExpired (const std::string &song_id) {
        auto it = find (song_id);
        if (it != end()) {
            if (it->second > time (nullptr))
                return true;
            // The overplay rating has expired, so remove it.
            erase (it);
        }
        return false;
    }

    /** Retrieve overplayed list for a user.
        If the user doesn't have a list, try to create one.
        @param user The user for whom to retrieve the list.
        @param key The source's type/name of the collection to retrieve.
        @return The list of overplays, or a nullptr if it couldn't be found or created. */
    OverplayedList *OverplayedList::retrieve (User *user, const std::string &key) {
        OverplayedList *list = dynamic_cast<OverplayedList *> (user->getData (Key::OverplayedTracks, key));
        if (!list) {
            list = new (std::nothrow) UserData::OverplayedList (key);
            if (list && !user->attachData (list)) {
                delete list;
                list = nullptr;
            }
        }
        return list;
    }

    /** Retrieve overplayed list for a user.
        If the user doesn't have a list, try to create one.
        @param user The user for whom to retrieve the list.
        @param key The source's type/name of the collection to retrieve.
        @return The list of overplays, or a nullptr if it couldn't be found or created. */
    OverplayedList *OverplayedList::retrieve (const User *user, const std::string &key) {
        return dynamic_cast<OverplayedList *> (user->getData (Key::OverplayedTracks, key));
    }

}  // namespace UserData
