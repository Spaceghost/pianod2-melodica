///
/// User data, privileges, details and preferences implementation.
/// @file       user.cpp - pianod project
/// @author     Perette Barella
/// @date       Initial: 2012-03-20.  Split from users.c: 2014-11-20.
/// @copyright  Copyright 2012-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cstdio>
#include <cassert>

#include <unistd.h>
#include <sys/types.h>

#ifdef SHADOW_CAPABLE
#ifdef PAM_AUTHENTICATION
#include <security/pam_appl.h>
#elif defined (TRADITIONAL_PASSWD)
#include <pwd.h>
#endif

#endif

#include <string>
#include "parsnip/parsnip.h"

#include "enum.h"
#include "datastore.h"
#include "user.h"
#include "logging.h"
#include "utility.h"
#include "connection.h"

#define countof(x) (sizeof (x) / sizeof (*x))

Rank User::visitor_rank = Rank::Listener;

/// Dictionary names for user data file.
namespace userconfig {
    const char *user_node_name = "name";
    const char *user_node_password = "password";
    const char *user_node_rank = "rank";

    const char *user_privileges = "privileges";
    const char *user_privilege = "privilege";
    const char *privilege_name = "name";
    const char *privilege_enable = "granted";
    const char *privilege_enabled = "true";
    const char *privilege_disabled = "false";

    const char *user_data = "preferences";
}  // namespace userconfig

time_t User::write_time = 0;
void User::scheduleWrite (WritePriority priority) {
    time_t when = time (nullptr) + priority;
    if (write_time == 0 || when < write_time) {
        write_time = when;
    }
}

bool User::shadow_mode = false;
void User::shadow (bool mode) {
    User::shadow_mode = mode;
}


/*
 *              Rankings
 */
// Enumeration to text translation for user rankings.
const LookupTable <Rank> Ranking ({
    { "disabled", Rank::None },
    { "listener", Rank::Listener },
    { "user",     Rank::Standard },
    { "admin",    Rank::Administrator }
});

/*
 *              Privileges
 */

typedef struct privileges_t {
    const char *name;
    Privilege value;
    bool initial_value;
    bool persistable;
} PRIVILEGES;

const static PRIVILEGES privilege_def[] = {
    { "shadow",     Privilege::Shadow,      false, true },
    { "service",    Privilege::Service,     false, true },
    { "deejay",     Privilege::Queue,       false, true },
    { "influence",  Privilege::Influence,   true,  true },
    { "tuner",      Privilege::Tuner,       false, true },
    { "present",    Privilege::Present,     false, true },
    { NULL }
};
/// Enumeration to text translation for user privileges.
const LookupTable <Privilege, PRIVILEGES> Privileges (privilege_def);


/*
 *              Body
 */

/** Encrypt a password.
    @param password the password to encrypt.
    @return The encrypted password.
    @warning Returned password is stored in a static buffer that is
    reused in successive calls. */
static const char *encrypt_password (const char *password) {
    static const char *saltchars = "./0123456789QWERTYUIOPASDFGHJKLZXCVBNMqwertyuiopasdfghjklzxcvbnm";
    assert (strlen (saltchars) == 64);
    char salt[3];
    salt [0] = saltchars [random() % 64];
    salt [1] = saltchars [random() % 64];
    salt [2] = '\0';
    return (crypt (password, salt));
}


/** Create a new user with the username and password.
    Encrypt the password if requested (if not, it's already encrypted).
    @param username The user ID assigned the new user.
    @param pass The password for the new user.
    @param encrypt Whether to encrypt the password (user data restored
           from file will already be enciphered). */
User::User (const std::string &username, const std::string &pass, bool encrypt)
: name (username),
  password (encrypt ? encrypt_password (pass.c_str()) : pass) {
    // Initialize the new user's privileges
      for (auto const &priv : Privileges) {
        privileges [priv.value] = priv.initial_value;
    }
}

User::~User () {
    for (auto &item : data) {
        delete item.second;
    }
};


/** Change the password in a user's record.
    @param pass The new password. */
