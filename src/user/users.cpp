///
/// User management provider.
/// @file       users.cpp - pianod project
/// @author     Perette Barella
/// @date       2012-03-20
/// @copyright  Copyright 2012-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>

#include <unistd.h>
#include <sys/types.h>
#ifdef SHADOW_CAPABLE
#include <pwd.h>
#endif

// crypt() is in unistd.h for BSD. 
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>

#include <football/fb_public.h>
#include <parsnip/parsnip.h>
#include <parsnip/parsnip_command.h>

#include "interpreter.h"

#include "enum.h"
#include "fundamentals.h"
#include "utility.h"
#include "user.h"
#include "users.h"
#include "response.h"
#include "logging.h"
#include "fileio.h"
#include "connection.h"

typedef enum find_kind_t {
    FIND_OPEN_CONNECTIONS,
    FIND_ALL_CONNECTIONS
} FIND_KIND;

namespace userconfig {
    const char *user_list = "users";

    const char *shadow_user = "shadow";
}  // namespace userconfig

const static char *UserDataJSONFilename = "passwd.json";

#define countof(x) (sizeof (x) / sizeof (*x))


/** Get the name of the user used as a template for shadow users */
const std::string &UserManager::shadowUserName () const {
    return shadow_user_name;
};

/** Get a user as a template for shadow users */
void UserManager::shadowUserName (const std::string &who) {
    if (shadow_user_name != who) {
        shadow_user_name = who;
        User::scheduleWrite (User::CRITICAL);
    }
};

/** Add the default 'admin' user to the users list */
void UserManager::createDefaultUser () {
    User admin ("admin", "admin", true);
    admin.rank = Rank::Administrator;
    if (addUser (admin)) {
        flog (LOG_WHERE (Log::GENERAL), "Created admin user.");
    } else {
        flog (LOG_WHERE (Log::ERROR), "Cannot create admin user.");
        assert (false);
    }
}

/// Resubstantiate a user from the XML userdata file
/// @throw bad_alloc, which will leave class in indeterminate state.
void UserManager::restore() {

    try {
        Parsnip::Data data = retrieveJsonFile (UserDataJSONFilename);
        try {
            auto handler = std::function<void (const Parsnip::Data &)> {
                [this] (const Parsnip::Data &data)->void {
                    User newuser = User::reconstitute_user (data);
                    std::string name = newuser.username();
#ifdef SHADOW_CAPABLE
                    if (newuser.privileges[Privilege::Shadow] &&
                        // Make sure shadowed users still exist on system, or remove them.
                        !getpwnam (newuser.username().c_str())) {
                        flog (LOG_WHERE (Log::WARNING), "Dropping removed shadow user ", name);
                        return;
                    }
#endif
                    if (!addUser (newuser)) {
                        flog (LOG_WHERE (Log::WARNING), "Duplicate user ", name);
                    };

                }
            };
            data [userconfig::user_list].foreach (handler);          

        } catch (const Parsnip::Exception &ex) {
            flog (LOG_WHERE (Log::WARNING), "Failure restoring user data from JSON: ", ex.what());
        }
    } catch (const std::exception &ex) {
        flog (LOG_WHERE (Log::WARNING), UserDataJSONFilename, ": ", ex.what());
    }
    // Never write the config file until changes are made.
    // Not only would this be wasteful, but it reduces chances of
    // clobbering the existing password file with a broken one.
    User::write_time = 0;

    // If there are no users, create a starter one.
    flog (LOG_WHERE (Log::GENERAL), "Restored ", user_manager->size(), " users");
    if (empty()) {
        createDefaultUser();
    }
}

/** Persist user data to a file.  Write to a new file, then carefully move
    the old one out/new one in in such a way as to minimize risk. */
bool UserManager::persist() {
    using namespace Parsnip;

    // Only write if there are updates.
    if (User::write_time == 0)
        return true;

    // If we fail along the way, try again in awhile.
    User::write_time = time (nullptr) + User::TRIVIAL;
    bool status = false;
    try {
        Data users {Data::List};
        for (const auto &user : *this) {
            users.push_back (user.second->persist());
        }
        Data document{Data::Dictionary,
            userconfig::user_list, std::move (users)
        };
        if (!shadow_user_name.empty()) {
            document [userconfig::shadow_user] = shadow_user_name;
        }
        status = (carefullyWriteFile (UserDataJSONFilename, document));
    } catch (std::exception &) {
        flog (LOG_WHERE (Log::ERROR), "Could not serialize user data");
        return false;
    }

    if (status) {
        User::write_time = 0;
    }
    return status;
};

