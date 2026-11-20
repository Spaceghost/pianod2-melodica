///
/// Football C++ service wrapper.
/// Implementations for Football::ServiceBase methods.
/// @file       fb_servicepp.cpp - Football socket abstraction layer
/// @author     Perette Barella
/// @date       2014-11-20
/// @copyright  Copyright 2014-2021 Devious Fish. All rights reserved.
///

#include <config.h>

#include <stdio.h>
#include <assert.h>

#include <iostream>
#include <exception>
#include <stdexcept>
#include <iterator>
#include <map>

#include "football.h"
#include "fb_service.h"

namespace Football {

    // Services
    /** Create a new service with requested options.  Creates the underlying
        standard C stuff, and a C++ object that wraps it.
        @param options The service options such as ports, behavior settings, etc.
        @param parent A parent service whose service port(s) the child will share.
        @see fb_create_service () */
    ServiceBase::ServiceBase (FB_SERVICE_OPTIONS options,
                              ServiceBase *parent) {
        assert (options.context_size == 0);
        if (parent) options.parent = parent->service;
        service = fb_create_service (&options);
        if (service) {
            service->relatedObject = this;
            return;
        }
        throw std::invalid_argument("check logs for further information");
    }

    ServiceBase::~ServiceBase () {        
    }
    

    /// Initiate shutdown of a service.
    void ServiceBase::close (void) {
        fb_close_service (service);
    }

    /** Create a new "connection" that reads from a file.
        Replies to the connection are transparently disposed.
        An appropriate event is dispatched to the connection when created,
        but nothing is read from the file yet.  The caller may adjust the
        context or privileges; reading is done by the Arena (service
        manager) poll routines.
        @param filename The name of the file to read from.
        @return An initialized Football::Connection, or NULL on error.
        */
    Connection *ServiceBase::newConnectionFromFile (const std::string &filename) {
        FB_EVENT *event = fb_accept_file (service, (char *) filename.c_str());
        if (!event) return nullptr;
        Arena::handleEvent (event);
        return Connection::getFromOld (event->connection);
    }


    /** Create a new connection that loops back to the service.
        Bidirectional communication is supported.
        Replies to the connection are transparently disposed.
        An appropriate event is dispatched to the connection when created,
        but nothing is read from the file yet.  The caller may adjust the
        context or privileges; reading is done by the Arena (service
        manager) poll routines.
        @param loopback On return, set to the socket used for loopback.
        @return A socket number, or -1 on error.
        */
    Connection *ServiceBase::newLoopbackConnection (int *loopback) {
        FB_EVENT *event = fb_loopback_socket (service);
        if (!event) return nullptr;
        Arena::handleEvent (event);
        *loopback = event->socket;
        return Connection::getFromOld (event->connection);
    }


    /** @internal
        Retrieve a C++ service object from the old C-style service structure.
        @param service The old standard C service structure.
        @return The C++ service object. */
    ServiceBase *ServiceBase::getFromOld(FB_SERVICE *service) {
        assert (service);
        assert (service->relatedObject);
        return (ServiceBase *) service->relatedObject;
    }


    /** Invoked when a service is shutting down.  Corresponds to
        FB_EVENT_STOPPED; by the time this is received, all
        connections have been closed. */
    void ServiceBase::serviceShutdown (void) {
        // Nothing to do by default
    }

    ssize_t ServiceBase::conditional_print (bool (*callback) (FB_CONNECTION *), const std::string &message) {
        return fb_cbfprintf (myThingie(), callback, "%s", message.c_str());
    }

} // </namespace>