void User::setPassword (const std::string &pass) {
    password = encrypt_password (pass.c_str());
    scheduleWrite (IMPORTANT);
}

#ifdef SHADOW_CAPABLE
#if defined(PAM_AUTHENTICATION)
/// Conversation function that does not converse.
static int null_pam_conversation (int, const struct pam_message **, struct pam_response **, void *) {
    return PAM_CONV_ERR;
}

static bool shadow_authenticate (const std::string &name, const char *trypass) {
    struct pam_conv pam_conv = {};
    int pam_status = 0;
    pam_handle_t *pamh = nullptr;

    // Initial PAM setup
    pam_conv.conv = null_pam_conversation;
    const char *func = "pam_start";
    pam_status = pam_start ("checkpw", name.c_str(), &pam_conv, &pamh);
    if (pam_status == PAM_SUCCESS) {
        func = "pam_set_item";
        pam_status = pam_set_item (pamh, PAM_AUTHTOK, trypass);
        if (pam_status == PAM_SUCCESS) {
            func = "pam_authenticate";
            pam_status = pam_authenticate (pamh, 0);
            if (pam_status == PAM_SUCCESS || pam_status == PAM_AUTH_ERR) {
                // Good or bad credentials
                pam_end (pamh, pam_status);
                return pam_status == PAM_SUCCESS;
            }
        }
    }
    flog (LOG_WHERE (Log::WARNING), func, ": ", pam_strerror (pamh, pam_status));
    pam_end (pamh, pam_status);
    return false;
}
#elif defined (TRADITIONAL_PASSWD)
/** Validate a user/password combination is correct. */
static bool shadow_authenticate (const std::string &name, const char *trypass) {
    // Password is stored in the password file with old-style crypt(), just like we do it.
    struct passwd *pwentry = getpwnam (name.c_str());
    if (!pwentry)
        return false;
    if (!*trypass && strcmp (pwentry->pw_passwd, "") == 0)
        return true;
    if (strlen (pwentry->pw_passwd) < 2)
        return false;
    const char *encrypted = crypt (trypass, pwentry->pw_passwd);
    return strcmp (encrypted, trypass) == 0;
}
#else
#error No authentication scheme selected.
#endif
#endif

/** Validate a user's credentials (username/password).
    @param trypass The password, unenciphered.  It is enciphered and compared
    to the ciphered password on file.
    @return true on correct password, false otherwise. */
bool User::authenticate (const char *trypass) const {
    assert (trypass);
    // First deal with null passwords performed by editing the password file.
    if (!*trypass && password == "")
        return true;
#ifdef SHADOW_CAPABLE
    if (password == "*" && shadow_mode) {
        // Shadowing account; compare to the password on record with the system.
        return shadow_authenticate (name, trypass);
    }
#endif
    if (password.length() < 2)
        return false;
    const char *encrypted = crypt (trypass, password.c_str());
    return password == encrypted;
}

/** Change a user's password, authenticating the old one.
    @param old The user's old password, unenciphered.
    @param pass The new password, unenciphered.
    @return true if the old password was valid, false otherwise. */
bool User::changePassword (const std::string &old, const std::string &pass) {
    if (!authenticate (old))
        return false;
    setPassword (pass);
    return true;
}

#ifdef SHADOW_CAPABLE
/** Adopt a user's system password, both before doing so.
    @param old The user's old password, unenciphered.
    @param pass The new password, unenciphered.
    @return true if the old password was valid, false otherwise. */
bool User::assumeShadowPassword (const std::string &old, const std::string &pass) {
    if (!authenticate (old))
        return false;
    std::string original = std::move (password);
    try {
        password = "*";
        if (authenticate (pass)) {
            scheduleWrite (IMPORTANT);
            return true;
        }
    } catch (...) {
        // In case of bad_alloc or something else going wrong, this is important.
        password = std::move (original);
        throw;
    }
    password = std::move (original);
    return false;
}
#endif

/** Set a user's rank.
    @param newrank The rank enumerator to assign the user. */