/** Execute periodic tasks:
    - If the user data file is scheduled for write, persist it now. */
float UserManager::periodic (void) {
    if (User::write_time == 0)
        return A_LONG_TIME;

    time_t now = time (nullptr);
    if (User::write_time > now)
        return now - User::write_time;
    persist();
    return A_LONG_TIME;
}

/** Add a user to the list of known users.
    @param user A profile for the user to register.
    @return A pointer to the newly constructed user. */
User *UserManager::addUser (User &user) {
    User *new_user = new User (std::move (user));
    auto item = make_pair(strtolower (new_user->name), new_user);
    auto result = insert (item);
    if (result.second) {
        User::scheduleWrite(User::IMPORTANT);
        return new_user;
    }
    delete new_user;
    return nullptr;
}

/** Remove a user from the registered list of users.
    @param user The user to remove.
    The passed pointer is invalidated by this call.*/
void UserManager::deleteUser (User *user) {
    erase(strtolower (user->name));
    User::scheduleWrite (User::CRITICAL);
}

#ifdef SHADOW_CAPABLE
/** Determine if a shell is listed in /etc/shells.
    ftpd set a precedent that "real" users' shells will be listed in therein.
    If not, this is a pseudo-user for system services.
    @param shell A user's shell setting.
    @return true if the shell is listed as a valid shell choice in /etc/shells.
 */
static bool allowedShell (const char *shell) {
    std::ifstream shelllist ("/etc/shells", std::ifstream::in);
    if (!shelllist.fail()) {
        std::string line;
        while (std::getline (shelllist, line)) {
            trim (line);
            if (line == shell) {
                return true;
            }
        }
    }
    return false;
}
#endif

/** Retrieve a user's record.
    @param who The username to retrieve.  Case insensitive.
    @return The record, or nullptr if not found. */
User *UserManager::tryget (const std::string &who) {
    iterator item = find (strtolower (who));
    return (item == end() ? nullptr : item->second);
};

/** Retrieve a user's record.
    @param who The username to retrieve.  Case insensitive.
    @return The record.
    @throw CommandError if the specified user is not found. */
User *UserManager::get (const std::string &who) {
    User *user = tryget (who);
    if (!user) {
        throw CommandError (E_NOTFOUND, who);
    }
    return user;
};

/** Verify a user's credentials.  If password shadowing is enabled and
    the user is not known to pianod, checks for a known, eligible
    system user with the name and adds them if found.
    @param who The username to retrieve.  Case insensitive.
    @param password The password to try.
    @return The record, if the users exists and the password is valid. */
User *UserManager::authenticate(const std::string &who, const std::string &password) {
    iterator item = find (strtolower (who));
#ifdef SHADOW_CAPABLE
    if (item == end() && User::shadow_mode) {
        // Implicitly create the user
        struct passwd *pwentry = getpwnam (who.c_str());
        if (!pwentry)
            return NULL;
        if (!allowedShell (pwentry->pw_shell))
            return NULL;
        User newuser (who, "*");
        if (!shadow_user_name.empty()) {
            User *shadow = get (shadow_user_name);
            if (shadow) {
                for (Privilege i : Enum<Privilege>()) {
                    newuser.privileges [i] = shadow->privileges [i];
                }
                newuser.rank = shadow->rank;
            } else {
                flog (Log::WARNING, "Could not find shadow user: ", shadow_user_name);
            }
        }
        newuser.privileges [Privilege::Shadow] = true;
        if (!addUser (newuser)) return NULL;
        flog (LOG_WHERE (Log::WARNING),
               "Created new shadow user ", who);
        item = find (who);
        assert (item != end());
    }
#endif
    if (item == end())
        return NULL;
    return item->second->authenticate (password) ? item->second : NULL;
};

/** Disable a privilege for all users.
    @param priv The privilege to clear. */
void UserManager::clearPrivilege (Privilege priv) {
    for (auto &user : *this) {
        user.second->privileges [priv] = false;
    };
}

/** Retrieve a list of a user's connections.
    @param service The football service to search.
    @param user The user to find.
    @return A list of connections, empty if none found. */
