///
/// Football utility functions.
/// @file       fb_utility.c - Football socket abstraction layer
/// @author     Perette Barella
/// @date       2014-03-26
/// @copyright  Copyright 2012-2020 Devious Fish. All rights reserved.
///

#include <config.h>

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <memory.h>
#include <time.h>
#include <arpa/inet.h>

#include "fb_public.h"
#include "fb_service.h"



static int logmode = 0;

/** Set logging level.
    Set which messages will be collected by the internal logging function, or
    provide an alternate logging function.  When using a private logging function,
    logtype is not used.
    @param logtype a bitmask indicating which data to log.
    @param func a logging function that replaces the default one.
 */
void fb_set_logging (int logtype, FB_LOGGING_FUNCTION func) {
    logmode = logtype;
    if (func != NULL) {
        fb_log = func;
    }
}

/** @internal
    Default logging implementation: Log stuff to stderr.
    The file, line, and func parameters are removed for release builds
    (when NDEBUG is set.)
    An application can provide an alternative function using the second
    parameter to fb_set_logging.
    @see fb_set_logging
    @see FB_WHERE
    @param file the filename from which the message originates.
    @param line the line number at which the message originates.
    @param func the function from which the message originates.
    @param level a bitmask indicating the type of the message (0=error, always logged)
    @param format a printf-style format string, with parameters to follow. */
void fb_log_impl (
#ifndef NDEBUG
                  const char *file, int line, const char *func,
#endif
                  int level, const char *format, ...) {
    char date [22];
    if (level == 0 || (logmode & level)) {
        va_list parameters;
        va_start (parameters, format);
        time_t now = time (NULL);
        strftime(date, sizeof (date),"%Y-%m-%d %H:%M:%S", localtime(&now));
#ifdef NDEBUG
        fprintf (stderr, "%s: ", date);
#else
        const char *shortfile = strrchr (file, '/');
        fprintf (stderr, "%s: %s:%d (%s): ", date, shortfile ? shortfile + 1 : file, line, func);
#endif
        vfprintf(stderr, format, parameters);
        fputc('\n', stderr);
        va_end(parameters);
    }
}
FB_LOGGING_FUNCTION fb_log = fb_log_impl;


/** @internal
    Return a a connection's opposing end as a string.
    @param connection the connection to get the string of.
    @return a string with the info, or a stand-in value. */
const char *fb_connection_info (FB_CONNECTION *connection) {
    static char buffer [200];
    if (connection->filename) {
        return "a file";
    }
#ifdef HAVE_IPV6
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
    if (address) {
        size_t len = strlen (address);
        if (len < (sizeof (buffer) - 50)) {
#ifdef HAVE_IPV6
            sprintf (buffer + len, " port %d",
                     connection->domain == PF_INET6 ?
                     ntohs (connection->origin.ip6addr.sin6_port) :
                     ntohs (connection->origin.ip4addr.sin_port));
#else
            sprintf (buffer + len, " port %d",
                     ntohs (connection->origin.ip4addr.sin_port));
#endif
        }
    }
    return address ? address : "???";
}


/** calloc() with ability to expand the allocated data.
    Does nothing if the actual capacity meets or exceeds the requested capacity.
    When expansion occurs, the added elements are zeroed out.
    @param data the allocation to expand.  May be *NULL for initial allocation.
    @param capacity the capacity of the allocation, which may be adjusted by this call.
    @param newcapacity the desired capacity of the allocation.
    @param itemsize the size of data items in the allocation.
    @return true on success, false on failure (existing allocation remains valid). */
bool fb_expandcalloc (void **data, size_t *capacity, size_t newcapacity, size_t itemsize) {
    assert ((*capacity == 0) == (*data == NULL));
    if (newcapacity <= *capacity) {
        /* It's big enough already */
        return true;
    }
    /* Expand in reasonable chunks when necessary, not by drips. */
    size_t mincapacity = *capacity * 2 + 25;
    if (mincapacity > newcapacity) {
        newcapacity = mincapacity;
    }
    size_t oldsize = *capacity * itemsize;
    size_t newsize = newcapacity * itemsize;
    char *newdata = realloc (*data, newsize);
    if (!newdata) {
        return false;
    }
    *data = (void *) newdata;
    memset(newdata + oldsize, 0, newsize - oldsize);
    *capacity = newcapacity;
    return true;
}


/** @internal
    Free up resources used by an argv array.
    @param argv The array to release. */
void fb_destroy_argv (char **argv) {
    if (argv) {
        /* The array is built from chopped up pieces of one line, so just free the first. */
        free (*argv);
        /* And free the vector itself */
        free (argv);
    }
}


