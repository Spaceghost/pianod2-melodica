///
/// User management provider.
/// @file       users.h - pianod project
/// @author     Perette Barella
/// @date       2012-03-20
/// @copyright  Copyright 2012-2020 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>
#include <map>
#include <vector>
#include <functional>

#include <football/fb_public.h>
#include <football/football.h>
#include <parsnip/parsnip.h>
#include <parsnip/parsnip_command.h>

#include "user.h"
#include "interpreter.h"

class PianodConnection;
class PianodService;

Response send_privileges (const User *user);

/// Container and manager class for users.
class UserManager : public PianodInterpreter, private std::map<std::string, User *> {
private:
    std::string shadow_user_name;
public:
    enum class WhichSources {
        User,           ///< Sources owned by a specific user.
        Listed,         ///< Sources that are publicly known
        Restorable      ///< Sources for automatic restore
    };
    
    static const Parsnip::Parser::Definitions &parser_definitions ();
    virtual const Parsnip::Parser::Definitions &getParserDefinitions () override;
    static const PianodSchema::CommandIds &json_request_names (PianodSchema &schema);
private:
    virtual bool authorizedCommand (Parsnip::Parser::CommandId, PianodConnection &conn) override;
    virtual ResponseCollector handleCommand (Parsnip::Parser::CommandId, const Parsnip::Data &options, PianodConnection &conn) override;
public:

    virtual ~UserManager ();

    using UserSelectionPredicate = std::function<bool (const User *user)>;
    using StoredSourcePair = std::pair<User *, UserData::DataStore *>;
    using StoredSourceList = std::vector<StoredSourcePair>;
    static inline time_t writeTime (void) { return User::write_time; };
    static inline bool needWrite (void) {
        return User::write_time != 0 && User::write_time > time(nullptr);
    };
    const std::string &shadowUserName () const;
    void shadowUserName (const std::string &who);

    User *addUser (User &user);
    User *tryget (const std::string &who);
    User *get (const std::string &who);
    User *authenticate (const std::string &who, const std::string &password);
    void clearPrivilege (Privilege priv);
    bool validUserList (Football::Connection *event, char *const *username);

    std::vector<PianodConnection *> getUserConnections (PianodService &service,
                                                        const User *user) const;
    UserList getUsers (UserSelectionPredicate predicate = [] (const User *) { return true; }) const;
    UserList getUsersPresent (PianodService &service,
                              bool use_attribute = false) const;
    UserList allUsers (void) const { return getUsers (); };
    StoredSourceList getStoredSources (WhichSources selection,
                                       const User *visibleBy = nullptr) const;
    RESPONSE_CODE findStoredSource (const std::string &type,
                                    const std::string &name,
                                    User *forWho,
                                    UserData::JSONData **found,
                                    User **owner);
    void createDefaultUser ();
    void deleteUser (User *user);
    bool persist ();
    void restore ();
    float periodic (void);
};

extern /* Global */ UserManager *user_manager;