void User::assignRank (Rank newrank) {
    if (rank != newrank)
        scheduleWrite (CRITICAL);
    rank = newrank;
}
/** Set a user's rank.
    @param newrank The text name of the rank to assign to the user.
    @return true if the text name is valid, false otherwise. */
bool User::assignRank (const std::string &newrank) {
    Rank old = rank;
    bool ok = Ranking.tryGetValue (newrank, &rank);
    if (rank != old)
        scheduleWrite (CRITICAL);
    return ok;
}

/** Determine if a user has a rank or better.
    @param minimum The desired rank.
    @return true if the user's rank is the minimum or better. */
bool User::haveRank (const Rank minimum) const {
    return (rank >= minimum);
};

void User::setPrivilege (const Privilege priv, bool setting) {
    if (privileges [priv] != setting)
        scheduleWrite (CRITICAL);
    privileges [priv] = setting;
}
bool User::havePrivilege (const Privilege priv) const {
    return privileges [priv];
}

/** Determine if a user is online.
    @param service Reference to the service to be checked for logins.
    @return true if user has at least one authenticated connection. */
bool User::online (const PianodService &service) const {
    std::vector <PianodConnection *> connections;
    for (auto conn : service) {
        if (conn->user == this) {
            return true;
        }
    }
    return false;
}

static std::string dataKey (const std::string &origin, const std::string &identity) {
    std::string key = origin + "<:>" + identity;
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    return key;
}

/** Attach some data to a user.  The data is persisted with the user's record.
    On success, the pointer passed is stored in the user's record; it must not
    be freed.  On failure, it is the responsibility of the caller to destroy it.
    @param element The data to attach.
    @return true on success, false on errror. */
bool User::attachData (UserData::DataStore *element) {
    std::string fullkey (dataKey (element->origin(), element->identity()));
    UserDataMap::iterator it = data.find (fullkey);
    if (it != data.end()) {
        if (element == it->second) {
            assert (0);
            flog (LOG_WHERE (Log::WARNING),
                  "Reattaching same element ", fullkey, " for user ", name);
        } else {
            // Replace existing value
            delete it->second;
            it->second = element;
            scheduleWrite (NOMINAL);
        }
        return true;
    }
    UserDataPair p (dataKey (element->origin(), element->identity()), element);
    std::pair<UserDataMap::iterator, bool> result = data.insert (p);
    scheduleWrite (NOMINAL);
    return result.second;
}

/** Get data associated with a user.
    @param datatype The data type portion of the key.
    @param dataid The ID portion of the key. */
UserData::DataStore *User::getData(const std::string &datatype, const std::string &dataid) const {
    UserDataMap::const_iterator it = data.find (dataKey (datatype, dataid));
    return (it == data.end() ? nullptr : it->second);
}

/** Remove data associated with a user.  The data is freed if found.
    @param datatype The data type portion of the key.
    @param dataid The ID portion of the key. */
void User::removeData(const std::string &datatype, const std::string &dataid) {
    UserDataMap::const_iterator it = data.find (dataKey (datatype, dataid));
    if (it != data.end()) {
        delete it->second;
        data.erase (it);
        scheduleWrite (NOMINAL);
    }
}

/** Prepare data for persistence.
    @return A Parsnip Data object ready for serialization. */
Parsnip::Data User::persist() const {
    using namespace Parsnip;

    // Construct privilege list...
    Data privs{Data::Dictionary};
    for (Privilege i : Enum<Privilege>()) {
        const PRIVILEGES *p = Privileges.get (i);
        if (p->persistable) {
            privs[p->name] = privileges[p->value];
        }
    }

    // Construct data section...
    Data userdata{Data::List};
    for (auto subset : data) {
        const auto &value = subset.second;
        userdata.push_back (value->persist());
    }

    // Initialize with name, rank, and serial nuh... uh, password
    return Parsnip::Data{Parsnip::Data::Dictionary,
                               userconfig::user_node_name,
                               name,
                               userconfig::user_node_password,
                               password,
                               userconfig::user_node_rank,
                               Ranking[rank],
                               userconfig::user_privileges,
                               std::move (privs),
                               userconfig::user_data,
                               std::move (userdata)};
};


