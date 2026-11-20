///
/// Football I/O event handlers.
/// @file       fb_event.c - Football socket abstraction layer
/// @author     Perette Barella
/// @date       2012-03-03
/// @copyright  Copyright 2012-2021 Devious Fish. All rights reserved.
///

#include <config.h>

#ifndef __FreeBSD__
#define _BSD_SOURCE /* snprintf() */
#define _DEFAULT_SOURCE
#endif
#if !defined(__FreeBSD__) && !defined(__APPLE__)
#define _POSIX_C_SOURCE 200112L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <stdbool.h>

#include <assert.h>

#include "fb_service.h"


/** @internal
    Create and populate a new message block.
    @param format a printf-style format string
    @param parameters parameters to the format string */
static FB_MESSAGE *fb_create_message (const char *format, va_list parameters) {
    FB_MESSAGE *message;
    if ((message = fb_messagealloc ())) {
        message->length = vasprintf(&message->message, format, parameters);
        if (message->length >= 0) {
            return message;
        }
        fb_perror ("vasprintf");
        fb_messagefree(message);
    }
    return NULL;
}


/** @internal
    Put a message into a connection's output queue.
    @param connection the connection to which the message is to be sent.
    @param message a message to send.
    @return length of message queued on success, or -1 on failure. */
static ssize_t fb_queue_single (FB_CONNECTION *connection, FB_MESSAGE *message) {
    fb_mutex_lock(&connection->mutex);
    ssize_t length = message->length;
    if (length == 0 || connection->state == FB_SOCKET_STATE_CLOSING ||
        connection->state < FB_SOCKET_STATE_OPEN) {
        /* Don't queue empty messages or if the connection isn't fully open
           or is closing, but feign success in all these cases. */
        fb_messagefree (message);
        fb_mutex_unlock(&connection->mutex);
        return length;
    }
    /** Add the message to the queue */
    if (connection->http) {
        if (fb_queue_add (&connection->assembly, message)) {
            fb_websocket_encode (connection);
        } else {
            length = -1;
        }
    } else {
        if (!fb_queue_add (&connection->out, message)) {
            length = -1;
        }
    }
    fb_mutex_unlock(&connection->mutex);
    /* Try to send now, which enables socket writes too. */
    fb_send_output (NULL, connection);
    return length;
}

/** @internal
    Put a message into all a service's queues.
    @param service the service to which the message will be sent.
    @param message the message to send.
    @param callback If NULL, broadcast to all connections.
    Otherwise, broadcast only if the specified callback function returns true.
    @return length on success (queued to all connections successfully).
            On failure or partial failure, return -1. */
static ssize_t fb_queue_broadcast (FB_SERVICE *service, FB_MESSAGE *message, bool (*callback) (FB_CONNECTION *)) {
    size_t length = message->length;
    if (message->length == 0) {
        /* Don't queue empty messages, but feign success. */
        fb_messagefree (message);
        return 0;
    }
    fb_mutex_lock(&service->mutex);
    for (unsigned i = 0; i < service->connection_count; i++) {
        if (service->connections[i]->state >= FB_SOCKET_STATE_OPEN &&
            service->connections[i]->state < FB_SOCKET_STATE_CLOSING &&
            !service->connections[i]->filename &&
            (!callback || callback (service->connections[i]))) {
            message->usecount++;
            if (fb_queue_single (service->connections[i], message) == -1) {
                length = -1;
            }
        }
    }
    fb_mutex_unlock(&service->mutex);
    /* When allocated, use count is already 1.  Correct for this now. */
    fb_messagefree (message);
    return length;
}

/** @internal
    Add a message message to a service, connection, or event.
    @param thing The service, connection or event to which message is sent.
    @param message The message to send.
    @param broadcast True if the message should be sent to all connections.
                     Broadcast is inherently true if 'thing' is a service.
    @return the length of the message, or -1 on error, as per stdio.
            In case of only partial success... Success. */
/* Accepts a service, connection or event because polymorphism is friendly.
   Looks at the type magic number inside the thing to determine which kind it is. */
