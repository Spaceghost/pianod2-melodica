///
/// Access control for objects.
/// @file       ownership.cpp - pianod
/// @author     Perette Barella
/// @date       2014-12-28
/// @copyright  Copyright 2014-2016 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <cassert>

#include <string>

#include "ownership.h"
#include "users.h"


/** Default construct ownership: 
    No owner, private access. */
PrimaryOwnership::PrimaryOwnership (void) {

}

/** Construct ownership.
    @param rule Access policy for non-owners.
    @param owneruser The owner. */
PrimaryOwnership::PrimaryOwnership (Type rule, User *owneruser) {
    access = rule;
    owner = owneruser;
};

PrimaryOwnership::~PrimaryOwnership (void) {
    abandon ();
}

/** Retrieve the name of the owner.
    @return The user's name, or text indicating no ownership. */
const std::string &PrimaryOwnership::ownerName (void) const {
    static const std::string noowner = "(Unowned)";
    if (owner) return owner->username();
    return noowner;
};

/** Disassociate ownership from any owner, and mark as disowned. */
void PrimaryOwnership::abandon (void) {
    owner = nullptr;
    access = Type::DISOWNED;
}

/** Conditionally dissociate ownership.
    @param user The user attempting dissociation.
    Dissociation is performed only if this is the user on file. */
void PrimaryOwnership::abandon (const User *user) {
    if (user == owner) {
        owner = nullptr;
        access = Type::DISOWNED;
    }
}

/** Check if we are owned by a user.
    @param user The user to compare ownership to.
    @return True if owned by the indicated user, false otherwise. */
bool PrimaryOwnership::isOwnedBy (const User *user) const {
    assert (access != Type::DISOWNED || owner == nullptr);
    return (user != nullptr && owner == user);
}

/** Determine if a user has permission to use this item in some manner.
    @param user The user attempting use.
    @param action The type of action the user is performing.
    @return True if the user owns this item, or the user's chosen
    access policy allows others to use the item for the requested action. */
bool PrimaryOwnership::hasPermission (const User *user, Action action) const {
    assert (access != Type::DISOWNED || owner == nullptr);
    // We are comparing two different enums here.
    // See Ownership::Type and Ownership::Action.
    // That the comparator is < and not <= is intentional.
    return ((user != nullptr && owner == user) ||
            (int (action) < int (access)));
}

bool SubordinateOwnership::isOwnedBy (const User *user) const {
    if (!parentOwner()) return false;
    return parentOwner()->isOwnedBy (user);
}

bool SubordinateOwnership::hasPermission (const User *user, Action action) const {
    if (!parentOwner()) return false;
    return parentOwner()->hasPermission (user, action);
}

/// Lookup able for ownership policies.
OwnershipLookup OWNERSHIP ( {
    { "disowned",       Ownership::Type::DISOWNED },
    { "private",        Ownership::Type::PRIVATE },
    { "shared",         Ownership::Type::SHARED },
    { "published",      Ownership::Type::PUBLISHED },
    { "public",         Ownership::Type::PUBLIC }
} );