std::vector<PianodConnection *> UserManager::getUserConnections (PianodService &service,
                                                          const User *user) const {
    std::vector <PianodConnection *> connections;
    for (auto conn : service) {
        if (conn->user == user) {
            connections.push_back (conn);
        }
    }
    return connections;
}

/** Retrieve a list of users matching some predicate. */
UserList UserManager::getUsers (UserSelectionPredicate predicate) const {
    UserList users;
    users.reserve (size());
    for (auto user : *this) {
        if (predicate (user.second)) {
            users.push_back (user.second);
        }
    }
    return users;
}

/** Retrieve a list of a users connected.
    @param service The football service to search.
    @param use_attribute If true, then the Present attribute is considered
    in determining presence.  If false, active sessions indicate presence.
    @return A list of users, empty if none online/present. */
UserList UserManager::getUsersPresent (PianodService &service,
                                       bool use_attribute) const {
    UserList users;
    users.reserve (size());
    for (auto user : *this) {
        if (user.second->online (service) ||
            (use_attribute && user.second->havePrivilege (Privilege::Present))) {
            users.push_back (user.second);
        }
    }
    return users;
}

/** Determine if a datastore's contents are eligible for reading by a user.
    @param data The datastore.
    @param owner The owner of the datastore.
    @param requester The user that wants to access the data.
    @return True if the requester is eligible to read the data. */
static bool datastoreIsListed (UserData::DataStore *data,
                               User *owner,
                               const User *requester) {
    assert (requester);
    auto dict = dynamic_cast<UserData::JSONData *> (data);
    if (!dict)
        return false;

    Ownership::Type perm;
    if (!OWNERSHIP.tryGetValue (dict->getOr ("accessMode", dict->getOr ("ownership", "private")), &perm))
        return false;

    PrimaryOwnership test (perm, owner);
    return test.isUsableBy (requester);
}

/** Return a list of persisted datasource parameters.
    @param selection Which selection criteria to apply in creating list.
    @param requester If specified, return only the requester's sources. */
UserManager::StoredSourceList UserManager::getStoredSources (UserManager::WhichSources selection,
                                                         const User *requester) const {
    StoredSourceList matches;
    matches.reserve (size() * 3);
    for (auto &user : *this) {
        for (auto &data : user.second->data) {
            if (data.second->isSourceData()) {
                bool selected = false;
                switch (selection) {
                    case WhichSources::User:
                        selected = (requester == user.second);
                        break;
                    case WhichSources::Listed:
                        selected = (requester == nullptr ||
                                    datastoreIsListed(data.second, user.second, requester));
                        break;
                    case WhichSources::Restorable:
                    {
                        auto dict = dynamic_cast<UserData::JSONData *> (data.second);
                        if (dict) {
                            selected = strcasecmp (dict->getOr ("persistence", "remember"), "restore") == 0;
                        }
                        break;
                    }
                }
                if (selected) {
                    matches.push_back (StoredSourcePair (user.second, data.second));
                }
            }
        }
    }
    return matches;
}

/** Find stored source parameters for use.  Success requires the
    stored source exists and is eligible for use by the requester.
    @param type The type of source.
    @param name The name of the source.
    @param forWho The user that wants to use the source.
    @param[out] found The source parameters retrieved.
    @param[out] owner The owner of the retrieved source.
    @return S_OK on success, or an appropriate error code. */
RESPONSE_CODE UserManager::findStoredSource (const std::string &type,
                                           const std::string &name,
                                           User *forWho,
                                           UserData::JSONData **found,
                                           User **owner) {
    // Check current user first
    if (!forWho)
        return E_LOGINREQUIRED;

    *owner = forWho;
    *found = dynamic_cast<UserData::JSONData *> (forWho->getData (type, name));
    if (!*found) {
        for (auto const &user : *user_manager) {
            *owner = user.second;
            *found = dynamic_cast<UserData::JSONData *> ((*owner)->getData (type, name));
            if (*found)
                break;
        }
    }
    if (!*found)
        return E_NOTFOUND;

    return (datastoreIsListed (*found, *owner, forWho) ? S_OK : E_UNAUTHORIZED);
}

UserManager::~UserManager () {
    for (auto &user : *this) {
        delete user.second;
    }
};

/* Global */ UserManager *user_manager; // The single declaration. 

