///
/// Football transport layer implementation.
/// Generic functions and support for unsecured connections and files.
/// @file       fb_transport.c - Football socket abstraction layer
/// @author     Perette Barella
/// @date       2016-03-09
/// @copyright  Copyright 2016 Devious Fish. All rights reserved.
///

#include <config.h>

#include <errno.h>
#include <assert.h>

#include "fb_public.h"
#include "fb_transport.h"
#include "fb_service.h"

#include <sys/socket.h>

#ifndef HAVE_MSG_NOSIGNAL
#define MSG_NOSIGNAL (0)
#ifndef HAVE_SO_NOSIGPIPE
#error Neither MSG_NOSIGNAL nor SO_NOSIGPIPE available; unreliable!
#endif
#endif

#define TLS_PUBLIC_KEY_FILE     "x509-server-key.pem"
#define TLS_CERTIFICATE_FILE    "x509-server.pem"

/// Connection closed normally.  Returned by read transport function only.
const ssize_t FB_TRANSPORT_CLOSED = 0;

/// An action was incomplete, but can be retried in the future.
const ssize_t FB_TRANSPORT_INCOMPLETE = -1;

/// An action failed permanently; the connection has failed and should be closed.
const ssize_t FB_TRANSPORT_FAILURE = -2;

//
//                     Unencrypted BSD Sockets
//

/// @internal Initialize an unencrypted BSD sockets connection.
bool fb_unencrypted_init (struct fb_connection_t *connection) {
    assert (connection);
    (void) connection;
    return true;
}

/// Perform TLS handshaking on a new connection.  Return incomplete, failure, or 0.
ssize_t fb_unencrypted_handshake (struct fb_connection_t *connection) {
    assert (connection);
    (void) connection;
    return 0;
}


/// Query number of bytes in TLS buffers.
ssize_t fb_unencrypted_buffering (struct fb_connection_t *connection) {
    assert (connection);
    (void) connection;
    return 0;
}


/// @internal Read data from an unencrypted BSD Sockets connection.
ssize_t fb_unencrypted_read (struct fb_connection_t *connection, char *data, ssize_t byte_count) {
    assert (connection);
    assert (data);
    assert (byte_count >= 0);

    ssize_t bytes_read = recv (connection->socket, data, byte_count, 0);  /* Only send() needs MSG_NOSIGNAL */
    if (bytes_read < 0 && errno != EAGAIN && errno != EINTR) {
        fb_log (FB_WHERE (FB_LOG_TLS_ERROR),
                "#%d: %s: %s", connection->socket, "recv", strerror (errno));
        return FB_TRANSPORT_FAILURE;
    };
    return bytes_read >= 0 ? bytes_read : FB_TRANSPORT_INCOMPLETE;
}

/// @internal Write data to an unencrytped BSD Sockets connection.
ssize_t fb_unencrypted_write (struct fb_connection_t *connection, const char *data, ssize_t byte_count) {
    assert (connection);
    assert (data);
    assert (byte_count >= 0);

    ssize_t written = send (connection->socket, data, byte_count, MSG_NOSIGNAL);
    if (written < 0 && errno != EAGAIN && errno != EINTR) {
        fb_log (FB_WHERE (FB_LOG_TLS_ERROR),
                "#%d: %s: %s", connection->socket, "send", strerror (errno));
        return FB_TRANSPORT_FAILURE;
    }
    return written >= 0 ? written : FB_TRANSPORT_INCOMPLETE;
}

/// @internal Close an unencrypted BSD Sockets connection.
void fb_unencrypted_done (FB_CONNECTION *connection) {
    // Nothing to do.
    (void) connection;
}

/// Transport virtual method table for unenrypted connections.
const FB_TRANSPORT_FUNCS fb_transport_unencrypted = {
    .init = fb_unencrypted_init,
    .handshake = fb_unencrypted_handshake,
    .buffering = fb_unencrypted_buffering,
    .read = fb_unencrypted_read,
    .write = fb_unencrypted_write,
    .done = fb_unencrypted_done
};



