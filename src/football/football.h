///
/// Football public C++ declarations.
/// Contains C++ classes which wrap standard C Football structures.
/// Methods exposed by the objects present a new API that is more
/// suitable for the C++ object-oriented approach.
/// @file       football.h - Football socket abstraction layer
/// @author     Perette Barella
/// @date       2014-10-27
/// @copyright  Copyright 2012–2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <stdarg.h>
#include <stdbool.h>
#include <sys/types.h>
#include "assert.h"

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <functional>

#include "fb_public.h"
#include "fb_service.h"

/// Football C++ connections, services and parsers.
namespace Football {
    /// Lists of commands
    using HelpList = std::vector<const char *>;

    /** Base class for services, events, and connections.
        Provides common mechanism for outputting to associated connection(s). */
    class Thingie : public std::basic_streambuf<char> {
    protected:
        virtual void *myThingie (void) const = 0;
    public:
        virtual ~Thingie();
        ssize_t print (const std::string &message);
        virtual ssize_t conditional_print (bool (*callback) (FB_CONNECTION *), const std::string &message) = 0;
        ssize_t conditional_broadcast (bool (*callback) (FB_CONNECTION *), const std::string &message);
        ssize_t printf (const char *format, ...);
        ssize_t vprintf (const char *format, va_list parameters);
        ssize_t bprintf (const char *format, ...);
        ssize_t bvprintf (const char *format, va_list parameters);
        ssize_t cbprintf (bool (*callback) (FB_CONNECTION *), const char *format, ...);
        ssize_t cbvprintf (bool (*callback) (FB_CONNECTION *), const char *format, va_list parameters);
        // Overrides to implement streambuf
        virtual std::streamsize xsputn(const char_type* data, std::streamsize count) override;
        virtual int_type overflow(int_type c) override;
    };

    /// Helper functions for sending output to a connection or service.
    inline Thingie &operator << (Thingie &there, const char *message) {
        there.printf ("%s", message);
        return there;
    };

    // Declare all these inbred bastards
    class Connection;
    class ServiceBase;


    /// C++ Connection wrapper
    class Connection: public Thingie {
        friend class Iterator;
        friend class ServiceBase;
        friend class Arena;
    private:
        FB_CONNECTION *connection = nullptr; ///< Pointer to underlying Football connection
        int argname_capacity = 0;
        static Connection *getFromOld (FB_CONNECTION *connection);
        virtual inline void *myThingie (void) const final { return connection; }
        int argvIndex (const char *itemname) const;
    protected:
        /** @internal
            Try to retrieve the C++ connection object from the old-style C connection.
            @param connection The Football standard C connection structure.
            @return The C++ connection object, or NULL if there is none. */
        static Connection *tryGetFromOld (FB_CONNECTION *connection) {
            return (Connection *) connection->relatedObject;
        }
    public:
        ServiceBase *service (void) const;
        virtual ~Connection() override;

        virtual ssize_t conditional_print (bool (*callback) (FB_CONNECTION *), const std::string &message) override;

        // Actions
        bool transfer (ServiceBase *service, bool invokeNewConnectionHandler = false);
        void close (void);
        void acceptInput (bool mode);

        // Stubs with default behavior, but probably will be overriden
        virtual void newConnection (const FB_EVENT *event);
        virtual void connectionClose (const FB_EVENT *event);
        virtual void inputReceived (const FB_EVENT *event);

    private:
        void setConnection (FB_CONNECTION *conn) { connection = conn; };

#ifdef FOOTBALL_TRANSITIONAL
        // Getters to get easy access to underlying C data structures.
        FB_CONNECTION *operator()(void) { return connection; }
#endif
    };


    /** C++ Service wrapper
        Manages the service's connections.
        */
    class ServiceBase: public Thingie {
        friend class Iterator;
        friend class Connection;
        friend class Arena;
    private:
    protected:
        FB_SERVICE *service; ///< Pointer to underlying Football service structure
    protected:
        virtual inline void *myThingie (void) const final { return service; }
        static ServiceBase *getFromOld (FB_SERVICE *service);
        // Constructor
        ServiceBase (FB_SERVICE_OPTIONS options, ServiceBase *parent = nullptr);
        virtual ~ServiceBase() override;
        // Factories
        virtual Connection *allocNewConnection () = 0;
    public:
        void close (void);
        Connection *newConnectionFromFile (const std::string &filename);
        Connection *newLoopbackConnection (int *loopback);
        virtual ssize_t conditional_print (bool (*callback) (FB_CONNECTION *), const std::string &message) override;

        // Stubs, expected to be overridden
        virtual void serviceShutdown (void);

#ifdef FOOTBALL_TRANSITIONAL
        // Getters to get easy access to underlying C data structures.
        FB_SERVICE *operator()(void) { return service; }
#endif
    };


    /** Football Service.
        The service is customized for use with a particular connection type.
        @tparam Connection The connection type (extended from Football::Connection)
        used for the service's connections.
        */
    template <class Connection> class Service : public ServiceBase {
    public:
        struct iterator : public std::iterator<std::forward_iterator_tag, Connection> {
            size_t position;
            const FB_SERVICE &service;
            /// Advance the iterator past any opening/closing connections.
            inline void advancePastBadness (void) {
                while (position < service.connection_count && !service.connections [position]->relatedObject) {
                    assert (service.connections [position]->state != FB_SOCKET_STATE_OPEN);
                    position++;
                }
            }
            iterator (const FB_SERVICE &svc) : service (svc) {
                position = 0;
                advancePastBadness();
            };
            iterator (const FB_SERVICE &svc, size_t pos) : service (svc) {
                assert (pos == svc.connection_count);
                position = pos;
                advancePastBadness();
            };
            Connection *operator *() {
                return (Connection *)(service.connections [position]->relatedObject);
            }
            iterator &operator++() {
                position++;
                advancePastBadness();
                return *this;
            }
            iterator operator++(int) {
                iterator item = *this;
                position++;
                advancePastBadness();
                return item;
            }
            bool operator!=(iterator &compare) {
                return position != compare.position;
            }
        };
        struct const_iterator : public iterator {
            using iterator::iterator;
            inline const Connection *operator *() { return iterator::operator *(); };
        };
        Service (const FB_SERVICE_OPTIONS options,
                 Service *parent = nullptr) : ServiceBase (options, parent) {
        };
        Connection *newConnectionFromFile (const std::string &filename) {
            return (Connection *) ServiceBase::newConnectionFromFile (filename);
        };
        iterator begin() { iterator it (*service); return it ; };
        iterator end() { iterator it (*service, service->connection_count); return it ; };
        const_iterator begin() const { const_iterator it (*service); return it ; };
        const_iterator end() const { const_iterator it (*service, service->connection_count); return it ; };
    protected:
        /// Factories for related types instead of generics.
        virtual Connection *allocNewConnection () {
            return new (std::nothrow) Connection ();
        };
    };
    
    
    /** Football Arena: Manager for services.
        The arena runs all the services, providing event management and dispatching.
        Services are automatically added to the arena when created; use the Arena's
        poll routines to service them and their connections.  Events are dispatched
        to the virtual methods provided in Football::Connection, with support from
        Football::Interpreter; the poll return value indicates if service was done
        (or false if a timeout happened).
        */
    class Arena {
        friend ServiceBase;
    public:
        static bool ready (void);
        
        static bool poll (void);
        static bool pollWait (void);
        static bool pollUntil (time_t untilwhen);
        static bool pollWithTimeout (double timeout);
    private:
        static bool handleEvent (const FB_EVENT *);
    };
    
    
} // </namespace>