ssize_t fb_queue_message (void *thing, FB_MESSAGE *message, bool broadcast) {
    assert (thing);
    assert (message);
    FB_SOCKETTYPE type = *(FB_SOCKETTYPE *)thing;
    if (message) {
        switch (type) {
            case FB_SOCKTYPE_SERVICE:
                return fb_queue_broadcast (((FB_SERVICE *)thing), message, NULL);
            case FB_SOCKTYPE_CONNECTION:
                if (broadcast) {
                    return fb_queue_broadcast (((FB_CONNECTION *)thing)->service, message, NULL);
                }
                return fb_queue_single ((FB_CONNECTION *)thing, message);
            case FB_SOCKTYPE_EVENT:
                if (broadcast) {
                    return fb_queue_broadcast (((FB_EVENT *)thing)->connection->service, message, NULL);
                }
                return fb_queue_single (((FB_EVENT *)thing)->connection, message);
            default:
                /* Nothing */
                break;
        }
        fb_log (FB_WHERE (FB_LOG_ERROR), "thing of unknown type %u passed", (unsigned) type);
        assert (!"Unknown thingie type");
        fb_messagefree (message);
    }
    return -1;
}

/** Adds an unformatted message to the output queue.
    @param thing the destined service, connection or event.
    @param bytes the bytes to transmit.
    @param length number of bytes in the message.
    @return -1 on error, number of bytes written on success or partial success. */
ssize_t fb_fprint (void *thing, const char *bytes, ssize_t length) {
    FB_MESSAGE *message;
    if ((message = fb_messagealloc ())) {
        message->length = length;
        message->message = malloc (length);
        if (message->message) {
            memcpy (message->message, bytes, length);
            fb_queue_message (thing, message, false);
            return length;
        }
        fb_perror ("malloc");
        fb_messagefree(message);
    }
    return -1;
}

/** Add messages to output queues in various forms and styles.
    All forms take an opaque type and determine if it's a connection
    or service, so you can write code once and deal with both
    single and broadcast messages.
      - 'b' variants broadcast, given either a connection or service.
            Services always broadcast, regardless of service.
      - 'cb' variants broadcast only to those connections identified
            buy the supplied callback function returning `true`.
      - 'v' accepts a pointer to the format arguments.
    @param thing the destined service, connection or event.
    @param format a printf format string, with values to follow.
    @return -1 on error, number of bytes written on success or partial success. */
ssize_t fb_fprintf (void *thing, const char *format, ...) {
    va_list parameters;
    va_start(parameters, format);
    FB_MESSAGE *message = fb_create_message (format, parameters);
    va_end (parameters);
    return fb_queue_message (thing, message, false);
}

/** @see fb_fprintf */
ssize_t fb_vfprintf (void *thing, const char *format, va_list parameters) {
    FB_MESSAGE *message = fb_create_message (format, parameters);
    return fb_queue_message (thing, message, false);
}
    
/** @see fb_fprintf */
ssize_t fb_bfprintf (void *thing, const char *format, ...) {
    va_list parameters;
    va_start(parameters, format);
    FB_MESSAGE *message = fb_create_message (format, parameters);
    va_end (parameters);
    return fb_queue_message (thing, message, true);
}

/** @see fb_fprintf */
ssize_t fb_bvfprintf (void *thing, const char *format, va_list parameters) {
    FB_MESSAGE *message = fb_create_message (format, parameters);
    return fb_queue_message (thing, message, true);
}

/** @see fb_fprintf */
ssize_t fb_cbfprintf (void *thing, bool (*callback) (FB_CONNECTION *), const char *format, ...) {
    va_list parameters;
    va_start(parameters, format);
    ssize_t result = fb_cbvfprintf (thing, callback, format, parameters);
    va_end (parameters);
    return result;
}

/** @see fb_fprintf */
ssize_t fb_cbvfprintf (void *thing, bool (*callback) (FB_CONNECTION *), const char *format, va_list parameters) {
    FB_MESSAGE *message = fb_create_message (format, parameters);
    assert (thing);
    FB_SOCKETTYPE type = *(FB_SOCKETTYPE *)thing;
    if (message) {
        switch (type) {
            case FB_SOCKTYPE_SERVICE:
                return fb_queue_broadcast (((FB_SERVICE *)thing), message, callback);
            case FB_SOCKTYPE_CONNECTION:
                return fb_queue_broadcast (((FB_CONNECTION *)thing)->service, message, callback);
            case FB_SOCKTYPE_EVENT:
                return fb_queue_broadcast (((FB_EVENT *)thing)->connection->service, message, callback);
            default:
                /* Nothing */
                break;
        }
        fb_log (FB_WHERE (FB_LOG_ERROR), "thing of unknown type %u passed", (unsigned) type);
        assert (!"Unknown thingie type");
        fb_messagefree (message);
    }
    return -1;
}



