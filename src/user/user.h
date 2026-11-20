///
/// User data, privileges, details and preferences implementation.
/// @file       user.h - pianod project
/// @author     Perette Barella
/// @date       Initial: 2012-03-20.  Split from users.c: 2014-11-20.
/// @copyright  Copyright 2012-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>
#include <unordered_map>

#include <parsnip/parsnip.h>

#include "logging.h"
#include "fundamentals.h"
#include "enumeratedarray.h"
#include "datastore.h"

/// User types in ascending ranks/level of authority
enum class Rank {
	None,              ///< User is unauthorized to do anything except log in.
	Listener,          ///< User can query and view song-related data, but not change selections.
	Standard,          ///< User can query, choose playlist, request music, change volume.
	Administrator      ///< User has full authorization, except for accessing others' private data.
};

/// User privilege flags, corresponding to an array position.
enum class Privilege {
	Service,      ///< User may add or remove sources.  (Automatic for admin rank.)
    Queue,        ///< User may queue or cancel songs.  (Automatic for standard rank.)
	Influence,    ///< User's playlist preferences influence autotuning selections.
	Tuner,        ///< User may set other users' presence, for autotuning.
    Shadow,       ///< Attribute: User was created automatically, shadowing a user account.
	Present,      ///< Attribute: Consider user present, even if not logged in.
	Count
};

/// Postfix increment operator to allow Privilege iteration.
static inline Privilege operator++(Privilege &p, int) {
    Privilege prior = p;
    p = static_cast<Privilege>(int (prior) + 1);
    return prior;
}


class PianodService;

/// Data about each user
class User {
    friend class UserManager;
    static bool shadow_mode;
    static time_t write_time;
private:
    typedef enum user_write_priority_t {
        CRITICAL = 1, ///< User removal, privilege change, etc.
        IMPORTANT = 10, ///< User creation
        NOMINAL = 60, ///< Sources, preferences, data attachments, etc.
        TRIVIAL = 300 ///< Ratings
    } WritePriority;
    std::string name; ///< User's login name
    std::string password; ///< Encrypted password
    Rank rank = Rank::None; ///< Assigned rank.
    EnumeratedArray<Privilege, bool> privileges; ///< Assigned privileges.
    // Note: effective privileges may be different, an effect of rank.

    using UserDataPair = std::pair <std::string, UserData::DataStore *>;
    using UserDataMap = std::unordered_map <std::string, UserData::DataStore *>;
    UserDataMap data; ///< Data sets attached to the user

    static void scheduleWrite (WritePriority priority);
public:
    User (const std::string &username, const std::string &pass, bool encrypt = false);
    User (const User &) = delete;
    User &operator=(const User &) = delete;
    User (User &&) = default;
    User &operator=(User &&) = default;
    ~User ();
    inline const std::string &username (void) const { return name; };
    void setPassword (const std::string &pass);
    bool authenticate (const char *trypass) const;
    bool authenticate (const std::string &trypass) const { return authenticate (trypass.c_str()); };
    bool changePassword (const std::string &old, const std::string &pass);
#ifdef SHADOW_CAPABLE
    bool assumeShadowPassword (const std::string &old, const std::string &pass);
#endif
    // Rank stuff
    void assignRank (Rank newrank);
    bool assignRank (const std::string &newrank);
    bool haveRank (const Rank rank) const;
    inline Rank getRank (void) const { return rank; };
    const char *getRankName (void) const;
    // Privilege stuff 
    void setPrivilege (const Privilege priv, bool setting);
    bool havePrivilege (const Privilege priv) const;
    // Other statusy things 
    bool online (const PianodService &service) const;
    bool attachData (UserData::DataStore *);
    inline void updateData (void) { scheduleWrite (TRIVIAL); };
    UserData::DataStore *getData (const std::string &datatype, const std::string &dataid = "") const;
    void removeData (const std::string &datatype, const std::string &dataid = "");
protected:
    inline bool operator < (const User &other) {
        return name.compare (other.name);
    }
    inline bool operator < (const std::string &other) {
        return name.compare (other);
    }
    Parsnip::Data persist () const;

    // Static methods & Variables 
    static Rank visitor_rank; ///< Rank for users that have not authenticated.
public:
    static void shadow (bool mode);
    static bool shadow () { return (shadow_mode); };
    static const Privilege getPrivilege (const std::string &privilege);
    static const Rank getRank (const std::string &rank);
    static const char *getRankName (Rank rank);
    static const char *getPrivilegeName (Privilege privilege);
    static inline Rank getVisitorRank () {
        return visitor_rank;
    }
    static void setVisitorRank (const std::string &rank);
    static User *getStartscriptUser (void);
private:
    static bool reconstituteUserData (User &user, const Parsnip::Data &data);
    static User reconstitute_user (const Parsnip::Data &data);
};

class PianodConnection;

namespace Football {
    class ServiceBase;
}

extern void register_user_commands (Football::ServiceBase *service);

