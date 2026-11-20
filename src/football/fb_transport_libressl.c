///
/// Football transport implementations for LibreSSL (libtls).
/// @file       fb_transport_libressl.c - Football socket abstraction layer
/// @author     Perette Barella
/// @date       2017-08-13
/// @copyright  Copyright 2017 Devious Fish. All rights reserved.
///


#include <config.h>

#include <assert.h>
#include <errno.h>

#include <tls.h>

#include "fb_public.h"
#include "fb_transport.h"
#include "fb_service.h"


//static const SSL_METHOD *server_method;
static struct tls_config *tls_configuration;
static struct tls *server_configuration;


/// @internal Initialize state, load certificates and keys.
bool fb_libressl_configure (const FB_TLS_CONFIG_FILENAMES *paths) {
    assert (paths);
    assert (!server_configuration);

    if (tls_init() < 0) {
    	fb_log (FB_WHERE (FB_LOG_ERROR), "tls_init(): Failed");
        return false;
    };

    if ((tls_configuration = tls_config_new ())) {
        int status = tls_config_set_cert_file (tls_configuration, paths->x509_certificate_name);
        if (status == 0) {
            status = tls_config_set_key_file (tls_configuration, paths->public_key_name);
            if (status == 0) {
                if ((server_configuration = tls_server ())) {
                    if (tls_configure (server_configuration, tls_configuration) == 0) {
                        return true;
                    } else {
                        fb_log (FB_WHERE (FB_LOG_ERROR), "tls_configure(): Failed");
                    }
                    tls_free (server_configuration);
                    server_configuration = NULL;
                } else {
                    fb_log (FB_WHERE (FB_LOG_ERROR), "tls_server(): Error or out of memory");
                }
            } else {
                fb_log (FB_WHERE (FB_LOG_ERROR), "tls_config_set_key_file(): %s", tls_config_error (tls_configuration));
            }
        } else {
            fb_log (FB_WHERE (FB_LOG_ERROR), "tls_config_set_cert_file(): %s", tls_config_error (tls_configuration));
        }
        tls_config_free (tls_configuration);
        tls_configuration = NULL;
    } else {
        fb_log (FB_WHERE (FB_LOG_ERROR), "tls_config_new(): Error or out of memory");
    }
    return false;
}



/** @internal
    Initialize the TLS stuff for a new connection.
    @param connection The connection to initialize.
@return true on success, false on error. */
bool fb_libressl_init (FB_CONNECTION *connection) {
    assert (server_configuration);
    if (!server_configuration) {
        fb_log (FB_WHERE (FB_LOG_ERROR), "TLS/server configuration not set.  Call fb_init_tls_support().");
        return false;
    }
    if (tls_accept_socket (server_configuration, &connection->tls.context, connection->socket) == 0) {
        return true;
    } else {
        fb_log (FB_WHERE (FB_LOG_ERROR), "tls_accept_socket(): #%d: Error accepting socket", connection->socket);
    }
    return false;
}


/// Perform TLS handshaking on a new connection.  Return incomplete, failure, or 0.
ssize_t fb_libressl_handshake (struct fb_connection_t *connection) {
    int status = tls_handshake (connection->tls.context);
    if (status == TLS_WANT_POLLIN || status == TLS_WANT_POLLOUT) {
        return FB_TRANSPORT_INCOMPLETE;
    } else if (status < 0) {
        const char *message = tls_error (connection->tls.context);
        fb_log (FB_WHERE (FB_LOG_ERROR), "tls_handshake(): %s", message ? message : "Error");
        return FB_TRANSPORT_FAILURE;
    }
    return 0;
}



/// Query number of bytes in TLS buffers.
ssize_t fb_libressl_buffering (struct fb_connection_t *connection) {
    assert (connection);

    if (!connection->tls.pending_read) {
        ssize_t bytes_read = tls_read (connection->tls.context, &connection->tls.buffered_char, 1);
        connection->tls.pending_read = (bytes_read == 1);
    }
    return connection->tls.pending_read;
}


/// @internal Read data from a TLS connection using LibreSSL.
ssize_t fb_libressl_read (struct fb_connection_t *connection, char *data, ssize_t byte_count) {
    assert (connection);
    assert (data);
    assert (byte_count >= 0);

    if (byte_count > 0 && connection->tls.pending_read) {
        *data = connection->tls.buffered_char;
        connection->tls.pending_read = false;
        if (byte_count == 1)
            return 1;
        ssize_t bytes_read = tls_read (connection->tls.context, data + 1, byte_count - 1);
        return (bytes_read < 0 ? 1 : (bytes_read + 1));
    }


    ssize_t bytes_read = tls_read (connection->tls.context, data, byte_count);
    if (bytes_read == TLS_WANT_POLLIN || bytes_read == TLS_WANT_POLLOUT) {
        return FB_TRANSPORT_INCOMPLETE;
    } else if (bytes_read < 0) {
        const char *message = tls_error (connection->tls.context);
        fb_log (FB_WHERE (FB_LOG_ERROR), "tls_read(): %s", message ? message : "Error");
        return (int) FB_TRANSPORT_FAILURE;
    }
    return bytes_read;
}

/// @internal Write data to a TLS connection using LibreSSL.
ssize_t fb_libressl_write (struct fb_connection_t *connection, const char *data, ssize_t byte_count) {
    assert (connection);
    assert (data);
    assert (byte_count >= 0);

    ssize_t written = tls_write (connection->tls.context, data, byte_count);
    if (written == TLS_WANT_POLLIN || written == TLS_WANT_POLLOUT) {
        return FB_TRANSPORT_INCOMPLETE;
    } else if (written <= 0) {
        const char *message = tls_error (connection->tls.context);
        fb_log (FB_WHERE (FB_LOG_ERROR), "tls_write(): %s", message ? message : "Error");
        return (int) FB_TRANSPORT_FAILURE;
    }
    return written >= 0 ? written : FB_TRANSPORT_INCOMPLETE;
}

void fb_libressl_done (FB_CONNECTION *connection) {
    int status = tls_close (connection->tls.context);
    if (status == TLS_WANT_POLLIN || status == TLS_WANT_POLLOUT) {
        fb_log (FB_WHERE (FB_LOG_WARNING), "#%d: Socket closed before ready", connection->socket);
    } else if (status <= 0) {
        const char *message = tls_error (connection->tls.context);
        fb_log (FB_WHERE (FB_LOG_ERROR), "tls_close(): %s", message ? message : "Error");
    }
    tls_free (connection->tls.context);
}

void fb_libressl_cleanup () {
    tls_free (server_configuration);
    server_configuration = NULL;
    tls_config_free (tls_configuration);
    tls_configuration = NULL;
}


const FB_TRANSPORT_FUNCS fb_transport_encrypted = {
    .configure = fb_libressl_configure,
    .cleanup = fb_libressl_cleanup,
    .init = fb_libressl_init,
    .handshake = fb_libressl_handshake,
    .buffering = fb_libressl_buffering,
    .read = fb_libressl_read,
    .write = fb_libressl_write,
    .done = fb_libressl_done
};
