///
/// Football public declarations.
/// @file       fb_public.h - Football socket abstraction layer
/// @author     Perette Barella
/// @date       2012-03-03
/// @copyright  Copyright 2012-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <stdarg.h>
#include <stdbool.h>
#include "stdarg.h"
#include <sys/types.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Event types returned from the socket manager.
    For FB_EVENT_CONNECT on HTTP or TLS ports, the event occurs when the session greets or
    begins a websocket session.  Connections may silently exist in the socket manager before
    that. */
typedef enum fb_event_type_t {
	FB_EVENT_CONNECT = 1,	/**< Connection: A line or websocket session is beginning. */
	FB_EVENT_INPUT,			/**< Connection: Input from a line or websocket session. */
	FB_EVENT_CLOSE,			/**< Connection: Connection is closing. */
	FB_EVENT_STOPPED,		/**< Service: No remaining connections, service is terminating. */
	FB_EVENT_ITERATOR,		/**< Iterator response: Connection is open. */
	FB_EVENT_ITERATOR_CLOSE,/**< Iterator response: Connection is closing */
	FB_EVENT_WRITABLE,		/**< User thingies only: Socket is write-ready. */
	FB_EVENT_READABLE,		/**< User thingies only: Socket is read-ready. */
	FB_EVENT_FAULTING,		/**< User thingies only: Socket has an error pending. */
	FB_EVENT_TIMEOUT,       /**< The poll timeout expired. */
    FB_EVENT_INTERRUPT      /**< A signal interrupted while waiting */
} FB_EVENTTYPE;

/** Events are returned in this structure */
typedef struct fb_event_t {
	int magic; /**< Private. */ /* Enums and ints are same thing, right? :( TODO: Fix this. */
	FB_EVENTTYPE type;
	int socket; /**< The socket creating an event, for user thingies only. */
	void *context; /**< Provided for connections, or if supplied for user thingies */
	struct fb_connection_t *connection; /**< For connect, close, and input */
	struct fb_service_t *service; /**< All connection events and service closed */
	char *command; /**< A command received on a connection, raw form */
	int argc; /**< The number of terms in argv */
	char **argv; /**< Lexically split command received on a connection. */
    int param_count; /**< Number of entries in param_names & param_values. */
    char **param_names; /**< Parameter names (for HTTP query parameters) */
    char **param_values; /**< Values corresponding to param_names positions (for HTTP query parameters). */
} FB_EVENT;

/** Greeting mode allows a service to require, accept, or not use greetings to trigger a
    line session via the HTTP or TLS port.  Greetings reduce port pollution, and encrypted
    line sessions are only supported via greeting. */
typedef enum fb_service_greeting_mode_t {
    FB_GREETING_OFF, /**< Immediately start line session, error on HELO.  HELO not accepted on HTTP. */
    FB_GREETING_ALLOW, /**< Immediately start line session but ignore HELO; accept on HTTP and start line session */
    FB_GREETING_FALLBACK, /**< Like Allow, but unrecognized HTTP request triggers line session. */
    FB_GREETING_REQUIRE /**< Both line and HTTP sessions wait for HELO.  Require HELO to start line session */
} FB_GREETING_MODE;

/** Internationalization mode-- how to mangle filenames for language-specific files */
typedef enum fb_internationalizaton_mode_t {
    FB_INTERNATIONALIZATION_NONE, /** Do not look for language-specific files */
    FB_INTERNATIONALIZATION_EXTENSION, /**< Append `.<lang>` to filenames */
    FB_INTERNATIONALIZATION_DIRECTORY /**< Insert `/<lang>/` after service path */
} FB_I18N_MODE;

/** Service options are passed to a new service, defining its behavior. */
typedef struct fb_service_options_t {
    int line_port; /**< Line-oriented port, or 0 to disable. */
    int http_port; /**< HTTP (or combo depending on service mode) port, or 0 to disable. */
    int https_port; /**< HTTP Secure port, or combo, or 0 to disable. */
    int queue_size; /**< Listen queue size before connections are rejected. */
    size_t context_size; /**< Size of user context allocated for new connections */
    char *greeting; /**< Default HELO */
    int greeting_timeout; /**< Limit greeting and/or TLS negotiation, 0 to disable. */
    char *name; /**< Name of service, for URL processing and hosts_access lookups. */
    char *serve_directory; /**< Location of files to be served by HTTP */
    char *locale_directory; /**< Subdirectory with internationalization files for HTTP */
    time_t websocket_ping_interval; /** How often to send a ping on websocket; 0 = never */
    FB_I18N_MODE internationalization; /**< Mode for locating localized files for HTTP */
    FB_GREETING_MODE greeting_mode; /**< Whether to accept/require greeting.  See FB_GREETING_MODE */
    bool transfer_only; /** Service will accept transfers; no ports required. */
    struct fb_service_t *parent; /** Parent that may direct HELO and URLs to us */
} FB_SERVICE_OPTIONS;

typedef struct fb_service_t FB_SERVICE;
typedef struct fb_connection_t FB_CONNECTION;
typedef struct fb_iterator_t FB_ITERATOR;

#ifdef NDEBUG
typedef void (* FB_LOGGING_FUNCTION)(int level, const char *format, ...);
#else
typedef void (* FB_LOGGING_FUNCTION)(const char *file, int line, const char *func, int level, const char *format, ...);
#endif
extern void fb_set_logging (int logtype, FB_LOGGING_FUNCTION func);
extern FB_LOGGING_FUNCTION fb_log;

/* Useful utility functions available */
extern bool fb_expandcalloc (void **data, size_t *count, size_t newcount, size_t itemsize);

/* "Legitimate", i.e. published API, subroutines */
extern FB_EVENT *fb_wait (void);
extern FB_EVENT *fb_poll (void);
extern FB_EVENT *fb_poll_until (time_t untilwhen);
extern FB_EVENT *fb_poll_with_timeout (double timeout);
extern void fb_interrupt (void);

extern FB_SERVICE *fb_create_service (const FB_SERVICE_OPTIONS *options);
extern bool fb_transfer (FB_CONNECTION *connection, FB_SERVICE *service);
extern bool fb_services_are_open (void);
extern void fb_close_service (FB_SERVICE *service);
extern bool fb_init_tls_support (const char *path);
    extern void fb_cleanup_tls_support (void);

extern FB_EVENT *fb_accept_file (FB_SERVICE *service, char *filename);
extern FB_EVENT *fb_loopback_socket (FB_SERVICE *service);
extern void fb_close_connection (FB_CONNECTION *connection);
extern ssize_t fb_fprint (void *thing, const char *bytes, ssize_t length);
extern ssize_t fb_fprintf (void *thing, const char *format, ...);
extern ssize_t fb_vfprintf (void *thing, const char *format, va_list parameters);
extern ssize_t fb_bfprintf (void *thing, const char *format, ...);
extern ssize_t fb_bvfprintf (void *thing, const char *format, va_list parameters);
extern ssize_t fb_cbfprintf (void *thing, bool (*callback) (FB_CONNECTION *), const char *format, ...);
extern ssize_t fb_cbvfprintf (void *thing, bool (*callback) (FB_CONNECTION *), const char *format, va_list parameters);

extern FB_ITERATOR *fb_new_iterator (FB_SERVICE *service);
extern FB_EVENT *fb_iterate_next (struct fb_iterator_t *it);
extern void fb_destroy_iterator (FB_ITERATOR *it);
extern void fb_accept_input (FB_CONNECTION *connection, bool input);

#ifdef __cplusplus
}
#endif


