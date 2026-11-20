///
/// Callback manager tests.
/// Validate all methods provided by the callback manager.
/// @file       callback_test.cpp - pianod
/// @author     Perette Barella
/// @date       2017-11-20
/// @copyright  Copyright (c) 2017-2021 Devious Fish. All rights reserved.
///


#ifdef NDEBUG
#warning Disabling assertion suppression for unit test.
#undefine NDEBUG
#endif

#include <assert.h>

#include <iostream>
#include <functional>

#include "callback.h"
#include "callbackimpl.h"


class CallbackProvider {
public:
    struct Callbacks {
        std::function<bool (bool)> delegate;
        std::function<void (int &count)> callback;
        std::function<int (int value)> aggregator;
    };

    CallbackManager<CallbackProvider, Callbacks> callbacks;

    void run_object_test (int callback_count) {
        // For delegate, these are really pretty trivial cases.
        assert (callbacks.queryUnanimousApproval(true, &Callbacks::delegate, true) == true);
        assert (callbacks.queryUnanimousApproval(false, &Callbacks::delegate, true) == (callback_count ? true : false));
        assert (callbacks.queryUnanimousApproval(true, &Callbacks::delegate, false) == (callback_count ? false : true));
        assert (callbacks.queryUnanimousApproval(false, &Callbacks::delegate, false) == false);

        assert (callbacks.queryAnyApproval(true, &Callbacks::delegate, true) == true);
        assert (callbacks.queryAnyApproval(false, &Callbacks::delegate, true) == (callback_count ? true : false));
        assert (callbacks.queryAnyApproval(true, &Callbacks::delegate, false) == (callback_count ? false : true));
        assert (callbacks.queryAnyApproval(false, &Callbacks::delegate, false) == false);

        // Each callback should increment the counter once.
        int count = 0;
        callbacks.callback (&Callbacks::callback, count);
        assert (count == callback_count);

        // Aggregator should sum up value we provide, so value * #aggregators.
        count = callbacks.aggregate (0, std::function<int (int, int)> (std::plus <int>()),
                                     &Callbacks::aggregator, 1);
        assert (count == callback_count);

        count = callbacks.aggregate (0, std::function<int (int, int)> (std::plus <int>()),
                                     &Callbacks::aggregator, 3);
        assert (count == callback_count * 3);
    }

    void run_nonobject_test (bool have_object_too) {
        // The nonobject delegate returns the opposite of what we ask.
        // Use this behavior to test and/or logic by mixing it with an object delegate.
        assert (callbacks.queryUnanimousApproval(true, &Callbacks::delegate, true) == false);
        assert (callbacks.queryUnanimousApproval(false, &Callbacks::delegate, true) == (have_object_too ? false : false));
        assert (callbacks.queryUnanimousApproval(true, &Callbacks::delegate, false) == (have_object_too ? false : true));
        assert (callbacks.queryUnanimousApproval(false, &Callbacks::delegate, false) == (have_object_too ? false : true));

        assert (callbacks.queryAnyApproval(true, &Callbacks::delegate, true) == (have_object_too ? true : false));
        assert (callbacks.queryAnyApproval(false, &Callbacks::delegate, true) == (have_object_too ? true : false));
        assert (callbacks.queryAnyApproval(true, &Callbacks::delegate, false) == (have_object_too ? true : true));
        assert (callbacks.queryAnyApproval(false, &Callbacks::delegate, false) == true);

        // Each callback should increment the counter once.
        int count = 0;
        callbacks.callback (&Callbacks::callback, count);
        assert (count == (have_object_too ? 2 : 1));

        // Aggregator should sum up value we provide, so value * #aggregators.
        count = callbacks.aggregate (0, std::function<int (int, int)> (std::plus <int>()),
                                     &Callbacks::aggregator, 1);
        assert (count == (have_object_too ? 0 : -1));

        count = callbacks.aggregate (0, std::function<int (int, int)> (std::plus <int>()),
                                     &Callbacks::aggregator, 3);
        assert (count == (have_object_too ? 0 : -3));
    }
};


CallbackProvider provider;


// A callback client that returns what we ask of it.
struct CallbackClient {
    bool delegate (bool desired_return_value) {
        return desired_return_value;
    }

    int callback_call_count = 0;
    void callback (int &count) {
        count++;
        callback_call_count++;
    }

    int aggregator (int value) {
        return value;
    }

    CallbackClient() {
        CallbackProvider::Callbacks callbacks;
        callbacks.delegate = std::bind (&CallbackClient::delegate, this, std::placeholders::_1);
        callbacks.callback = std::bind (&CallbackClient::callback, this, std::placeholders::_1);
        callbacks.aggregator = std::bind (&CallbackClient::aggregator, this, std::placeholders::_1);

        provider.callbacks.subscribe (this, callbacks);
    }

    ~CallbackClient() {
        provider.callbacks.unsubscribe (this);
    }

    void check_results () {
        
    }
};


// A client with some of the callback slots empty.  Actually, all of them.
struct EmptyClient {
    EmptyClient() {
        CallbackProvider::Callbacks callbacks;
        provider.callbacks.subscribe (this, callbacks);
    }

    ~EmptyClient() {
        provider.callbacks.unsubscribe (this);
    }
};



int main (int, char **, char **) {
    // Check provider works.
    {
        provider.run_object_test(0);
    }

    // Check object callbacks on their own.
    {
        CallbackClient client;
        provider.run_object_test(1);
        client.check_results();

    }

    // Check that empty callback slots are handled
    {
        EmptyClient empty;
        provider.run_object_test(0);
    }

    // Check that multiple callbacks work, along with mixed-in empty slots
    {
        CallbackClient client;
        CallbackClient client_two;
        EmptyClient empty;
        provider.run_object_test(2);
        client.check_results();
    }

    // Check the non-object callback data
    CallbackProvider::Callbacks callbacks;

    // Set up delegate to do the opposite of the parameter.
    callbacks.delegate = std::function <bool (bool)> ([] (bool param) {
        return !param;
    });

    int nonobject_callback_count = 0;
    callbacks.callback = std::function <void (int &)> ([&nonobject_callback_count] (int &count) {
        count++;
        nonobject_callback_count++;
    });

    // Set up callback to return the negative of the parameter.
    callbacks.aggregator = std::function <int (int)> ([] (int value) {
        return -value;
    });


    // Check the standalone non-object callbacks.
    provider.callbacks.subscribe (&callbacks, callbacks);
    provider.run_nonobject_test (false);
    assert (nonobject_callback_count == 1);

    // Check the non-object callbacks mixed w/ object callbacks.
    // This also tests the and/or logic in the aggregator.
    CallbackClient client;
    provider.run_nonobject_test (true);
    assert (nonobject_callback_count == 2);
    provider.callbacks.unsubscribe (&callbacks);
}

