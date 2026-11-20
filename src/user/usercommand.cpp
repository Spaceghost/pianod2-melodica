///
/// Command handlers for all things user-related.
/// @file       usercommand.cpp - pianod project
/// @author     Perette Barella
/// @date       Initial: 2012-03-10.  C++: 2014-11-22.
/// @copyright  Copyright 2012-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cstdio>
#include <cassert>

#include <vector>

#include <football/football.h>
#include <parsnip/parsnip.h>
#include <parsnip/parsnip_command.h>
#include <parsnip/parsnip_schema.h>

#include "fundamentals.h"
#include "utility.h"
#include "enum.h"
#include "interpreter.h"
#include "response.h"
#include "user.h"
#include "users.h"
#include "servicemanager.h"
#include "mediamanager.h"
#include "engine.h"


/** Assemble a user's privileges into a response.
    @param user User whose privileges to send. */
Response send_privileges (const User *user) {
    assert (user);
    Response::List privs;
    Parsnip::Data privs_json {Parsnip::Data::Dictionary,
        "rank", user->getRankName()
    };
    privs.push_back (user->getRankName());
    for (Privilege p : Enum<Privilege>()) {
        privs_json [User::getPrivilegeName (p)] = user->havePrivilege (p);
        if (user->havePrivilege (p)) {
            privs.push_back (User::getPrivilegeName (p));
        }
    }
    return Response (I_USER_PRIVILEGES, std::move (privs), std::move (privs_json));
}

/** Check a list of users and record errors.
    @param users The list of users.
    @return A CommandReply recording any invalid users encountered. */
static CommandReply validate_user_list (const std::vector<std::string> &users) {
    CommandReply response (CommandReply::Aggregation::PESSIMISTIC);
    for (const std::string &username : users) {
        if (user_manager->tryget (username) == NULL) {
            response.fail (username, E_NOTFOUND);
        };
    }
    return response;
}

/** Set the requested privilege for a list of users.
    @param users A list of usernames.
    @param priv The privileges to set.
    @param setting Whether to enable (true) or disable (false).
 */
static void set_privileges (const std::vector<std::string> &users, Privilege priv, bool setting) {
    assert (Enum<Privilege>().isValid (priv));
    for (std::string username : users) {
        User *u = user_manager->get (username);
        u->setPrivilege (priv, setting);
    }
}

/** Return a user record in transmittable form, either
    just name or with details.
    @param user The user to send information of.
    @param details Whether to send privileges, etc.
    @return User details arranged for transmittal. */
ResponseGroup send_user (const User *user, bool details) {
    ResponseGroup response;
    response (Response (I_ID, user->username()));
    if (details) {
        response (send_privileges (user));
    }
    return response;
}

/** Prepare lists of users for transmission based on privilege or state criteria.
    @warning The pseudoenumeration values for `which` must not overlap.
    @param which A list of users to send.
    @param details Whether to include privileged information in the list.
    @return User details arranged for transmittal. */
static DataResponse send_users (UserList which, bool details) {
    DataResponse response;
    for (auto user : which) {
        response.data (send_user (user, details));
    }
    return response;
}

/** Prepare lists of users for transmission.
    @param conn The connection to which the tranmission is being sent.  If
    an administrator, details are included; otherwise omitted.
    @param which A list of users to send.
    @return User details arranged for transmittal. */
static DataResponse send_users (PianodConnection &conn, UserList which) {
    return send_users (which, conn.haveRank (Rank::Administrator));
}

/** Logoff users.
    @param service The service/room to logoff user in, or null to logoff everywhere.
    @param user The user to logoff, or nullptr to logoff visitors/unauthenticated users.
    @param message An optional message to send before disconnecting. */
static bool user_logoff (PianodService *service, User *user, const std::string &message) {
    bool any = false;
    if (!service) {
        for (auto const &svc : *service_manager) {
            any = user_logoff (svc.second, user, message) || any;
        }
    } else {
        for (auto conn : *service) {
            if (conn->user == user) {
                conn << Response (V_SERVER_STATUS, message.empty() ? "Logged off by an administrator" : message);
                any = true;
                conn->close();
            }
        }
    }
    return any;
}

#define RANK_PATTERN "<rank:disabled|listener|user|admin>"
#define PRIV_PATTERN "<privilege:service|deejay|influence|tuner>"

