///
/// Football C++ connection and event wrappers and extensions.
/// Implementations for Football::Thingie and Football::Connection methods.
/// @file       fb_connection.cpp - Football socket abstraction layer
/// @author     Perette Barella
/// @date       2014-10-26
/// @copyright  Copyright 2014-2021 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cstdio>
#include <cstdarg>
#include <string>
#include <cassert>
#include <fcntl.h>

#include <typeinfo>
#include <iostream>
#include <exception>
#include <stdexcept>
#include <iterator>
#include <vector>
#include <map>
#include <future> // for exception types
#include <regex> // for exception types

#include "football.h"
#include "fb_service.h"

namespace Football {
    Thingie::~Thingie() {
        // Virtually needed.
    }

    /** Add a string to the output queues. */
    ssize_t Thingie::print (const std::string &message) {
        return fb_fprint (myThingie(), message.c_str(), message.size());
    }

    /** Send a message to some output queues depending on the callback function return value. */
    ssize_t Thingie::conditional_broadcast (bool (*callback) (FB_CONNECTION *), const std::string &message) {
        return fb_cbfprintf (myThingie(), callback, "%s", message.c_str());
    }


    /** Add messages to output queues.
        - 'b' variants broadcast.
        - 'cb' variants do conditional broadcast based on return value of a provided callback.
        - 'v' accepts a pointer to format arguments.
        @param format a printf-style format string
        @return -1 on error, or number of bytes written/queued on success or partial success.
        */
    ssize_t Thingie::printf (const char *format, ...) {
        va_list parameters;
        va_start(parameters, format);
        ssize_t result = vprintf (format, parameters);
        va_end (parameters);
        return result;
    }

    /// @see Football::Thingie::printf
    ssize_t Thingie::vprintf (const char *format, va_list parameters) {
        return fb_vfprintf (myThingie(), format, parameters);
    }

    /// @see Football::Thingie::printf
    ssize_t Thingie::bprintf (const char *format, ...) {
        va_list parameters;
        va_start(parameters, format);
        ssize_t result = bvprintf (format, parameters);
        va_end (parameters);
        return result;
    }

    /// @see Football::Thingie::printf
    ssize_t Thingie::bvprintf (const char *format, va_list parameters) {
        return fb_bvfprintf (myThingie(), format, parameters);

    }

    /// @see Football::Thingie::printf
    ssize_t Thingie::cbprintf (bool (*callback) (FB_CONNECTION *), const char *format, ...) {
        va_list parameters;
        va_start(parameters, format);
        ssize_t result = cbvprintf (callback, format, parameters);
        va_end (parameters);
        return result;
    }

    /// @see Football::Thingie::printf
    ssize_t Thingie::cbvprintf (bool (*callback) (FB_CONNECTION *), const char *format, va_list parameters) {
        return fb_cbvfprintf (myThingie(), callback, format, parameters);
    }


    std::streamsize Thingie::xsputn(const Thingie::char_type* data, std::streamsize count) {
        return fb_fprint (myThingie(), data, count);
    };
    
    Thingie::int_type Thingie::overflow(Thingie::int_type c) {
        Thingie::char_type ch = c;
        return (fb_fprint (myThingie(), &ch, sizeof (ch))) < 0 ? EOF : c;
    }




    // Connections

    Connection::~Connection() {
    };


    /** @internal
        Find the C++ connection object from the old-style C connection.
        If there isn't one yet, create it one.
        @param connection The Football standard C connection structure.
        @return The C++ connection object. */
    Connection *Connection::getFromOld (FB_CONNECTION *connection) {
        assert (connection);
        assert (connection->service->relatedObject);
        if (!connection->relatedObject) {
            ServiceBase *svc = ServiceBase::getFromOld(connection->service);
            Connection *conn = svc->allocNewConnection();
            if (conn) {
                connection->relatedObject = conn;
                conn->connection = connection;
            } else {
                fb_close_connection (connection);
            }
        }
        return (Connection *) connection->relatedObject;
    }



    /*
     *              Actions
     */

    /** Send a message to some output queues depending on the callback function return value.
        @param callback A callback function accepting `FB_CONNECTION *` that returns true to
        broadcast to, false to skip.
        @param message The message to put on the output queue. */
    ssize_t Connection::conditional_print (bool (*callback) (FB_CONNECTION *), const std::string &message) {
        if (callback (connection)) {
            return print (message);
        }
        return message.size();
    }


    /// Initiate connection closure.
    void Connection::close (void) {
        fb_close_connection(connection);
    }

    /** Transfer a connection to another service.
        @param newservice The service to transfer to.
        @param invokeNewConnectionHandler If true, the `newConnection` handler is
        invoked with a nullptr. */
    bool Connection::transfer (ServiceBase *newservice, bool invokeNewConnectionHandler) {
        if (fb_transfer(connection, newservice->service)) {
            if (invokeNewConnectionHandler) {
                newConnection (nullptr);
            }
            return true;
        }
        return false;
    }

    /** Get a connection's parent service.
        @return The C++ service object for the connection.  */
    ServiceBase *Connection::service () const {
        assert (this);
        assert (this->connection);
        return ServiceBase::getFromOld (connection->service);
    }

    /** Control acceptance of input.
        @param mode If true, enables input.  If false, the connection is
        ignored; received input will be queued and received when input
        is again enabled. */
    void Connection::acceptInput (bool mode) {
        fb_accept_input(connection, mode);
    }

    /*
     *              Connection event handlers
     */


    /** Invoked when a new connection has arrived/greeted.  Corresponds
        to FB_EVENT_CONNECT.
        @param event The new connection details, or a nullptr for
        transferred connections. */
    void Connection::newConnection (const FB_EVENT *event) {
        *this << "[Football::Connection] Connected\n";
    }


    /** Invoked when a connection is closing.  Corresponds to
        FB_EVENT_CLOSE event.  This is the last notification/event
        for a connection before it closes.
        @param event Closing connection details. */
    void Connection::connectionClose (const FB_EVENT *event) {
        *this << "[Football::Connection] Connection closing\n";
    }

    /** Invoked when a completed message is received.  Corresponds
        to FB_EVENT_INPUT event.
        @param event Details of the message received. */
    void Connection::inputReceived (const FB_EVENT *event) {
        *this << "[Football::Connection] Input received: " << event->command << "\n";
    }

} // </namespace>
