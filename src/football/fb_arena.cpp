///
/// Football service container and dispatcher.
/// Implementations for Football::Arena methods.
/// This class is mostly a namespace; it isn't used as an object.
/// @file       fb_arena.cpp - Football socket abstraction layer
/// @author     Perette Barella
/// @date       2014-10-27
/// @copyright  Copyright 2014-2020 Devious Fish. All rights reserved.
///

#include <config.h>

#include <stdio.h>
#include <assert.h>

#include <exception>
#include <stdexcept>
#include <map>

#include "fb_public.h"
#include "football.h"

namespace Football {

    /** Determine if the Arena (service manager) is ready.
        @return true if ready, false otherwise. */
    bool Arena::ready (void) {
        return fb_services_are_open();
    }

    /** Process a Football event by dispatching a call to the connection's.
        @param event The standard C Football event.
        @return true if an event was polled, false if a timeout happened.
     */
    bool Arena::handleEvent (const FB_EVENT *event) {
        assert (event);
        if (!event) return false;

        // Timeouts don't have a service or connection.
        if (!event->service) {
            if (event->type == FB_EVENT_STOPPED) {
                fb_log (FB_WHERE (FB_LOG_WARNING), "Last service has shutdown.");
                return true;
            }
            assert (event->type == FB_EVENT_TIMEOUT ||
                    event->type == FB_EVENT_INTERRUPT);
            return false;
        }

        // Locate the related service and connection.
        ServiceBase *service = ServiceBase::getFromOld (event->service);
        // Service shutdown events don't have connections.
        if (!event->connection) {
            assert (event->type == FB_EVENT_STOPPED);
            service->serviceShutdown ();
            delete service;
            return true;
        }
        // If there isn't already a C++ connection object, it is created.
        Connection *conn = Connection::getFromOld(event->connection);
        if (!conn) {
            // Something lower failed, such as failed new connection allocation.
            return true;
        }
        switch (event->type) {
            case FB_EVENT_CONNECT:
                conn->newConnection (event);
                return true;
            case FB_EVENT_INPUT:
                conn->inputReceived (event);
                return true;
            case FB_EVENT_CLOSE:
                conn->connectionClose (event);
                conn->connection->relatedObject = nullptr;
                delete conn;
                return true;
            case FB_EVENT_FAULTING:
            case FB_EVENT_READABLE:
            case FB_EVENT_WRITABLE:
                // User thingies 
                assert (!"User thingies unimplemented.");
                return true;
            case FB_EVENT_ITERATOR:
            case FB_EVENT_ITERATOR_CLOSE:
                // These shouldn't be returned from Football's poll routine 
                assert (!"fb_poll returned iterator event");
                return true;
            case FB_EVENT_STOPPED:
            case FB_EVENT_INTERRUPT:
            case FB_EVENT_TIMEOUT:
                // This should have been handled above. 
                assert (!"Event type should have been handled earlier.");
                return true;

        }
        assert (!"Switch/case unmatched");
    }


    /** Poll services.  Return immediately if there is no activity.
     */
    bool Arena::poll (void) {
        return handleEvent (fb_poll ());

    }
    /** Poll services.  Wait indefinitely for activity if necessary.
        @return true*/
    bool Arena::pollWait (void) {
        return handleEvent (fb_wait ());
    }
    /** Poll services.  If idle, wait until the specified time for activity.
        @param until_when When to give up waiting.
        @return true if input was processed, false if timeout.
     */
    bool Arena::pollUntil (time_t until_when) {
        return handleEvent (fb_poll_until (until_when));
    }
    /** pollWithTimeout.
        @param duration The duration to wait for activity before timeout.
        @return true if input was processed, false if timeout.
     */
    bool Arena::pollWithTimeout (double duration) {
        return handleEvent (fb_poll_with_timeout (duration));
    }

}  // </namespace>
