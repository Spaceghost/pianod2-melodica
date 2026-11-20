///
/// Football socket management and dispatching.
/// @file       fb_socketmgr.c - Football socket abstraction layer
/// @author     Perette Barella
/// @date       2012-03-03
/// @copyright  Copyright 2012-2021 Devious Fish. All rights reserved.
///

#include <config.h>

#ifndef __FreeBSD__
#define _BSD_SOURCE /* strdup() */
#define _DARWIN_C_SOURCE /* strdup() on OS X */
#define _DEFAULT_SOURCE
#endif
#ifdef __linux__
#include <linux/posix_types.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/select.h>

#include <assert.h>

#include "fb_service.h"

/* Some Linuxes seem to be missing these */
#ifndef FD_COPY
#define FD_COPY(from,to) memmove(to, from, sizeof(*(from)))
#endif

/* The strategy here is to just register sockets in an array by their socket number.
   When socket N needs attention, it's easy and fast to find.  We also store the
   socket number in with that data, so can refer back. */
typedef struct socket_data_t {
    int socket;
    FB_SOCKETTYPE type;
    union {
        FB_CONNECTION *connection;
        FB_SERVICE *service;
        void *user;
    } thingie; /* Holds one of those thingies, y'know? */
} FB_SOCKET_DATA;

/* We need 3 bit arrays for the select() system call, one for each possible
   I/O state for each socket. We need a fourth for GNUTLS, which may buffer too. */
typedef enum select_action_t {
    ACTION_READING,
    ACTION_WRITING,
    ACTION_FAULTING,
    ACTION_BUFFERING,
    ACTION_COUNT,
    ACTION_SELECT_COUNT = 3
} ACTION;


static FB_SERVICE *reapq = NULL;
static FB_SOCKET_DATA **sockets; /**< Index by socket number to service/connection/etc. */
static int maxsockets = 0;
static int activesockets = 0;
static fd_set select_state [ACTION_COUNT];    /**< What we'll use on the next select() */
static fd_set last_state [ACTION_SELECT_COUNT];    /**< What came back on the last select() */
static FB_EVENT *queued_event = NULL; /**< Pending event (NULL if none, or single event. */
static bool tls_currently_buffering; /**< Set when TLS has stuff in its buffers. */
static volatile bool interrupt_pending = false; /**< Return EVENT_INTERRUPTED on EINTR. */

#ifdef FOOTBALL_THREADS
/** Sync registering/unregistering sockets. */
static pthread_mutex_t socketmanager_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tickler_mutex = PTHREAD_MUTEX_INITIALIZER;
static int tickler_fds [2] = { -1, -1 };
#define TICKLER_RECEIVE (0)
#define TICKLER_SEND (1)
#endif

/** @internal
    Schedule a service for reaping when it has no connections left. */
void fb_schedule_reap (FB_SERVICE *service) {
    /* It's called reapq, but they're stacked. Order doesn't matter. */
    service->next_reap = reapq;
    reapq = service;
}

/** @internal
    Queue an event.
    Only used for fallback mode on HTTP port, when we need to
    deliver a connection arrival and then the command, so size is limited
    to 1.
    @param event the event to queue
*/
void fb_queue_event (FB_EVENT *event) {
    assert (!queued_event);
    queued_event = event;
}

/** @internal
    Destroy an event and free its resources.
    Events are all statically allocated, so we never free them
    in the current implementation.  However, dispose frees the
    dynamically allocated elements and clears out the structure.
    @param event the event to free */
void fb_dispose_event (FB_EVENT *event) {
    free (event->command);
    fb_destroy_argv (event->argv);
    fb_destroy_argv (event->param_names);
    memset (event, 0, sizeof (*event));
}

/** Return an FB_EVENT_INTERRUPTED on current or next poll. */
void fb_interrupt (void) {
    interrupt_pending = true;
}

/** @internal
    Increase the registry to accommodate more sockets.
    @param required_size The minimum size required.  This function may
    choose to go larger.
    @return True on success, false on failure. */