/** @internal
    Write output to a connection from the queue.
    Events are generated for connection closure only; there are none for writing.

    If the socket is in a closing state, when the buffer is empty, close the connection.
    @param event A partially complete event record, or NULL.
                 If NULL, events are delayed (along with moving state toward closure)
                 until a later time, when an event can be returned for those state changes.
    @param connection The connection whose queued data is to be sent.
    @return FB_EVENT_CLOSE or NULL */
FB_EVENT *fb_send_output (FB_EVENT *event, FB_CONNECTION *connection) {
    assert (connection);
#ifdef FOOTBALL_THREADS
    int gotten = pthread_mutex_trylock (&connection->mutex);
    if (gotten != 0) {
        fb_set_writable (connection->socket, true);
        return NULL;
    }
#endif
    if (fb_queue_empty (&connection->out)) {
        /* Output queue is empty. */
        if (!event) {
            // Do nothing
        } else if (connection->state == FB_SOCKET_STATE_FLUSHING) {
            if (connection->http) {
                fb_websocket_announce_close (connection);
            }
            /* The output buffer is empty, send a close event before finalizing */
            connection->state = FB_SOCKET_STATE_CLOSING;
            event->type = FB_EVENT_CLOSE;
            fb_mutex_unlock(&connection->mutex);
            return event;
        } else if (connection->state == FB_SOCKET_STATE_CLOSING) {
            /* If the service is closing and this is the last connection, schedule reap */
            if (connection->service->connection_count == 1 &&
                connection->service->state == FB_SOCKET_STATE_CLOSING) {
                fb_schedule_reap (connection->service);
            }
            /* Disassemble the connection */
            fb_mutex_unlock (&connection->mutex);
            fb_destroy_connection (connection);
            return NULL;
        } else {
            /* The queue is empty, turn off write monitoring */
            fb_set_writable (connection->socket, false);
        }
    } else {
        ssize_t written;
        do {
            FB_MESSAGE *message = connection->out.first->message;
            size_t outlen = message->length - connection->out.consumed;

            written = connection->transport->write (connection,
                                                    message->message + connection->out.consumed,
                                                    outlen);
            if (written >= 0) {
                fb_queue_consume (&connection->out, written);
            } if (written == FB_TRANSPORT_FAILURE) {
                /* Socket has closed/connection is lost. */
                fb_queue_destroy (&connection->out);
                fb_close_connection (connection);
                break;
            }
        } while (written > 0 && !fb_queue_empty (&connection->out));
        if (!fb_queue_empty (&connection->out)) {
            fb_set_writable (connection->socket, true);
        }
    }
    fb_mutex_unlock (&connection->mutex);
    return NULL;
}

/** @internal
    Expand the input buffer to a specified size.
    @param connection The connection whose buffer size is being set.
    @param size the desired size.
    @return true on success, false on failure. */
bool fb_set_input_buffer_size (FB_CONNECTION *connection, size_t size) {
    assert (size > connection->in.capacity); /* Shouldn't be called uselessly */
    const size_t new_capacity = size + 100;
    char *newbuf = realloc (connection->in.message, new_capacity);
    if (!newbuf) {
        fb_perror ("realloc");
        return false;
    }
    connection->in.message = newbuf;
    connection->in.capacity = new_capacity;
    return true;
}

/** @internal
    Read some bytes directly from a socket or via TLS if encrypted.
    Handle connection closing and errors.
    @param connection the connection to read input from.
    @param byte_count the number of bytes to read
    @return true if the byte count is met, false otherwise. */
bool fb_recv_input (FB_CONNECTION *connection, ssize_t byte_count) {
    assert (connection);
    assert (byte_count > 0);
    assert (connection->in.size + byte_count <= connection->in.capacity);
    ssize_t bytes_read;

    bytes_read = connection->transport->read (connection,
                                             connection->in.message + connection->in.size,
                                             byte_count);
    if (bytes_read == FB_TRANSPORT_CLOSED) {
        /* EOF/Connection closed from other end. Initiate closure. */
        if (connection->in.size != 0) {
            fb_log (FB_WHERE (FB_LOG_WARNING),
                    "#%d: Connection closed with non-empty input buffer.",
                    connection->socket);
        }
        fb_close_connection (connection);
        return false;
    } else if (bytes_read == FB_TRANSPORT_FAILURE) {
        fb_close_connection (connection);
        return false;
    } else if (bytes_read < 0) {
        assert (bytes_read == FB_TRANSPORT_INCOMPLETE);
        /* Nothing to read right now */
        return false;
    }
    connection->in.size += bytes_read;
    assert (bytes_read <= byte_count);
    return bytes_read >= byte_count;
}

