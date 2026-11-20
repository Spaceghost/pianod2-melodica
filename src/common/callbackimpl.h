///
/// Callback manager implementation.
/// Allow registration, removal, and invokation of callback methods.
/// In Devious Fish parlance, we have 3 types of callbacks:
/// - Notifications inform that something happened, triggering some action.
/// Handlers have no return value and cannot modify arguments.
/// - Delegates decide something.  They typically return boolean but aren't
/// restricted to this; like notifications, they can't modify arguments.
/// - Callbacks manipulate.  Their arguments are maleable.
/// @file       callbackimpl.h - pianod
/// @author     Perette Barella
/// @date       2017-11-20
/// @copyright  Copyright (c) 2017-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <functional>
#include <utility>

/** Invoke a notification function.
    @param function A TCallbacks member selector for the callback to be invoked.
    @param arguments Any arguments to be passed to the callback functions. */
template <typename TOwner, typename TCallbacks, typename TCallbackClientId>
template <typename TMemberFunction, typename... Arguments>
void CallbackManager<TOwner, TCallbacks, TCallbackClientId>::notify (TMemberFunction function, const Arguments &... arguments) {
    for (const auto &handler : registered_handlers) {
        if (handler.second.*function) {
            (handler.second.*function) (arguments...);
        }
    }
};


/** Invoke a callback function.
    @param function A TCallbacks member selector for the callback to be invoked.
    @param arguments Any arguments to be passed to the callback functions. */
template <typename TOwner, typename TCallbacks, typename TCallbackClientId>
template <typename TMemberFunction, typename... Arguments>
void CallbackManager<TOwner, TCallbacks, TCallbackClientId>::callback (TMemberFunction function, Arguments &&... arguments) {
    for (const auto &handler : registered_handlers) {
        if (handler.second.*function) {
            (handler.second.*function) (std::forward<Arguments> (arguments)...);
        }
    }
};

/** Invoke a delegate function and aggregate the results.
    @tparam TReturnType The type returned by the delegates, and aggregated and returned by this method.
    @param default_result The result returned if there are no registered delegates.
    @param combine A function that combines results when there are multiple delegates.
    @param function A TCallbacks member selector for the callback to be invoked.
    @param arguments Any arguments to be passed to the callback functions. */
template <typename TOwner, typename TCallbacks, typename TCallbackClientId>
template <typename TReturnType, typename TMemberFunction, typename... Arguments>
TReturnType CallbackManager<TOwner, TCallbacks, TCallbackClientId>::aggregate (TReturnType default_result,
                                                                               const std::function<TReturnType (TReturnType, TReturnType)> &combine,
                                                                               TMemberFunction function, const Arguments &... arguments) {
    for (auto handler = registered_handlers.begin(); handler != registered_handlers.end(); handler++) {
        if (handler->second.*function) {
            TReturnType computed_result = (handler->second.*function) (arguments...);
            while (++handler != registered_handlers.end()) {
                if (handler->second.*function) {
                    computed_result = combine ((handler->second.*function) (arguments...), computed_result);
                }
            }
            return computed_result;
        }
    }
    return default_result;
};

/** Check for approval from all delegates.  All delegates are queried.
    @param default_result The result in the event there are no delegates.
    @param function A TCallbacks member selector for the delegate to be queried.
    @param arguments Any arguments to be passed to delegates.
    @return True if all delegates return true, false if any return false. */
template <typename TOwner, typename TCallbacks, typename TCallbackClientId>
template <typename TMemberFunction, typename... Arguments>
inline bool CallbackManager<TOwner, TCallbacks, TCallbackClientId>::queryUnanimousApproval (bool default_result, TMemberFunction function, const Arguments &... arguments) {
    return aggregate (default_result, std::function<bool (bool, bool)> (std::logical_and <bool>()),
                      function, arguments...);
};

/** Check for approval by any delegate.
    @param default_result The result in the event there are no delegates.
    @param function A TCallbacks member selector for the delegate to be queried.
    @param arguments Any arguments to be passed to delegates.
    @return True if any delegate returns true, false otherwise. */
template <typename TOwner, typename TCallbacks, typename TCallbackClientId>
template <typename TMemberFunction, typename... Arguments>
inline bool CallbackManager<TOwner, TCallbacks, TCallbackClientId>::queryAnyApproval (bool default_result, TMemberFunction function, const Arguments &... arguments) {
    return aggregate (default_result, std::function<bool (bool, bool)> (std::logical_or <bool>()),
                      function, arguments...);
};

/** Register callback, delegate, or notification functions.
    @param client_id A unique ID for the client.  Suggest using its 'this' object value.
    @param handlers The handler functions to invoke for callbacks. */
template <typename TOwner, typename TCallbacks, typename TCallbackClientId>
void CallbackManager<TOwner, TCallbacks, TCallbackClientId>::subscribe (const TCallbackClientId client_id, const TCallbacks &handlers) {
    registered_handlers [client_id] = handlers;
}

/** Remove registered callbacks, delegates, or notifications.
    @param client_id The ID the client registered its callback with. */
template <typename TOwner, typename TCallbacks, typename TCallbackClientId>
void CallbackManager<TOwner, TCallbacks, TCallbackClientId>::unsubscribe (const TCallbackClientId client_id) {
    registered_handlers.erase (client_id);
}