static bool expand_socket_registry (int required_size) {
    /* Make sure we've got enough space in the socket registry */
    if (maxsockets <= required_size) {
        /* maxsockets affects the select(), so used a balanced
         approach to growing the collection */
        int newsize = maxsockets + (maxsockets / 4) + 10;
        if (newsize <= required_size) {
            newsize = required_size + 10;
        }
        if (newsize > FD_SETSIZE) {
            newsize = FD_SETSIZE;
        }
        FB_SOCKET_DATA **newsockets = realloc (sockets, newsize * sizeof (FB_SOCKET_DATA *));
        if (newsockets == NULL) {
            fb_perror ("realloc");
            return false;
        }
        /* Zero out the new portion of the collection */
        memset (newsockets + maxsockets, 0, (newsize - maxsockets) * sizeof (FB_SOCKET_DATA *));
        sockets = newsockets;
        maxsockets = newsize;
    }
    return true;
}

#ifdef FOOTBALL_THREADS
/** @internal
    Lock the socket registry when changing it.  If the read thread is
    currently in select(), tickle it to get it out, and steal the
    mutex away. */
static bool fb_lock_socketmanager_mutex (void) {
    if (tickler_fds [0] == -1) {
        if (pipe (tickler_fds) != 0) {
            fb_perror ("pipe");
            return false;
        }
        if (!expand_socket_registry (tickler_fds [TICKLER_RECEIVE])) {
            close (tickler_fds [0]);
            close (tickler_fds [1]);
            tickler_fds [0] = -1;
            tickler_fds [1] = -1;
            return false;
        }
        FD_SET (tickler_fds [TICKLER_RECEIVE], &select_state[ACTION_READING]); /* Enable input */
    }
    int gotten = pthread_mutex_trylock (&socketmanager_mutex);
    if (gotten != 0) {

        fb_mutex_lock (&tickler_mutex);
        /* Send the tickle */
        if (write (tickler_fds [TICKLER_SEND], "X", 1) != 1) {
            fb_perror ("write");
        };

        fb_mutex_lock (&socketmanager_mutex);
        fb_mutex_unlock (&tickler_mutex);
    }
    return true;
}
#else
#define fb_lock_socketmanager_mutex() true
#endif


/** Add a socket to the registry.
    @param socket_fd The file descriptor of the socket being added.
    @param type Type of socket (service, connection, user...)
    @param thing Pointer of type indicated by 'type'.
    @return true on success, false on failure */
bool fb_register (int socket_fd, FB_SOCKETTYPE type, void *thing) {
    /* We're limited by select(2), unless we want to do unsupported things */
    if (socket_fd >= FD_SETSIZE) {
        return false;
    }

    if (!fb_lock_socketmanager_mutex ()) {
        return false;
    };

    if (expand_socket_registry (socket_fd)) {
        /* Insert the socket into the registry. */
        assert (sockets [socket_fd] == NULL);        /* There shouldn't be anything there yet. */
        if ((sockets [socket_fd] = malloc (sizeof (FB_SOCKET_DATA)))) {
            sockets [socket_fd]->thingie.user = thing;
            sockets [socket_fd]->socket = socket_fd;
            sockets [socket_fd]->type = type;
            FD_SET (socket_fd, &select_state[ACTION_READING]); /* Enable input */
            if (socket_fd >= activesockets) {
                activesockets = socket_fd + 1;
            }
            fb_mutex_unlock (&socketmanager_mutex);
            return true;
        }
        fb_perror ("malloc");
    }
    fb_mutex_unlock (&socketmanager_mutex);
    return false;
}

/** Remove a socket from the registry.
    Always succeeds, unless there's a bug.
    @param socket_fd the file descriptor to unregister */
void fb_unregister (int socket_fd) {
    assert (socket_fd > 0 && socket_fd < maxsockets); /* We shouldn't be freeing sockets that don't fit in the registry. */
    assert (sockets [socket_fd]); /* The socket to be freed should exist in the registry. */
    
    fb_lock_socketmanager_mutex ();
    /* Turn off listening to it and cancel any pending processing */
    int i;
    for (i = 0; i < ACTION_COUNT; i++) {
        FD_CLR(socket_fd, &select_state[i]);
    }

    free (sockets [socket_fd]);
    sockets [socket_fd] = NULL;

    /* Stop polling inactive sockets to reduce impact on select() */
    while (activesockets > 0 && sockets [activesockets - 1] == NULL) {
        activesockets--;
    }
    fb_mutex_unlock (&socketmanager_mutex);
}