typedef enum user_commands_t {
    AUTHENTICATE = CMD_RANGE_USER,
    AUTHANDEXEC,
    SETMYPASSWORD,
    USESHADOWPASSWORD,
    USERCREATE,
    USERSETPASSWORD,
    GETVISITORRANK,
    SETVISITORRANK,
    GETSHADOWUSERNAME,
    SETSHADOWUSERNAME,
    GETUSERRANK,
    USERSETRANK,
    USERDELETE,
    USERGRANT,
    USERREVOKE,
    USERLISTBYPRIVILEGE,
    USERLIST,
    USERKICK,
    USERKICKVISITORS,
    USERSONLINE,
    USERSINROOM,
    AUTOTUNEUSERS,
    AUTOTUNEUSERSLIST,
    AUTOTUNEADDREMOVE,
} COMMAND;

const Parsnip::Parser::Definitions &UserManager::parser_definitions() {
    static const Parsnip::Parser::Definitions user_statements{
        { AUTHENTICATE, "user {username} {password}" },                   // Authenticate
        { AUTHANDEXEC, "as user {username} {password} [{command...}]" },  // Authenticate and execute command.
        { GETUSERRANK, "get privileges" },                                // Request user's level/privileges
        { SETMYPASSWORD, "set password {old} {new}" },                    // Let a user update their password
        { USESHADOWPASSWORD, "set shadow password {pianod} {system}" },   // Switch from custom to shadow password

        { GETVISITORRANK, "get visitor rank" },                    // Visitor privilege level
        { SETVISITORRANK, "set visitor rank " RANK_PATTERN },      // Visitor privilege level
        { GETSHADOWUSERNAME, "get shadow user name" },             // Query the shadow user's username
        { SETSHADOWUSERNAME, "set shadow user name {username}" },  // Select a template for new shadow users

        { USERCREATE, "create <rank:listener|user|admin> {user} {password}" },  // Add a new user
        { USERSETPASSWORD, "set user password {user} {password}" },             // Change a user's password
        { USERSETRANK, "set user rank {user} " RANK_PATTERN },                  // Alter rank
        { USERDELETE, "delete user {user}" },                                   // Remove a user
        { USERGRANT, "grant " PRIV_PATTERN " to {user} ..." },                  // Grant privilege
        { USERREVOKE, "revoke " PRIV_PATTERN " from {user} ..." },              // Revoke privilege
        { AUTOTUNEUSERS, "autotune for [{user}] ..." },                         // Tune for a list of users.
        { AUTOTUNEUSERSLIST, "autotune list users" },                           // List users considered by autotuning
        { AUTOTUNEADDREMOVE, "autotune <action:consider|disregard> {user} ..." },  // Add users to tuning list.
        { USERLISTBYPRIVILEGE, "users with <attribute:owner|service|influence|tuner|present>" },
        // List users with a privilege
        { USERLIST, "users list [{user}]" },                                     // List all or a specific user
        { USERSONLINE, "users online" },                                         // List users logged in
        { USERSINROOM, "users in room" },                                        // Users using current room
        { USERKICK, "kick [region:all|room] user {user} [{message...}]" },       // Log a user off
        { USERKICKVISITORS, "kick [region:all|room] visitors [{message...}]" },  // Disconnect unauthenticated
    };
    return user_statements;
}

const Parsnip::Parser::Definitions &UserManager::getParserDefinitions() {
    return parser_definitions();
}

/** Retrieve names for our JSON requests, and make schema revisions.
    @param schema The schemaset containing our requests.
    @return Request name to command ID mappings. */
const PianodSchema::CommandIds &UserManager::json_request_names (PianodSchema &schema) {
    static const PianodSchema::CommandIds mappings{ { "authenticate", AUTHENTICATE },
                                                    { REQUEST_ASUSER, AUTHANDEXEC },
                                                    { "getPrivileges", GETUSERRANK },
                                                    { "setPassword", SETMYPASSWORD },
                                                    { "switchToSystemPassword", USESHADOWPASSWORD },
                                                    { "getVisitorRank", GETVISITORRANK },
                                                    { "setVisitorRank", SETVISITORRANK },
                                                    { "getShadowUserName", GETSHADOWUSERNAME },
                                                    { "setShadowUsername", SETSHADOWUSERNAME },
                                                    { "createUser", USERCREATE },
                                                    { "setUserPassword", USERSETPASSWORD },
                                                    { "setUserRank", USERSETRANK },
                                                    { "deleteUser", USERDELETE },
                                                    { "grantUserPrivilege", USERGRANT },
                                                    { "revokeUserPrivilege", USERREVOKE },
                                                    { "setAutotuneUsers", AUTOTUNEUSERS },
                                                    { "getAutotuneUsers", AUTOTUNEUSERSLIST },
                                                    { "adjustAutotuneUsers", AUTOTUNEADDREMOVE },
                                                    { "getUserByPrivilege", USERLISTBYPRIVILEGE },
                                                    { "getUserList", USERLIST },
                                                    { "getUsersOnline", USERSONLINE },
                                                    { "getUsersInRoom", USERSINROOM },
                                                    { "logoffUsers", USERKICK },
                                                    { "logoffVisitors", USERKICKVISITORS } };
    schema.removeMember (AUTHANDEXEC, KEY_COMMAND);
    schema.addMember (AUTHANDEXEC, KEY_REQUEST, Parsnip::UncheckedSchema{});
    return mappings;
}

