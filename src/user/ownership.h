///
/// Access control for objects.
/// @file       ownership.h - pianod
/// @author     Perette Barella
/// @date       2014-12-28
/// @copyright  Copyright (c) 2014-2016 Devious Fish. All rights reserved.
///

#ifndef __pianod__ownership__
#define __pianod__ownership__

#include <config.h>

#include "fundamentals.h"
#include "lookup.h"

/// Ownership where privilege is defined within the object.
class PrimaryOwnership : public Ownership {
private:
    Type access = Type::DISOWNED;
    User *owner = nullptr;
public:
    PrimaryOwnership (void);
    PrimaryOwnership (Type rule, User *owner = nullptr);
    ~PrimaryOwnership (void);
    void abandon (void);
    void abandon (const User *user);
    User *getOwner (void) const { return owner; };
    inline bool isOwned (void) const { return owner != nullptr; };
    const std::string &ownerName (void) const;
    virtual bool isOwnedBy (const User *user) const override;
    virtual bool hasPermission (const User *user, Action action) const override;
};


/// Ownership where privilege is regulated by some parent object.
class SubordinateOwnership : public Ownership {
protected:
    virtual Ownership *parentOwner (void) const = 0;
public:
    virtual bool isOwnedBy (const User *user) const override;
    virtual bool hasPermission (const User *user, Action action) const override;
};


typedef const LookupTable<Ownership::Type> OwnershipLookup;
extern OwnershipLookup OWNERSHIP;


#endif // defined(__pianod__ownership__) 