/** @internal
    Enable/disable the flags for selecting, or the buffering flags.
    @param socket_fd the file descriptor to set flags for
    @param group which flags to adjust (read, write, ...)
    @param enable true to enable the flag, false to clear it */
void fb_set_socket_select_flags (int socket_fd, ACTION group, bool enable) {
    assert (socket_fd > 0 && socket_fd < activesockets);
    assert (sockets [socket_fd] != NULL);

    fb_lock_socketmanager_mutex();
    if (sockets [socket_fd]) {
        if (enable) {
            FD_SET(socket_fd, &select_state[group]);
        } else {
            FD_CLR(socket_fd, &select_state[group]);
        }
    }
    fb_mutex_unlock (&socketmanager_mutex);
}



/** @internal
    Enable/disable writing on a socket.
    This is used to remember TLS data buffering.
    @param socket_fd the file descriptor to apply flags to
    @param enable true if TLS is buffering, false if it is not. */
void fb_set_buffering (int socket_fd, bool enable) {
    fb_set_socket_select_flags (socket_fd, ACTION_BUFFERING, enable);
    if (enable) {
        tls_currently_buffering = true;
    }
}

/** @internal
    Enable/disable writing on a socket.
    This is used within Football, enabled when fresh data is queued for a socket and
    disabled when the buffer is empty.
    @param socket_fd the file descriptor to apply flags to
    @param enable true to enable write-polling on the socket */
void fb_set_writable (int socket_fd, bool enable) {
    fb_set_socket_select_flags (socket_fd, ACTION_WRITING, enable);
}

/** @internal
    Enable/disable reading on a socket.
    @param socket_fd the file descriptor to apply flags to
    @param enable true to enable reads from the socket, false to disable */
void fb_set_readable (int socket_fd, bool enable) {
    fb_set_socket_select_flags (socket_fd, ACTION_READING, enable);
}

/** Public interface to enable/disable reading on a connection.
    @param connection - the connection to adjust.
    @param input true to enable reading, false to disable. */
void fb_accept_input (FB_CONNECTION *connection, bool input) {
    assert (connection);
    assert (connection->state >= FB_SOCKET_STATE_OPEN);
    
    fb_set_readable (connection->socket, input && connection->state <= FB_SOCKET_STATE_OPEN);
    if (input) {
        /* Set buffering flag if TLS is buffering data for this socket. */
        if (connection->encrypted && connection->state > FB_SOCKET_STATE_TLS_HANDSHAKE &&
                                     connection->state <= FB_SOCKET_STATE_OPEN) {
            fb_set_buffering (connection->socket, connection->transport->buffering (connection));
        }
    } else {
        /* Clear buffering flag so we don't try to read that way either. */
        fb_set_buffering (connection->socket, false);
    }
}

/** @internal
    Called by the poll routine, this function takes a socket and an action
    and processes it, either inline or by calling another function to do
    the work.
    @param socket_fd file descriptor of a socket that is ready/wants attention.
    @param action indicates if socket is ready for reading, writing, etc.
    @return an event, when appropriate.  
            If things are handled completely internally, returns NULL. */
