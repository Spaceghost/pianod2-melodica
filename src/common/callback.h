///
/// Callback manager.
/// Allow registration, removal, and invokation of callback methods.
/// @file       callback.h - pianod
/// @author     Perette Barella
/// @date       2017-11-20
/// @copyright  Copyright (c) 2017-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <unordered_map>
#include <functional>

/** A template class to manage callback, delegate and notification functions.
    @tparam TOwner CallbackManager is expected to be a nested class.  TOwner is the enclosing class.
    @tparam TCallbacks A structure with callback hooks as members.
    @tparam TCallbackClientId Unique ID to allow update or removal of hooks.
    Typically the caller's 'this'. */
template <typename TOwner, typename TCallbacks, typename TCallbackClientId = void *>
class CallbackManager {
    friend TOwner;

    /// The list of subscribers to the callbacks.
    std::unordered_map<TCallbackClientId, TCallbacks> registered_handlers;

    template<typename TMemberFunction, typename... Arguments>
    void notify (TMemberFunction function, const Arguments &... arguments);


    template<typename TMemberFunction, typename... Arguments>
    void callback (TMemberFunction function, Arguments &&... arguments);

    template <typename TReturnType, typename TMemberFunction, typename... Arguments>
    TReturnType aggregate (TReturnType result, const std::function<TReturnType (TReturnType, TReturnType)> &combine,
                           TMemberFunction function, const Arguments &... arguments);

    template<typename TMemberFunction, typename... Arguments>
    bool queryUnanimousApproval (bool default_result, TMemberFunction function, const Arguments &... arguments);

    template<typename TMemberFunction, typename... Arguments>
    bool queryAnyApproval (bool default_result, TMemberFunction function, const Arguments &... arguments);

public:
    void subscribe (const TCallbackClientId client_id, const TCallbacks &handlers);
    void unsubscribe (const TCallbackClientId client_id);
};