/** Create (restore) a user from a Parsnip object.
    @param data The root parsnip object containing the user information.
    @return A user structure.
    @throw Throws an exception if the object does not provide a minimally valid user,
    which is name, password, rank. */
User User::reconstitute_user (const Parsnip::Data &data) {
    User user (data[userconfig::user_node_name].asString(), data[userconfig::user_node_password].asString());
    const std::string &rank = data[userconfig::user_node_rank].asString();
    if (!user.assignRank (rank.c_str())) {
        flog (LOG_WHERE (Log::WARNING), "User ", user.name, " rank '", rank, "' is invalid");
    }

    // Restore user privileges
    auto handler =
            std::function<void (const std::string &, bool)>{[&user] (const std::string &name, bool value) -> void {
                const PRIVILEGES *privilege = Privileges.get (name.c_str());
                if (!privilege) {
                    flog (LOG_WHERE (Log::WARNING), "User ", user.name, ", invalid privilege '", name, "'");
                } else if (!privilege->persistable) {
                    flog (LOG_WHERE (Log::WARNING), "User ", user.name, ", ignored non-persistable privilege ", name);
                } else {
                    user.privileges[privilege->value] = value;
                }
            }};
    data[userconfig::user_privileges].foreach (handler);

    auto data_handler =
            std::function<void (const Parsnip::Data &)>{[&user] (const Parsnip::Data &subset) -> void {
                const std::string &name = subset[UserData::Key::DataOrigin].asString();
                UserData::DataStore *data = nullptr;
                if (UserData::DataStore::isSourceData (name.c_str())) {
                    data = new UserData::JSONData (subset);
                } else if (name == UserData::Key::PlaylistRatings || name == UserData::Key::TrackRatings) {
                    data = new UserData::Ratings (subset);
                } else if (name == UserData::Key::OverplayedTracks) {
                    data = new UserData::OverplayedList (subset);
                } else {
                    flog (LOG_WHERE (Log::ERROR), "Unknown data type ", name, " for ", user.name);
                }
                if (data) {
                    if (!user.attachData (data)) {
                        delete data;
                        flog (LOG_WHERE (Log::ERROR), "Cannot store user data ", name, " for ", user.name);
                    };
                }
            }};
    data[userconfig::user_data].foreach (data_handler);
    return user;
}

/** Translate a privilege to its enumerator.
    @param privilege The privilege name.
    @return The privilege enumerator.
    @throw Invalid argument if the privilege string is invalid. */
const Privilege User::getPrivilege (const std::string &privilege) {
    Privilege p;
    if (Privileges.tryGetValue (privilege, &p)) return p;
    throw std::invalid_argument ("Invalid privilege string");
}
/** Translate a rank to its enumerator.
    @param rank The rank name.
    @return The rank enumerator.
    @throw Invalid argument if the rank string is invalid. */
const Rank User::getRank (const std::string &rank) {
    Rank r;
    if (Ranking.tryGetValue (rank, &r)) return r;
    throw std::invalid_argument ("Invalid rank string");
}
/** Translate a rank enumerator to its text.
    @param rank The rank to lookup.
    @return The corresponding rank name (string). */
const char *User::getRankName (Rank rank) {
    return Ranking.get(rank)->name;
}
/** Get the rank name for the current user.
    @return Rank name (string) for the current user. */
const char *User::getRankName (void) const {
    return Ranking.get(rank)->name;
}
/** Translate a privilege enumerator to its text.
    @param privilege The privilege to lookup.
    @return The corresponding privilege name (string). */
const char *User::getPrivilegeName(Privilege privilege) {
    return Privileges.get(privilege)->name;
}

void User::setVisitorRank (const std::string &rankname) {
    visitor_rank = Ranking [rankname];
}

/** Retrieve a user for processing the startup script.
    @returns A fake administrator user with all privileges. */
User *User::getStartscriptUser (void) {
    static User user ("startscript", "inapplicable");
    user.rank = Rank::Administrator;
    for (Privilege p : Enum<Privilege>()) {
        user.privileges [p] = true;
    }
    return &user;
}