/** @internal
    Read line-oriented input until encountering a newline.
    If there isn't a newline, the read input is kept in the connection's
    input buffer between invocations.
    @param connection the connection to read from.
    @param length the number of bytes in the completed input
    @return a pointer to the input, or NULL if it is not complete yet. */
static char *fb_get_line_bytes (FB_CONNECTION *connection, size_t *length) {
    assert (connection);
    bool ok;
    do {
        assert (connection->in.size <= connection->in.capacity);
        if (connection->in.size == connection->in.capacity) {
            if (!fb_set_input_buffer_size (connection, connection->in.capacity * 2 + 64)) {
                return false;
            }
        }
        ok = fb_recv_input (connection, 1);
    } while (ok && connection->in.message [connection->in.size - 1] != '\n');
    if (ok) {
        *length = connection->in.size;
        connection->in.size = 0;
        return connection->in.message;
    }
    return NULL;
}

/** @internal
    Read input from a connection, return it as an event.
    The event has the raw command in 'command'.
    The event has a broken up command in 'argv'.  On failure, 'argv' may be null.
    @param event a partially filled event applicable to the connection.
    @param connection the connection from which input is read
    @return An FB_EVENT_INPUT event, or input is incomplete.
 */
FB_EVENT *fb_read_line_input (FB_EVENT *event, FB_CONNECTION *connection) {
    assert (connection);
    assert (event);
    char *line;
    char *buffer = NULL;
    size_t buffilled = 0;
    
    /* If we got here, socket_data will be for a connection */
    event->type = FB_EVENT_INPUT;
    line = fb_get_line_bytes (connection, &buffilled);
    if (!line) {
        /* Haven't got a full line yet, or error. */
        return NULL;
    }

    /* Make a copy of the line */
    buffer = malloc (buffilled + 1);
    if (!buffer) {
        fb_perror ("malloc");
        return NULL;
    }
    memcpy (buffer, line, buffilled);
    /* strip returns off the end of the unparsed line */
    while (buffilled > 0 && (buffer [buffilled - 1] == '\r' || buffer [buffilled - 1] == '\n')) {
        buffilled--;
    }
    buffer [buffilled] = '\0';
    
    /* Store the command and parse it into an argv array */
    event->command = buffer;
    event->argc = fb_create_argv(event->command, &event->argv);
    if (event->argc < 0) {
        event->argc = 0;
    }
    
    return (event);
}

/** @internal
    Read input based on protocol and state.
    If applicable, returns an event for the input.
      - FB_EVENT_INPUT is returned when there is input
      - FB_EVENT_CONNECT is returned on greeting or WebSocket establishment
    @param ev a partially-filled event applicable to this input
    @param connection the connection from which input is read
    @return an event (if applicable), or NULL. */