FB_EVENT *fb_process_event (int socket_fd, ACTION action) {
    static FB_EVENT event;
    fb_mutex_lock(&socketmanager_mutex);
    assert (socket_fd >= 0 && socket_fd < activesockets);
    assert (action >= 0 && action < ACTION_SELECT_COUNT);
    assert (sockets [socket_fd]);

    /* Look up the type of the event originator */
    FB_SOCKET_DATA *socket_data = sockets [socket_fd];
    fb_mutex_unlock (&socketmanager_mutex);

    if (socket_data == NULL) {
        return NULL;
    }

    /* Release any even resources, then clear it out */
    fb_dispose_event (&event);

    /* Initialize the event in case we need it */
    event.magic = FB_SOCKTYPE_EVENT;
    event.socket = socket_fd;

    switch (socket_data->type) {
        case FB_SOCKTYPE_SERVICE:
            switch (action) {
                case ACTION_READING:
                    return fb_new_connect (&event, socket_data->thingie.service);
                default:
                    assert (!"Service socket unexpected readiness");
                    break;
            }
            break;
        case FB_SOCKTYPE_CONNECTION:
            event.context = socket_data->thingie.connection->context;
            event.connection = socket_data->thingie.connection;
            event.service = event.connection->service;
            switch (action) {
                case ACTION_READING:
                {
                    FB_EVENT *e = fb_read_input (&event, socket_data->thingie.connection);
#ifdef FB_HAVE_TLS
                    // Need to make sure socket is still open before using it.
                    if (sockets [socket_fd] &&
                        socket_data->thingie.connection->encrypted) {
                        fb_set_buffering (socket_fd,
                                          socket_data->thingie.connection->state <= FB_SOCKET_STATE_OPEN &&
                                          FD_ISSET (socket_fd, &select_state [ACTION_READING]) &&
                                          socket_data->thingie.connection->transport->buffering (socket_data->thingie.connection));
                    }
#endif
                    return e;
                }
                case ACTION_WRITING:
                    return fb_send_output (&event, socket_data->thingie.connection);
                default:
                    assert (!"Connection socket unexpected readiness");
                    break;
            }
            break;
        case FB_SOCKTYPE_USER:
            event.context = socket_data->thingie.user;
            switch (action) {
                case ACTION_READING:
                    event.type = FB_EVENT_READABLE;
                    return &event;
                case ACTION_WRITING:
                    event.type = FB_EVENT_WRITABLE;
                    return &event;
                case ACTION_FAULTING:
                    event.type = FB_EVENT_FAULTING;
                    return &event;
                default:
                    assert (!"User socket unexpected readiness");
                    break;
            }
            break;
        default:
            fb_log (FB_WHERE (FB_LOG_ERROR), "Invalid socket type %d in switch", socket_data->type);
            assert (!"Unxpected socket type");
            return NULL;
    }
    fb_log (FB_WHERE (FB_LOG_ERROR), "Event type not handled: type=%d, action=%d", socket_data->type, action);
    return NULL;
}


/** @internal
    Remove items from the reap queue.
    Defer parent services until all children are dead.
    @return an event structure, or NULL; neither is an error. */
static FB_EVENT *fb_perform_reaping () {
    assert (reapq);
    static FB_EVENT event;

    memset (&event, 0, sizeof (event));
    event.type = FB_EVENT_STOPPED;
    FB_SERVICE *back = NULL;
    FB_SERVICE *reap = reapq;
        
    /* Free services that have already been reap notified */
    while (reap) {
        FB_SERVICE *current = reap;
        reap = reap->next_reap;
        if (current->shutdown_event_done && current->next_child && !current->options.parent) {
            /* This is a parent with children.  Skip it. */
        } else if (!current->shutdown_event_done) {
            /* Send STOPPED event with valid service pointer */
            current->shutdown_event_done = true;
            event.service = current;
            return &event;
        } else {
            /* Eligible for destruction. */
            if (!back) {
                reapq = reap;
            } else {
                back->next_reap = reap;
            }
            fb_destroy_service (current);
            fb_free_freelists ();
        }
        back = reap;
    }
    /* After last service is reaped, return STOPPED event with no service. */
    if (activesockets == 0) {
        return &event;
    }
    return NULL;
}


/** @internal
    Subtract time values.
    @param [in,out] minuend The time being subtracted from.
    @param subtrahend The value being subtracted. */
static void subtract_from_timeval (struct timeval *minuend, const struct timeval *subtrahend) {
    // Subtract the delta from the timeout.
    minuend->tv_usec -= subtrahend->tv_usec;
    minuend->tv_sec -= subtrahend->tv_sec;
    if (minuend->tv_usec < 0) {
        minuend->tv_usec += 1000000;
        minuend->tv_sec--;
    }
}


/** @internal
    Poll the stuff in the registry.
    This is the worker function; the four exposed functions with various timeout
    mechanisms follow.
    @param timeout Duration to poll, or NULL for indefinite wait.
    @return an event structure, or NULL on failure. */