bool UserManager::authorizedCommand (Parsnip::Parser::CommandId command, PianodConnection &conn) {
    switch (command) {
        case AUTHENTICATE:
        case AUTHANDEXEC:
        case GETUSERRANK:
        case SETMYPASSWORD:
        case USESHADOWPASSWORD:
            return true;

        case AUTOTUNEUSERSLIST:
            // If sharing user actions, anyone can see who influences autotuning
            if (service_manager->broadcastingActions())
                return true;
            // FALLTHRU
        case AUTOTUNEADDREMOVE:
        case AUTOTUNEUSERS:
            return conn.havePrivilege (Privilege::Tuner);

        case USERSONLINE:
        case USERLIST:
            // If sharing user actions, anyone can view users since we announce them anyway.
            // But, never allow search by privilege since that would breach secrets.
            // Admins can always see users online and get their privileges.
            if (service_manager->broadcastingActions())
                return true;
            // FALLTHRU
        default:
            return conn.haveRank (Rank::Administrator);
    }
};

ResponseCollector UserManager::handleCommand (Parsnip::Parser::CommandId command,
                                              const Parsnip::Data &options,
                                              PianodConnection &conn) {
    switch (command) {
        case AUTHENTICATE: {
            conn.user = user_manager->authenticate (options ["username"].asString(), options ["password"].asString());
            if (!conn.authenticated()) {
                throw CommandError (E_CREDENTIALS);
            }
            CommandReply response (S_OK);
            response.broadcast_events (A_SIGNED_IN);
            response.broadcast_events (V_PLAYLISTRATING_CHANGED);
            conn.service().usersChangedNotification();
            response.information (conn.updateConnection());
            response.information (conn.sendEffectivePrivileges());
            return std::move (response);
        }
        case GETUSERRANK: {
            CommandReply response {S_OK};
            response.information (conn.sendEffectivePrivileges());
            return std::move (response);
        }
        case AUTHANDEXEC: {
            ResponseCollector response;
            conn.user = user_manager->authenticate (options ["username"].asString(), options ["password"].asString());
            try {
                if (!conn.authenticated()) {
                    throw CommandError (E_BAD_COMMAND);
                }
                response = conn.service().dispatch.redispatch (options, conn);
            } catch (...) {
                response = CommandReply (std::current_exception());
            }
            response.close();
            return response;
        }
        case USESHADOWPASSWORD:
#ifdef SHADOW_CAPABLE
            if (!conn.authenticated()) {
                throw CommandError (E_LOGINREQUIRED);
            } else if (conn.user->assumeShadowPassword (options ["pianod"].asString(), options ["system"].asString())) {
                return S_OK;
            } else {
                throw CommandError (E_CREDENTIALS);
            };
#else
            throw CommandError (E_UNSUPPORTED);
            return;
#endif
        case SETMYPASSWORD:
            if (!conn.authenticated()) {
                throw CommandError (E_LOGINREQUIRED);
            } else if (conn.user->changePassword (options ["old"].asString(), options ["new"].asString())) {
                return S_OK;
            }
            throw CommandError (E_CREDENTIALS);
        case GETVISITORRANK:
            return Response (I_USER_PRIVILEGES, User::getRankName (User::getVisitorRank()));
        case SETVISITORRANK:
            User::setVisitorRank (options ["rank"].asString());
            service_manager->broadcastEffectivePrivileges (nullptr);
            return S_OK;
        case GETSHADOWUSERNAME: {
            const std::string &shadow = user_manager->shadowUserName();
            if (shadow.empty()) {
                throw CommandError (E_NOTFOUND);
            }
            return Response (I_NAME, shadow);
        }
        case SETSHADOWUSERNAME: {
            const std::string &name = options ["username"].asString();
            user_manager->shadowUserName (name);
            return S_OK;
        }

        case USERCREATE: {
            // Create a new user with a rank and password
            User newuser (options ["user"].asString(), options ["password"].asString(), true);
            kept_assert (newuser.assignRank (options ["rank"].asString()));
            if (user_manager->addUser (newuser)) {
                return S_OK;
            }
            throw CommandError (E_DUPLICATE);
        }
        case USERSETPASSWORD: {
            User *revise = user_manager->get (options ["user"].asString());
            if (revise) {
                revise->setPassword (options ["password"].asString());
                return S_OK;
            }
            throw CommandError (E_NOTFOUND);
        }
        case USERSETRANK: {
            User *revise = user_manager->get (options ["user"].asString());
            if (revise) {
                kept_assert (revise->assignRank (options ["rank"].asString()));
                service_manager->broadcastEffectivePrivileges (revise);
                return S_OK;
            }
            throw CommandError (E_NOTFOUND);
        }
        case USERGRANT:
        case USERREVOKE: {
            auto userlist = options ["user"].as<std::vector<std::string>>();
            ResponseCollector response (validate_user_list (userlist));
            if (!response.anyFailure()) {
                Privilege privilege = User::getPrivilege (options ["privilege"].asString());
                set_privileges (userlist, privilege, command == USERGRANT);
                if (privilege == Privilege::Influence) {
                    conn.service().usersChangedNotification();
                }
                service_manager->broadcastEffectivePrivileges();
                return S_OK;
            }
            return response;
        }

        case AUTOTUNEUSERSLIST:
            return send_users (conn, conn.service().audioEngine()->getAutotuneUsers());
        case AUTOTUNEUSERS:
        case AUTOTUNEADDREMOVE: {
            auto userlist = options ["user"].as<std::vector<std::string>>();
            CommandReply response = validate_user_list (userlist);
            if (!response.anyFailure()) {
                if (command == AUTOTUNEUSERS) {
                    user_manager->clearPrivilege (Privilege::Present);
                }
                set_privileges (userlist,
                                Privilege::Present,
                                command == AUTOTUNEUSERS || optionIs (options, "action", "consider"));
                response.succeed();
                conn.service().usersChangedNotification();
            }
            return std::move (response);
        }

        case USERSONLINE:
            return send_users (conn, user_manager->getUsers ([] (const User *user) -> bool {
                return service_manager->userIsOnline (user);
            }));
        case USERSINROOM: {
            PianodService &service = conn.service();
            return send_users (conn, user_manager->getUsers ([&service] (const User *user) -> bool {
                return user->online (service);
            }));
        }
        case USERLIST: {
            if (options.contains ("user")) {
                User *user = user_manager->get (options ["user"].asString());
                return send_user (user, conn.haveRank (Rank::Administrator));
            }
            return send_users (conn, user_manager->getUsers());
        }
        case USERLISTBYPRIVILEGE: {
            // haveRank: Only reveal privileges to administrators
            Privilege priv = User::getPrivilege (options ["attribute"].asString());
            return send_users (conn, user_manager->getUsers ([priv] (const User *user) -> bool {
                return user->havePrivilege (priv);
            }));
        }

        case USERKICKVISITORS:
        case USERKICK: {
            User *target = nullptr;
            std::string whom{ "the visitors" };
            if (command == USERKICK) {
                whom = options ["user"].asString();
                target = user_manager->get (whom);
            }
            bool all_rooms = optionIs (options, "region", "all");
            if (user_logoff (all_rooms ? nullptr : &conn.service(), target, options.getOr ("message", ""))) {
                CommandReply response (S_OK);
                response.broadcast_events (Response (A_KICKED, whom));
                return std::move (response);
            }
            throw CommandError (E_WRONG_STATE, "Target not online.");
        }
        case USERDELETE: {
            User *remove = user_manager->get (options ["user"].asString());
            if (!remove) {
                throw CommandError (E_NOTFOUND);
            }
            if (service_manager->userIsOnline (remove)) {
                throw CommandError (E_WRONG_STATE, "User is logged in.");
            }
            // Disown the user's sources before deleting user
            for (auto &src : *media_manager) {
                if (src.second->isOwnedBy (remove)) {
                    src.second->abandon();
                }
            }
            user_manager->deleteUser (remove);
            return S_OK;
        }
        default:
            flog (LOG_WHERE (Log::WARNING), "Unimplemented command ", command);
            throw CommandError (E_NOT_IMPLEMENTED);
    }
}