FB_EVENT *fb_read_input (FB_EVENT *ev, FB_CONNECTION *connection) {
    FB_SERVICE *service = connection->service;
    FB_EVENT *event;
    ssize_t status;
    switch (connection->state) {
        case FB_SOCKET_STATE_TLS_HANDSHAKE:
#ifdef FB_HAVE_TLS
            status = connection->transport->handshake (connection);
            if (status < 0) {
                if (status == FB_TRANSPORT_FAILURE) {
                    fb_destroy_connection (connection);
                }
                return NULL;
            }
            fb_log (FB_WHERE (FB_LOG_TLS_STATUS), "#%d: TLS handshake successful",
                    connection->socket);
#else
            assert (!"Connection is in TLS handshake state without TLS support");
#endif
            connection->state = FB_SOCKET_STATE_GREETING;
            /* FALLTHROUGH */
        case FB_SOCKET_STATE_GREETING:
            event = fb_read_line_input (ev, connection);
            if (event && event->argc) {
                if (service->options.greeting_mode != FB_GREETING_OFF &&
                    strcasecmp (event->argv [0], service->options.greeting) == 0) {
                    if (event->argv [1]) {
                        /* A name was given with the greeting request */
                        if (!fb_transfer_by_name(connection, event->argv [1])) {
                            fb_log (FB_WHERE (FB_LOG_CONN_STATUS),
                                    "#%d: Greeted requesting unknown service",
                                    connection->socket);
                            fb_destroy_connection(connection);
                            return NULL;
                        }
                    }
                    fb_log (FB_WHERE (FB_LOG_CONN_STATUS),
                            "#%d: Received greeting, switching to line mode",
                            connection->socket);
                    connection->state = FB_SOCKET_STATE_OPEN;
                    connection->expiry_time = FAR_FUTURE;
                    connection->http = false;
                    event->type = FB_EVENT_CONNECT;
                    return (event);
                }
                if (fb_http_command (event->argv [0])) {
                    fb_collect_http_request (event, &connection->request);
                    connection->state = FB_SOCKET_STATE_GATHERING_HEADER;
                    connection->http = true;
                    return NULL;
                }
                if (service->options.greeting_mode == FB_GREETING_FALLBACK) {
                    fb_log (FB_WHERE (FB_LOG_CONN_STATUS),
                            "#%d: Unknown request, falling back to line mode.",
                            connection->socket);
                    /* Make a copy of the event and queue it for delivery later. */
                    static FB_EVENT message;
                    fb_dispose_event(&message);
                    message = *event;
                    fb_queue_event (&message);
                    /* Return a connection event now. */
                    event->argv = NULL;
                    event->argc = 0;
                    event->command = NULL;
                    connection->state = FB_SOCKET_STATE_OPEN;
                    connection->expiry_time = FAR_FUTURE;

                    event->type = FB_EVENT_CONNECT;
                    connection->http = false;
                    return (event);
                }
                fb_log (FB_WHERE (FB_LOG_CONN_STATUS), "#%d: Invalid request: %s",
                        connection->socket, event->command);
                /* Protocol error.  Report error and drop connection. */
                fb_destroy_connection(connection);
            }
            return NULL;
        case FB_SOCKET_STATE_GATHERING_HEADER:
            event = fb_read_line_input (ev, connection);
            if (event && event->argc) {
                /* Stash line in header collection */
                fb_collect_http_parameter (event->command, &connection->request);
                return NULL;
            }
            /* On end of header, do something. */
            return event ? fb_execute_http_request (event, connection) : NULL;
        case FB_SOCKET_STATE_OPEN:
            if (!connection->http && service->options.greeting_mode == FB_GREETING_ALLOW && !connection->greeted) {
                event = fb_read_line_input (ev, connection);
                connection->greeted = (event != NULL);
                return event && event->argc && strcasecmp (event->argv [0], service->options.greeting) == 0 ?
                        NULL : event;
            }
            return connection->http ? fb_read_websocket_input (ev, connection) :
                                      fb_read_line_input (ev, connection);
        case FB_SOCKET_STATE_FLUSHING:
            assert (!"Reading input in flushing state");
            fb_accept_input (connection, false);
            return NULL;
        default:
            assert (!"Switch/case unhandled.");
            return NULL;
    }
    assert (!"Reached end of function");
    return NULL;
}

/** @internal
    Accept a new connection and return an event announcing it if applicable.
    @param event a partially filled event applicable to the new connection.
    @param service the service on which the connection is arriving.
    @return FB_EVENT_CONNECTED for successful line-oriented connections, NULL otherwise.
 */
FB_EVENT *fb_new_connect (FB_EVENT *event, FB_SERVICE *service) {
    assert (event);
    assert (service);
    FB_SOCKETID id;
    for (id = 0; id < FB_SOCKET_COUNT; id++) {
        if (service->socket [id] == event->socket) break;
    }
    assert (id != FB_SOCKET_COUNT);
    event->connection = fb_accept_connection (service, id);
    if (event->connection) {
        if (fb_register(event->connection->socket, FB_SOCKTYPE_CONNECTION, event->connection)) {
            event->socket = event->connection->socket;
            event->context = event->connection->context;
            event->service = service;
            event->type = FB_EVENT_CONNECT;
            fb_log (FB_WHERE (FB_LOG_CONN_STATUS |
                              (fb_encrypted_socket(id) ? FB_LOG_TLS_STATUS : 0)),
                    "#%d: New %s IP%d %s connection from %s",
                    event->connection->socket,
                    fb_encrypted_socket(id) ? "encrypted" : "cleartext",
                    fb_ip6_socket(id) ? 6 : 4,
                    fb_http_socket(id) ? "HTTP" : "line",
                    fb_connection_info (event->connection));
            return (event->connection->state == FB_SOCKET_STATE_OPEN ? event : NULL);
        }
        fb_destroy_connection(event->connection);
    }
    return NULL;
}