static FB_EVENT *fb_poll_for (struct timeval *timeout) {
pollagain:;
    static FB_EVENT event; /* Reusable timeout event */
    static int events_remaining = 0;
    static ACTION process_action = 0;
    static int process_fd = 0;

    /* If an event was queued, give it precedence. */
    if (queued_event) {
        FB_EVENT *temp = queued_event;
        queued_event = NULL;
        return temp;
    }

    /* See if there's any reaping queued; if so, do it. */
    if (reapq) {
        FB_EVENT *reap = fb_perform_reaping ();
        if (reap) {
            return reap;
        }
    }
    
    /* Ping any websocket connections that have been idle.
       Also, close connections that are past their expiry time. */
    time_t now = time (NULL);
    time_t next_ping = FAR_FUTURE;
    for (int i = 0; i < activesockets; i++) {
        if (sockets [i] && sockets [i]->type == FB_SOCKTYPE_CONNECTION) {
            FB_CONNECTION *connection = sockets [i]->thingie.connection;
            if (connection->expiry_time < now && connection->state <= FB_SOCKET_STATE_OPEN) {
                fb_log (FB_WHERE (FB_LOG_CONN_STATUS), "#%d: Connection expired",
                        connection->socket);
                fb_close_connection (connection);
            } else if (connection->http && connection->state == FB_SOCKET_STATE_OPEN) {
                if (connection->next_ping < now) {
                    fb_websocket_ping (connection);
                }
                if (connection->next_ping < next_ping) {
                    next_ping = connection->next_ping;
                }
            }
        }
    }
    next_ping -= now;

    /* Something should have been registered by now. */
    assert (maxsockets > 0);
    if (activesockets <= 0) {
        /* If there are no registrations left, return. */
        fb_log (FB_WHERE (FB_LOG_ERROR), "No sockets registered.");
        return NULL;
    }
    
    /* See if it's time to refill the coffers, so-to-speak */
    if (events_remaining == 0) {
        /* Reset the event searching loops */
        process_action = 0;
        process_fd = 0;
        
        /* Create a fresh copy of the selector masks */
        fb_mutex_lock(&socketmanager_mutex);
        int i;
        for (i = 0; i < ACTION_SELECT_COUNT; i++) {
            FD_COPY(&select_state[i], &last_state [i]);
        }
        /* Select, repeating if we get an interrupted system call */
        int select_errno;
        do {
            if (interrupt_pending) {
                fb_mutex_unlock(&socketmanager_mutex);
                if (events_remaining < 0) {
                    events_remaining = 0;
                }
                memset (&event, 0, sizeof (event));
                event.magic = FB_SOCKTYPE_EVENT;
                event.type = FB_EVENT_INTERRUPT;
                interrupt_pending = false;
                return (&event);
            }
            struct timeval internal_timeout;
            internal_timeout.tv_usec = 0;
            if (tls_currently_buffering || next_ping < 0) {
                internal_timeout.tv_sec = 0;
            } else if (!timeout || next_ping <= timeout->tv_sec) {
                internal_timeout.tv_sec = next_ping;
            } else if (timeout) {
                internal_timeout.tv_sec = timeout->tv_sec;
                internal_timeout.tv_usec = timeout->tv_usec;
            }
            struct timeval delta;
            delta.tv_sec = internal_timeout.tv_sec;
            delta.tv_usec = internal_timeout.tv_usec;
            events_remaining = select (maxsockets, &last_state [ACTION_READING],
                                       &last_state [ACTION_WRITING], &last_state [ACTION_FAULTING],
                                       &internal_timeout);
            select_errno = errno;
            if (timeout) {
                // Compute timeout used
                subtract_from_timeval (&delta, &internal_timeout);
                // Subtract the delta from the user's timeout.
                subtract_from_timeval (timeout, &delta);
                if (timeout->tv_sec < 0) {
                    timeout->tv_sec = 0;
                }
            }
#ifdef FOOTBALL_THREADS
            if (events_remaining > 0 &&
                FD_ISSET(tickler_fds [TICKLER_RECEIVE], &last_state [ACTION_READING])) {
                fb_mutex_unlock (&socketmanager_mutex);
                fb_mutex_lock(&tickler_mutex);
                FD_CLR(tickler_fds [TICKLER_RECEIVE], &last_state [ACTION_READING]);
                char junk;
                read (tickler_fds [TICKLER_RECEIVE], &junk, 1);
                fb_mutex_unlock (&tickler_mutex);
                if (--events_remaining == 0) {
                    goto pollagain;
                }
                fb_mutex_lock (&socketmanager_mutex);
            }
#endif
        } while (!tls_currently_buffering && events_remaining < 0 && select_errno == EINTR);
        fb_mutex_unlock(&socketmanager_mutex);

        /* Check for/handle (don't handle) errors */
        if (events_remaining < 0) {
            events_remaining = 0;
            fb_perror ("select");
            return NULL;
        }

        /* Add TLS's buffered reads into select's read flags */
        if (tls_currently_buffering) {
            tls_currently_buffering = false;
            for (i = 0; i < maxsockets; i++) {
                if ( FD_ISSET(i, &select_state [ACTION_BUFFERING]) &&
                    !FD_ISSET(i, &last_state [ACTION_READING])) {
                    events_remaining++;
                    FD_SET(i, &last_state [ACTION_READING]);
                }
            }
        }

        /* Check for/handle timeout */
        if (events_remaining == 0) {
            /* We'll just keep reusing this, just clearing it out each time. */
            memset (&event, 0, sizeof (event));
            event.magic = FB_SOCKTYPE_EVENT;
            event.type = FB_EVENT_TIMEOUT;
            return (&event);
        }
    }

    /* Search the mask returned by select to find what needs attention */
    while (process_action < ACTION_SELECT_COUNT) {
        while (process_fd < maxsockets) {
            if (FD_ISSET (process_fd, &last_state [process_action])) {
                /* This needs attention.  Count it and, if we haven't been
                   subsequently told to ignore it, process it. */
                events_remaining--;
                if (FD_ISSET (process_fd, &select_state [process_action])) {
                    FB_EVENT *fd_event = fb_process_event (process_fd, process_action);
                    if (fd_event) {
                        /* An event resulted from the processing. Bump to the */
                        /* next socket so we don't repeat, then return the event. */
                        if (++process_fd >= maxsockets) {
                            process_fd = 0;
                            process_action++;
                        }
                        return (fd_event);
                    }
                }
#ifdef NDEBUG
                /* If there are no more events, stop looking.  But in test mode,
                   continue to make sure counts confirm good behavior. */
                if (events_remaining <= 0) goto pollagain;
#endif
            }
            process_fd++;
        }
        process_fd = 0;
        process_action++;
    }
    if (events_remaining != 0) {
        fb_log (FB_WHERE (FB_LOG_WARNING), "%d event(s) not found", events_remaining);
    }
    events_remaining = 0;
    goto pollagain; /* Tail recursion would probably collapse and work too */
}

