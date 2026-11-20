///
/// Class for source parameters and their parsing.
/// @file       mediaparameters.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-01-28
/// @copyright  Copyright (c) 2015-2021 Devious Fish. All rights reserved.
///

#include <config.h>
#include <football/football.h>

#include "fundamentals.h"
#include "datastore.h"
#include "user.h"
#include "users.h"
#include "mediaparameters.h"
#include "datastore.h"


#define KEY_PERSISTENCE "persistence"
#define KEY_SOURCE_NAME "name"
#define KEY_OWNER_USER "owner"
#define KEY_WAIT_FLAG "wait"
#define KEY_ACCESS_MODE "accessMode"
#define KEY_SONG_PROXY_MODE "songProxy"


namespace Media {

    LookupTable<Media::ProxyMode> ProxyWords {
        { "none",      ProxyMode::None },
        { "donor",     ProxyMode::Donor },
        { "recipient", ProxyMode::Recipient }
    };

    LookupTable<Media::PersistenceMode> PersistenceWords {
        { "none",       PersistenceMode::Temporary },
        { "remember",   PersistenceMode::Remember },
        { "restore",    PersistenceMode::Restore }
    };

    const Parsnip::OptionParser::Definitions &SourceParameters::parser_definitions() {
        /** Parse definitions for the parameter parser */
        static const Parsnip::OptionParser::Definitions standard_options = {
            "<" KEY_PERSISTENCE ":remember>",
            "<" KEY_PERSISTENCE ":restore>",
            "<" KEY_WAIT_FLAG ":wait>",
            "owned by {" KEY_OWNER_USER "}",
            "name {" KEY_SOURCE_NAME "}",
            "[access] <" KEY_ACCESS_MODE ":disowned|private|shared|published|public>",
            "song proxy <" KEY_SONG_PROXY_MODE ":none|recipient|donor>"
        };
        return standard_options;
    }


    void SourceParameters::extractOptions (const Parsnip::Data &options) {
        extract (options);
    }
    
    
    void SourceParameters::extract (const Parsnip::Data &options) {
        if (options.contains (KEY_PERSISTENCE)) {
            persistence = PersistenceWords [options [KEY_PERSISTENCE].asString()];
        }
        if (options.contains (KEY_WAIT_FLAG)) {
            waitForReady = true;
        }
        if (options.contains (KEY_OWNER_USER)) {
            std::string owner_name = options [KEY_OWNER_USER].asString();
            owner = user_manager->get (owner_name);
            userSpecified = true;
        }
        if (options.contains (KEY_ACCESS_MODE)) {
            permissions = OWNERSHIP [options [KEY_ACCESS_MODE].asString()];
            if (permissions == Ownership::Type::DISOWNED) {
                owner = nullptr;
            }
        }
        if (options.contains (KEY_SOURCE_NAME)) {
            name = options [KEY_SOURCE_NAME].asString();
            for (auto ch : name) {
                if (!isalnum (ch)) {
                    throw CommandError (E_INVALID, name);
                }
            }
        }
        if (name.empty()) {
            throw (CommandError (E_INVALID, "Empty source name"));
        }
        if (options.contains (KEY_SONG_PROXY_MODE)) {
            song_proxy = ProxyWords [options [KEY_SONG_PROXY_MODE].asString()];
        }
        if (permissions == Ownership::Type::DISOWNED) {
            if (userSpecified) {
                throw CommandError (E_CONFLICT, "Disowned source has owner");
            } else if (persistence != PersistenceMode::Temporary){
                throw CommandError (E_CONFLICT, "Disowned source must be temporary");
            }
        }
    }



    bool SourceParameters::persist (UserData::JSONData &dest) const {
        assert (persistence != PersistenceMode::Temporary);
        dest [KEY_SOURCE_NAME] = name;
        dest [KEY_ACCESS_MODE] = OWNERSHIP [permissions];
        dest [KEY_SONG_PROXY_MODE] = ProxyWords [song_proxy];
        dest [KEY_PERSISTENCE] = PersistenceWords [persistence];
        return !name.empty();
    }
    
    SourceParameters::SourceParameters (Ownership::Type perm, User *user) {
        permissions = perm;
        owner = user;
        if (user) {
            name = user->username();
        }
    }

    SourceParameters::SourceParameters (Ownership::Type perm,
                                        const std::string &sourcename) {
        assert (!sourcename.empty());
        permissions = perm;
        name = sourcename;
    };

    SourceParameters::SourceParameters (const UserData::JSONData &src) {
        extract (src);
    }
    
    SourceParameters::~SourceParameters () {
    }
}