/** @internal
    Create an argv-style array.
    A command line with nothing on it results in 0 and a pointer to an array
    with one null.
    @param commandline The command to split up.  This original string remains unaltered.
    @param result The place to put the argv array.
    @return The number of populated terms in the argv array.
            On error, returns a negative number and a null pointer. */
int fb_create_argv (const char *commandline, char ***result) {
    /* ***result: Anything can be solved with sufficient indirection */
    /* Skip leading whitespace */
    while (*commandline && isspace (*commandline)) {
        commandline++;
    }
    /* Make a copy of the command line to scratch up */
    char *command = strdup (commandline);
    if (command == NULL) {
        fb_perror ("strdup");
        *result = NULL;
        return -1;
    }
    /* First get a quick count of the words. */
    int wordcount = 0;
    char *c = command;
    while (*c) {
        wordcount++;
        while (*c && !isspace (*c)) {
            c++;
        }
        while (*c && isspace (*c)) {
            c++;
        }
    }
    /* Allocate a vector for the pointers and populate it */
    char **argv = (char **) calloc(wordcount + 1, sizeof (char *));
    if (argv == NULL) {
        fb_perror ("calloc");
        free (command);
        *result = NULL;
        return -1;
    }
    c = command;
    wordcount = 0;
    while (*c) {
        argv [wordcount++] = c;
        char quote = *c;
        if (quote == '"' || quote == '\'') {
            /* Quoted string */
            char *shifted = c++;
            while (*c) {
                if (*c == quote && *(c+1) == quote) {
                    *(shifted++) = quote;
                    c += 2;
                } else if (*c == quote && (isspace (*(c+1)) || *(c+1) == '\0')) {
                    c++;
                    break;
                } else {
                    *(shifted++) = *(c++);
                }
            }
            *shifted = '\0';
        } else {
            /* Plain word */
            while (*c && !isspace (*c)) { /* skip the word */
                c++;
            }
        }
        if (*c) { /* Null terminate if needed */
            *(c++) = '\0';
        }
        while (*c && isspace (*c)) { /* Skip whitespace */
            c++;
        }
    }
    assert (argv [wordcount] == NULL);
    if (wordcount == 0) {
        /* The command isn't actually used, so free it before it leaks */
        free (command);
    }
    *result = argv;
    return wordcount;
}



static char hexvalue (char ch) {
    static const char digits[] = "0123456789abcdef";
    return strchr (digits, tolower (ch)) - digits;
}


/** @internal
    Create arrays of parameter names and values from an HTTP query parameter string.
    An empty query string results in 0 and pointers arrays with one null.
    @param query_string The command to split up.  This original string remains unaltered.
    @param result_names The place to put the parameter name array reference.
    @param result_values The place to put the parameter value array reference.
    @return The number of populated terms in the parameter array.
            On error, returns a negative number and a null pointer. */
int fb_create_argv_from_query_string (const char *query_string, char ***result_names, char ***result_values) {
    /* ***result: Anything can be solved with sufficient indirection */
    assert (query_string);
    assert (result_names);
    assert (result_values);
    
    /* Make a copy of the paramters to scratch up */
    char *query = strdup (query_string);
    if (query == NULL) {
        fb_perror ("strdup");
        return -1;
    }
    /* First get a quick count of the words. */
    int wordcount = 2;
    char *c = query;
    while (*c) {
        if (*c == '&') {
            wordcount += 2;
        }
        c++;
    }
    /* Allocate a vector for the pointers and populate it */
    char **names = (char **) calloc((wordcount + 1) * 2, sizeof (char *));
    if (names == NULL) {
        fb_perror ("calloc");
        free (query);
        return -1;
    }
    char **values = names + wordcount + 1;
    c = query;
    wordcount = 0;
    while (*c) {
        names [wordcount] = c;
        char *out = c;
        while (*c && *c != '&') {
            if (*c == '=' && !values[wordcount]) {
                values[wordcount] = c+1;
                *out = '\0';
            } if (*c == '+') {
                *out = ' ';
            } else if (*c == '%') {
                if (*(c+1) && isxdigit (*(c+1))) {
                    char hex = hexvalue (*(++c));
                    if (*(c+1) && isxdigit (*(c+1))) {
                        hex = (hex << 4) | hexvalue (*(++c));
                    }
                    *out = hex;
                } else {
                    *out = '%';
                }
            } else if (out != c) {
                *out = *c;
            }
            out++;
            c++;
        }
        if (*c) {
            c++;
        }
        *out = '\0';
        if (!values [wordcount]) {
            // If parameter value is missing, insert an empty one.
            values[wordcount] = out;
        }
        wordcount++;
    }
    assert (names [wordcount] == NULL);
    assert (values [wordcount] == NULL);

    if (wordcount == 0) {
        /* The command isn't actually used, so free it before it leaks */
        free (query);
    }
    *result_names = names;
    *result_values = values;
    return wordcount;
}
           