/** Quick poll with no waiting.
    @return An event. */
FB_EVENT *fb_poll (void) {
    struct timeval zero;
    zero.tv_sec = 0;
    zero.tv_usec = 0;
    return fb_poll_for (&zero);
}
/** Poll with a duration.
    @param timeout in seconds.
    @return An event. */
FB_EVENT *fb_poll_with_timeout (double timeout) {
    struct timeval _timeout;
    _timeout.tv_sec = timeout;
    _timeout.tv_usec = (timeout - _timeout.tv_sec) * 1000000;
    return fb_poll_for (&_timeout);
}
/** Poll indefinitely.
   @return An event. */
FB_EVENT *fb_wait (void) {
    return fb_poll_for (NULL);
}
/** Poll with a timeout at a specific time.
    @param untilwhen Time at which timeout should occur.
    @return An event.*/
FB_EVENT *fb_poll_until (time_t untilwhen) {
    struct timeval timeout;
    time_t now = time(NULL);
    if (now == -1) { /* According to the man page time() can actually fail */
        fb_perror ("time");
        timeout.tv_sec = 1; /* *shrug*, there's really bigger problems if you don't know what time it is */
    } else {
        long timeLeft = untilwhen - now;
        timeout.tv_sec = (timeLeft < 0) ? 0 : timeLeft;
    }
    timeout.tv_usec = 0;
    return fb_poll_for (&timeout);
}


