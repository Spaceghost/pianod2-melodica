///
/// Football service and connection creation/destruction.
/// @file       fb_service.c - Football socket abstraction layer
/// @author     Perette Barella
/// @date       2012-03-03
/// @copyright  Copyright 2012-2021 Devious Fish. All rights reserved.
///

#include <config.h>

#ifndef __FreeBSD__
#define _BSD_SOURCE /* strdup, realpath */
#define _DEFAULT_SOURCE
#endif

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include "fb_transport.h"
#include "fb_service.h"

#ifdef WITH_ACCESSCONTROL
#ifndef __STDC__
// Eliminate warning about implicit declaration on older OS X (Snow Leopard)
#define __STDC__
#endif
#include <tcpd.h>
#endif

static int fb_open_service_count = 0;
#ifdef FOOTBALL_THREADS
/** Sync registering/unregistering services. */
static pthread_mutex_t service_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/** Indicate whether there are any services open.
    @return true if services exist, false otherwise. */
bool fb_services_are_open (void) {
    return fb_open_service_count > 0;
}

/** @internal Set sane options on a socket after opening.
    @param socket The socket to sanitize.
    @return True on success, false on error. */
static bool fb_sanitize_socket (int socket) {
    /* put the socket in non-blocking mode */
    if (fcntl (socket, F_SETFL, fcntl (socket, F_GETFL) | O_NONBLOCK) == -1) {
        fb_perror ("fcntl");
        return false;
    }
    /* Disable the dreaded killer sigpipe */
#ifdef HAVE_SO_NOSIGPIPE
    int option_value = 1; /* We're setting NOSIGPIPE to ON */
    if (setsockopt (socket, SOL_SOCKET, SO_NOSIGPIPE, &option_value, sizeof (option_value)) < 0) {
        fb_log (FB_WHERE (FB_LOG_ERROR), "setsockopt(%d,,SO_NOSIGPIPE...): %s", socket, strerror (errno));
        return false;
    }
#endif
    return true;
}



/** @internal
    Initialize and set up a listening socket.
    @return true on success, false on failure */