//
//                     Reading from a file
//


/// @internal Prepare to read from a file in the guise of a connection.
bool fb_file_init (struct fb_connection_t *connection) {
    assert (connection);
    fprintf (stderr, "[Start of %s transcript]\n", connection->filename);
    return true;
}

/// @internal Read data from an unencrypted BSD Sockets connection.
ssize_t fb_file_read (struct fb_connection_t *connection, char *data, ssize_t byte_count) {
    assert (connection);
    assert (data);
    assert (byte_count >= 0);

    ssize_t bytes_read = read (connection->socket, data, byte_count);
    if (bytes_read < 0 && errno != EAGAIN && errno != EINTR) {
        fb_log (FB_WHERE (FB_LOG_TLS_ERROR),
                "#%d: %s: %s", connection->socket, "read", strerror (errno));
        return FB_TRANSPORT_FAILURE;
    };
    return bytes_read >= 0 ? bytes_read : FB_TRANSPORT_INCOMPLETE;
}

/// @internal Send writes to the logfile when reading from a file.
ssize_t fb_file_write (struct fb_connection_t *connection, const char *data, ssize_t byte_count) {
    assert (connection);
    assert (data);
    assert (byte_count >= 0);

    write (fileno (stderr), data, byte_count);

    return byte_count;
}

/// @internal Wrap up reading from a file.
void fb_file_done (FB_CONNECTION *connection) {
    fprintf (stderr, "[End of %s transcript]\n", connection->filename);
}

/// Transport virtual method table for reading from files.
const FB_TRANSPORT_FUNCS fb_transport_read_file = {
    .init = fb_file_init,
    .handshake = fb_unencrypted_handshake,
    .buffering = fb_unencrypted_buffering,
    .read = fb_file_read,
    .write = fb_file_write,
    .done = fb_file_done
};



#ifdef FB_HAVE_TLS

 /// Track number of initializations, and cleanup only when matched.
static int fb_tls_initializations;

/** Initialize TLS support structures.
    This may be called harmlessly when built without TLS support.
    @param path location/prefix of the key/certificate data files.
    path MUST have a '/' if the name is a directory
    - "/etc/" becomes "/etc/x509-server-key.pem".
    - "/etc" becomes "/etcx509-server-key.pem".
    - "/etc/myapp-" becomes "/etc/myapp-x509-server-key.pem".
    @return true on success, false on failure (allocation error, files not found). */
bool fb_init_tls_support (const char *path) {
    if (fb_tls_initializations) {
        fb_tls_initializations++;
        return true;
    }
    assert (path);
    FB_TLS_CONFIG_FILENAMES files;
    memset(&files, '\0', sizeof (files));

    if ((asprintf (&files.public_key_name, "%s%s", path, TLS_PUBLIC_KEY_FILE) < 0)) {
        fb_perror("asprintf");
        files.public_key_name = NULL;
    }
    if ((asprintf (&files.x509_certificate_name, "%s%s", path, TLS_CERTIFICATE_FILE) < 0)) {
        fb_perror("asprintf");
        files.x509_certificate_name = NULL;
    }
    bool status = false;
    if (files.public_key_name && files.x509_certificate_name) {
        status = fb_transport_encrypted.configure (&files);
        if (status) {
            fb_tls_initializations++;
        }
    }
    free (files.public_key_name);
    free (files.x509_certificate_name);
    free (files.x509_revokation_name);
    return status;
}

void fb_cleanup_tls_support (void) {
    if (fb_tls_initializations) {
        fb_tls_initializations--;
        fb_transport_encrypted.cleanup();
    }
}

#else

bool fb_init_tls_support (const char *path) {
    assert (path);
    fb_log (FB_WHERE (FB_LOG_WARNING), "Invoked on build without FB_HAVE_TLS (mostly harmless).");
    return false;
}

void fb_cleanup_tls_support (void) {
}

#endif