static bool fb_setup_socket (FB_SERVICE *service, FB_SOCKETID which) {
#ifndef FB_HAVE_TLS
    /* That's #if_n_def: skip socket creation for encrypted socket when not available. */
    if (fb_encrypted_socket(which)) {
        fb_log (FB_WHERE (FB_LOG_WARNING), "TLS support not available.  Install/update TLS package and rebuild, or review config.log for missing prerequisites.");
        return false;
    }
#endif
    bool ipv6 = fb_ip6_socket (which);
    in_port_t port = fb_http_socket (which) ?
                    (fb_encrypted_socket (which) ? service->options.https_port : service->options.http_port) :
                    service->options.line_port;
    if (!port) {
        return false;
    }
#ifdef HAVE_IPV6
    if ((service->socket [which] = socket (ipv6 ? PF_INET6 : PF_INET, SOCK_STREAM, 0)) >= 0) {
#else
    if (ipv6) {
        fb_log (FB_WHERE (FB_LOG_WARNING), "IPV6 support not available.  Check HAVE_IPV6 in config.log if it should be.");
        return false;
    }
    if ((service->socket [which] = socket (PF_INET, SOCK_STREAM, 0)) >= 0) {
#endif
        int on = 1;
        /* We got a socket, bind and listen on it. */
        if (ipv6) {
#ifdef HAVE_IPV6
            service->address [which].ip6.sin6_family = PF_INET6;
            service->address [which].ip6.sin6_addr = in6addr_any;
            service->address [which].ip6.sin6_port = htons (port);
#else
            assert (!"Reached without IPv6 support");
#endif
        } else {
            service->address [which].ip4.sin_family = PF_INET;
            service->address [which].ip4.sin_addr.s_addr = INADDR_ANY;
            service->address [which].ip4.sin_port = htons (port);
        }
        if (setsockopt(service->socket [which], SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
        {
            fb_perror ("setsockopt(SO_REUSEADDR)");
            /* That's annoying but not critical, so keep going. */
        }
#ifdef HAVE_IPV6
        if (bind (service->socket [which],
                  (struct sockaddr*) &(service->address [which]),
                  ipv6 ? sizeof (service->address [which].ip6) : sizeof (service->address [which].ip4)) >= 0) {
#else
        if (bind (service->socket [which],
                    (struct sockaddr*) &(service->address [which]), sizeof (service->address [which].ip4)) >= 0) {
#endif
            if (listen (service->socket [which], service->options.queue_size) >= 0) {
                if (fb_register (service->socket [which], FB_SOCKTYPE_SERVICE, service)) {
                    return true;
                }
                /* Errors already announced by fb_register */
            }
        } else {
            fb_log (FB_WHERE (FB_LOG_ERROR), "bind: %s (%s port %d)",
                    strerror (errno), fb_ip6_socket (which) ? "IP6" : "IP4", (int) port);
        }
        close (service->socket [which]);
    } else {
        fb_perror ("socket");
    };
    service->socket [which] = 0;
    return false;
}


/** Create a new service and initialize its listeners.
    Subsequent changes made to the options do not effect behavior.
    @param options Options for the new service's behavior.
    @return a pointer to the service, or NULL on failure. */
FB_SERVICE *fb_create_service (const FB_SERVICE_OPTIONS *options) {
    assert (options->line_port || options->http_port ||
            options->https_port || options->transfer_only);
    if (options->line_port == 0 && options->http_port == 0 &&
        options->https_port == 0 && !options->transfer_only) {
        fb_log (FB_WHERE (FB_LOG_ERROR), "No ports specified in service '%s' options",
            options->name ? options->name : "unnamed");
        return NULL;
    }

    /* Either we're a root, or the parent's the root.  No multilevel. */
    if (options->parent) {
        assert (!options->parent->options.parent);
        assert (strcasecmp (options->name, options->parent->options.name) != 0);
    }

    /* Allocate and initialize memory for the service */
    FB_SERVICE *service;
    if ((service = calloc (1, sizeof (*service)))) {
        fb_mutex_lock (&service_mutex);
        service->options = *options;
        service->options.name = options->name ? strdup (options->name) : NULL;
        service->options.greeting = strdup (options->greeting ? options->greeting
                                            : options->parent ? options->parent->options.greeting
                                            : "HELO");
        const char *servedir = ((options->serve_directory || !options->parent)
                                ? options->serve_directory
                                : options->parent->options.serve_directory);
        if (servedir) {
#ifdef HAVE_CANONICALIZE_FILE_NAME
            service->options.serve_directory = canonicalize_file_name(servedir);
#elif defined (HAVE_REALPATH)
            service->options.serve_directory = realpath (servedir, NULL);
#else
            service->options.serve_directory = strdup (servedir);
#endif
            if (!service->options.serve_directory) {
                fb_log (FB_WHERE (FB_LOG_ERROR), "%s: %s",servedir, strerror (errno));
            }
        }
        const char *langdir = ((options->locale_directory || !options->parent)
                                ? options->locale_directory
                                : options->parent->options.locale_directory);
        service->options.locale_directory = langdir ? strdup (langdir) : NULL;
        if ((service->options.name || !options->name) &&
            (service->options.serve_directory || !servedir) &&
            (service->options.locale_directory || !langdir) &&
            service->options.greeting) {
            service->type = FB_SOCKTYPE_SERVICE;
            fb_mutex_init (&service->mutex);

            /* Initialize and set up the sockets */
            fb_mutex_lock (&service->mutex);
            int successes = 0;
            for (int i = 0; i < FB_SOCKET_COUNT; i++) {
                successes += fb_setup_socket (service, i);
            }

            if (successes > 0 || options->transfer_only) {
                service->state = FB_SOCKET_STATE_OPEN;
                fb_open_service_count++;
                if (options->parent) {
                    service->next_child = options->parent->next_child;
                    options->parent->next_child = service;
                    /* If we're using parents, everybody needs a name. */
                    assert (options->name);
                    assert (options->parent->options.name);
                }
                fb_mutex_unlock (&service->mutex);
                fb_mutex_unlock (&service_mutex);
                return (service);
            }
            fb_mutex_unlock (&service->mutex);
            fb_mutex_destroy (&service->mutex);
            /* Errors already announced by fb_setup_socket */
        }
        free (service->options.name);
        free (service->options.greeting);
        free (service->options.serve_directory);
        free (service->options.locale_directory);
        free (service);
        fb_mutex_unlock (&service_mutex);
    } else {
        fb_perror ("calloc");
    }
    return NULL;
}


/** @internal
    Destroy a service's resources.
    Abruptly closes any remaining connections, frees them,
    closes listener sockets and frees service.
    @param service the service to terminate.
    @return Nothing. */
void fb_destroy_service (FB_SERVICE *service) {
    fb_mutex_lock (&service_mutex);
    /* Close all the connections first */
    assert (service);
    assert (service->connection_count == 0);
    while (service->connection_count > 0) {
        fb_destroy_connection (service->connections [0]);
    }
    /* Close our listeners */
    for (FB_SOCKETID id = 0; id < FB_SOCKET_COUNT; id++) {
        if (service->socket [id]) {
            fb_unregister (service->socket [id]);
            close (service->socket [id]);
        }
    }
    if (service->options.parent) {
        /* Remove dead child from linked list. */
        FB_SERVICE *node = service->options.parent;
        while (node->next_child != service) {
            node = node->next_child;
            assert (node);
        }
        node->next_child = node->next_child->next_child;
    } else {
        /* This might be a parent.  Orphan all children. */
        FB_SERVICE *child = service->next_child;
        while (child) {
            FB_SERVICE *next = child->next_child;
            child->options.parent = NULL;
            child->next_child = NULL;
            child = next;
        }
    }
    free (service->options.greeting);
    free (service->options.serve_directory);
    free (service->options.locale_directory);
    free (service->options.name);
    free (service->connections); /* Per man page, freeing null is okay */
    free (service);
    fb_open_service_count--;
    fb_mutex_unlock (&service_mutex);
}


/** Initiate closure of a service.
    @return Nothing. */
void fb_close_service (FB_SERVICE *service) {
    fb_mutex_lock (&service->mutex);
    assert (service);
    assert (service->state == FB_SOCKET_STATE_OPEN);
    service->state = FB_SOCKET_STATE_CLOSING;
    for (unsigned i = 0; i < service->connection_count; i++) {
        fb_close_connection (service->connections[i]);
    }
    /* Stop listening so we don't accept new connections */
    for (FB_SOCKETID id = 0; id < FB_SOCKET_COUNT; id++) {
        if (service->socket [id]) {
            fb_set_readable (service->socket [id], false);
        }
    }
    fb_mutex_unlock (&service->mutex);
    if (service->connection_count == 0) {
        /* If there's no connections left, schedule for reaping. */
        fb_schedule_reap (service);
    }
}


/** Validate connection with hosts_access.
    @param service The service on which the connection is arriving.
    @param connection The connection.
    @return True if access is allowed, false if denied. */
bool fb_validate_connection (FB_SERVICE *service, FB_CONNECTION *connection) {
    assert (connection);
    char buffer [200];
    // Mac issue: deny occasional connections from 0.0.0.0
    bool allowed = (connection->origin.ip4addr.sin_addr.s_addr != 0);
#ifdef HAVE_IPV6
    allowed = allowed || (connection->domain == PF_INET6);
    const char *address = inet_ntop (connection->domain,
                                     connection->domain == PF_INET6 ?
                                     (struct in_addr *) &(connection->origin.ip6addr.sin6_addr) :
                                     &(connection->origin.ip4addr.sin_addr),
                                     buffer, sizeof (buffer));
#else
    const char *address = inet_ntop (connection->domain,
                                     &(connection->origin.ip4addr.sin_addr),
                                     buffer, sizeof (buffer));
#endif
#ifdef WITH_ACCESSCONTROL
    allowed = allowed && hosts_ctl (service->options.name ? service->options.name : STRING_UNKNOWN,
                                    STRING_UNKNOWN, // Client name
                                    (char *) (address ? address : STRING_UNKNOWN), // Client address
                                    STRING_UNKNOWN); // User
#endif
    fb_log (FB_WHERE (FB_LOG_CONN_STATUS), "#%d: Connection from %s %s.", 
            connection->socket, address, allowed ? "permitted" : "denied");

    return allowed;
}

/** @internal
    Prepare a new connection.
    Make space in the service's connection array, allocate
    a connection and (if requested in service options) a user context.
    Any mutexing is expected to be handled by the caller.
    @param service the service for the new connection.
    @return a fully-allocated and zero-initialized FB_CONNECTION. */
static FB_CONNECTION *fb_new_connection (FB_SERVICE *service) {
    /* Expand the connection list for this service if needed. */
    assert (service);
    assert (service->connection_count <= service->connections_size); /* If this is off, something already went wrong */
    if (!fb_expandcalloc ((void **) &service->connections, &service->connections_size,
                          service->connection_count + 1, sizeof (FB_CONNECTION *))) {
        fb_perror ("fb_expandcalloc");
        return NULL;
    }
    /* Allocate the connection, initialize it, and add a context if necessary */
    FB_CONNECTION *connection = calloc (1, sizeof (*connection));
    if (connection) {
        connection->type = FB_SOCKTYPE_CONNECTION;
        connection->service = service;
        fb_mutex_init (&connection->mutex);
        /* If a context isn't needed, we're good */
        if (service->options.context_size == 0) {
            return (connection);
        }
        connection->context = calloc (1, service->options.context_size);
        if (connection->context) {
            return (connection);
        }
        free (connection);
    }
    fb_perror ("calloc");
    return NULL;
}    


/** @internal
    Accept a connection.
    @param service The service on which a connection is arriving.
    @param id Identifies the arrival socket (line, HTTP, TLS, IP4 vs IP6)
    @return a pointer to a new connection, or NULL on failure. */
FB_CONNECTION *fb_accept_connection (FB_SERVICE *service, FB_SOCKETID id) {
    assert (service);
    fb_mutex_lock(&service->mutex);
    FB_CONNECTION *connection = fb_new_connection (service);
    if (!connection) {
        /* If there's not enough memory to store the connection, just reject it */
        fb_mutex_unlock (&service->mutex);
        return NULL;
    }
    /* Allocate and initialize memory for the service */
    connection->domain = fb_ip6_socket (id) ? PF_INET6 : PF_INET;
    connection->http = fb_http_socket (id);
    connection->encrypted = fb_encrypted_socket (id);
#ifdef FB_HAVE_TLS
    connection->transport = (connection->encrypted ?
                             &fb_transport_encrypted : &fb_transport_unencrypted);
#else
    connection->transport = &fb_transport_unencrypted;
#endif
    socklen_t addr_size = (socklen_t) sizeof (connection->origin);
    connection->state = connection->encrypted ? FB_SOCKET_STATE_TLS_HANDSHAKE :
                        connection->http || service->options.greeting_mode == FB_GREETING_REQUIRE ||
                                            service->options.greeting_mode == FB_GREETING_FALLBACK ?
                        FB_SOCKET_STATE_GREETING : FB_SOCKET_STATE_OPEN;
    if (service->options.greeting_timeout && (connection->state == FB_SOCKET_STATE_TLS_HANDSHAKE ||
                                               connection->state == FB_SOCKET_STATE_GREETING)) {
        connection->expiry_time = time(NULL) + service->options.greeting_timeout;
    } else {
        connection->expiry_time = FAR_FUTURE;
    }

    /* Accept the connection */
    if ((connection->socket = accept (service->socket [id],
                                      (struct sockaddr *) &(connection->origin), &addr_size)) >= 0) {
        /* Add to the connection list */
        if (fb_sanitize_socket(connection->socket) &&
            fb_validate_connection (service, connection) && connection->transport->init (connection)) {
            service->connections [service->connection_count++] = connection;
            fb_mutex_unlock (&service->mutex);
            return (connection);
        }
        close (connection->socket);
    } else {
        fb_perror ("accept");
    }
    fb_mutex_unlock (&service->mutex);
    fb_mutex_destroy (&connection->mutex);
    free (connection->context);
    free (connection);
    return NULL;
}


/** @internal
    Remove a connection from a service's connection list.
    @param service The service to remove from.
    @param connection The connection to remove.
 */
static void fb_remove_connection_from_service (FB_SERVICE *service, FB_CONNECTION *connection) {
    /* Take the connection out of the service's list */
    unsigned int i;
    for (i = 0; i < service->connection_count && service->connections [i] != connection; i++)
    /* nothing */;
    assert (i < service->connection_count); /* If we didn't find it, there's a bug */
    service->connection_count--;
    unsigned int j;
    for (j = i; j < service->connection_count; j++) {
        service->connections [j] = service->connections [j+1];
    }
}


/** Transfer a connection to a new service.
    @param connection The connection to transfer.
    @param service The service to reassign it to. */
bool fb_transfer (FB_CONNECTION *connection, FB_SERVICE *service) {
    if (service == connection->service) {
        return true;
    }
    
    fb_mutex_lock (&service->mutex);
    fb_mutex_lock (&connection->service->mutex);
    assert (connection);
    assert (service);

    bool status = false;
    
    /* Make sure there's room in the destination's connection array */
    if (!fb_expandcalloc ((void **) &service->connections, &service->connections_size,
                          service->connection_count + 1, sizeof (FB_CONNECTION *))) {
        fb_perror ("fb_expandcalloc");
    } else if (service->state != FB_SOCKET_STATE_OPEN) {
        fb_log (FB_WHERE (FB_LOG_WARNING), "Transfer denied: target service not open.");
    } else {
        /* Remove connection from the original service's array */
        fb_remove_connection_from_service (connection->service, connection);

        /* Add the connection to the destination service's array */
        service->connections [service->connection_count++] = connection;
        connection->service = service;
        status = true;
    }
    fb_mutex_unlock (&connection->service->mutex);
    fb_mutex_unlock (&service->mutex);
    return status;
}

/** @internal
    Transfer a connection to a related service.
    @param connection The connection to transfer.
    @param name The name of the service to reassign it to. */
bool fb_transfer_by_name (FB_CONNECTION *connection, const char *name) {
    assert (connection);
    assert (name);

    fb_mutex_lock (&service_mutex);
    FB_SERVICE *service = connection->service->options.parent;
    if (!service) {
        service = connection->service;
    }
    while (service) {
        if (strcasecmp (service->options.name, name) == 0) {
            return fb_transfer(connection, service);
        }
        service = service->next_child;
    }
    fb_mutex_unlock (&service_mutex);
    return false;
}

/** @internal
    Close and destroy a connection.
    @return Nothing. */
void fb_destroy_connection (FB_CONNECTION *connection) {
    assert (connection);

    fb_unregister(connection->socket);

    fb_mutex_lock (&connection->service->mutex);
    fb_remove_connection_from_service (connection->service, connection);
    fb_mutex_unlock (&connection->service->mutex);

    fb_mutex_lock (&connection->mutex); // Paranoia

    /* Release TLS resources */
    connection->transport->done (connection);

    /* Close the socket and free resources */
    close (connection->socket);
    free (connection->filename);
    free (connection->context);
    free (connection->in.message);
    fb_queue_destroy(&connection->assembly);
    fb_queue_destroy(&connection->out);
    fb_destroy_httprequest (&connection->request);
    fb_mutex_unlock (&connection->mutex);
    fb_mutex_destroy (&connection->mutex);
    fb_log (FB_WHERE (FB_LOG_CONN_STATUS), "#%d: Connection terminated.", connection->socket);
    free (connection);
}



/** Initiate connection closure.
    If the socket is in an open state, change its state and enable
    writes.  The next write attempt will generate a close event.
    If the connection is already on its way out, leave it alone.
    @param connection the connection to close */
void fb_close_connection (FB_CONNECTION *connection) {
    assert (connection);
    
    if (connection->state <= FB_SOCKET_STATE_OPEN) {
        connection->state = FB_SOCKET_STATE_FLUSHING;
    }
    /* Ignore further input: if service is closing, this avoids a connection
     requesting individual closure and causing a duplicate close. */
    fb_set_writable (connection->socket, true);
    fb_set_readable (connection->socket, false);
    fb_set_buffering (connection->socket, false);
}



/** Make a connection that reads from a file.
    @param service the service to register the connection with
    @param filename the file to read from
    @return a pointer to the connection, or NULL on failure. */
FB_EVENT *fb_accept_file (FB_SERVICE *service, char *filename) {
    assert (service);
    assert (filename && *filename);
    static FB_EVENT event;
    memset (&event, 0, sizeof (event));
    event.magic = FB_SOCKTYPE_EVENT;
    event.type = FB_EVENT_CONNECT;
    event.service = service;
    /* Allocate a new connection and fill it in; create an event for the new connection. */
    fb_mutex_lock(&service->mutex);
    event.connection = fb_new_connection (service);
    if (event.connection) {
        event.connection->state = FB_SOCKET_STATE_OPEN;
        event.connection->expiry_time = FAR_FUTURE;
        event.connection->transport = &fb_transport_read_file;
        if ((event.connection->filename = strdup (filename))) {
            /* All the allocations were successful, so open the file. */
            if ((event.connection->socket = open (filename, O_RDONLY)) >= 0) {
                event.context = event.connection->context;
                event.socket = event.connection->socket;
                if (fb_register (event.connection->socket, FB_SOCKTYPE_CONNECTION, event.connection)) {
                    /* Add to the connection list */
                    service->connections [service->connection_count++] = event.connection;
                    event.connection->transport->init (event.connection);
                    fb_mutex_unlock(&service->mutex);
                    fb_log (FB_WHERE (FB_LOG_CONN_STATUS), "#%d: New file connection for %s",
                            event.connection->socket, filename);
                    return &event;
                }
                close (event.connection->socket);
            } else {
                fb_log (FB_WHERE (FB_LOG_ERROR), "%s: %s", filename, strerror (errno));
            }
            free (event.connection->filename);
        } else {
            fb_perror ("strdup");
        }
        fb_mutex_destroy (&event.connection->mutex);
        free (event.connection->context);
        free (event.connection);
    }
    fb_mutex_unlock(&service->mutex);
    return NULL;
}


/** Make a connection with a pipe.
    @param service the service to register the connection with
    @return a pointer to the connection, or NULL on failure. */
FB_EVENT *fb_loopback_socket (FB_SERVICE *service) {
    assert (service);
    static FB_EVENT event;
    memset (&event, 0, sizeof (event));
    event.magic = FB_SOCKTYPE_EVENT;
    event.type = FB_EVENT_CONNECT;
    event.service = service;
    /* Allocate a new connection and fill it in; create an event for the new connection. */
    fb_mutex_lock(&service->mutex);
    event.connection = fb_new_connection (service);
    if (event.connection) {
        event.connection->state = FB_SOCKET_STATE_OPEN;
        event.connection->expiry_time = FAR_FUTURE;
        event.connection->transport = &fb_transport_unencrypted;
        int sockets [2];
        if ((socketpair (PF_LOCAL, SOCK_STREAM, 0, sockets)) == 0) {
            event.context = event.connection->context;
            event.connection->socket = sockets [1];
            event.socket = sockets [0];
            if (fb_sanitize_socket (event.connection->socket) && fb_sanitize_socket (event.socket)
                && fb_register (event.connection->socket, FB_SOCKTYPE_CONNECTION, event.connection)) {
                /* Add to the connection list */
                service->connections [service->connection_count++] = event.connection;
                fb_mutex_unlock(&service->mutex);
                fb_log (FB_WHERE (FB_LOG_CONN_STATUS), "#%d: New loopback connection",
                        event.connection->socket);
                return &event;
            }
            close (event.connection->socket);
            close (event.socket);
        } else {
            fb_perror ("socketpair");
        }
        fb_mutex_destroy (&event.connection->mutex);
        free (event.connection->context);
        free (event.connection);
    }
    fb_mutex_unlock(&service->mutex);
    return NULL;
}



/** Create a new iterator.
    Create and initialize a new iterator for a service.
    @see fb_destroy_iterator().
    @return an iterator or NULL on error. */
FB_ITERATOR *fb_new_iterator (FB_SERVICE *service) {
    assert (service);
    FB_ITERATOR *it = calloc (sizeof (FB_ITERATOR), 1);
    if (it) {
        it->service = service;
        it->iteration = service->connection_count;
        fb_mutex_lock(&service->mutex);
    } else {
        fb_perror ("calloc");
    }
    return (it);
}


/** Get the next iteration.
    @see FB_EVENTTYPE
    @param it The iterator
    @return An event, or NULL if there are no more connections. */
FB_EVENT *fb_iterate_next (FB_ITERATOR *it) {
    static FB_EVENT event;
    assert (it);
    assert (it->service);
    assert (it->iteration >= 0);
    if (it->iteration > 0) {
        it->iteration--;
        if (it->service->connections [it->iteration]->state < FB_SOCKET_STATE_OPEN) {
            return fb_iterate_next (it);
        }
        memset (&event, 0, sizeof (event));
        event.magic = FB_SOCKTYPE_EVENT;
        event.service = it->service;
        event.connection = it->service->connections [it->iteration];
        event.type = event.connection->state == FB_SOCKET_STATE_OPEN ? FB_EVENT_ITERATOR : FB_EVENT_ITERATOR_CLOSE;
        event.socket = event.connection->socket;
        event.context = event.connection->context;        
        return (&event);
    }
    return NULL;
}


/** Destroy an iterator and release its resources.
    @param it The iterator */
void fb_destroy_iterator (FB_ITERATOR *it) {
    assert (it);
    fb_mutex_unlock(&it->service->mutex);
    free (it);
}
