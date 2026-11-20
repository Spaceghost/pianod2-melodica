///
/// Football transport implementations for GNU TLS.
/// @file       fb_transport_gnutls.c - Football socket abstraction layer
/// @author     Perette Barella
/// @date       2016-03-09
/// @copyright  Copyright 2016-2017 Devious Fish. All rights reserved.
///


#include <config.h>

#include <assert.h>
#include <errno.h>

#include "fb_public.h"
#include "fb_transport.h"
#include "fb_service.h"

#include <sys/socket.h>
#include <gnutls/gnutls.h>

/* GNUTLS initialization code cookbooked from GNUTLS manual, pp. 189-192 */
static bool fb_tls_initialized;
static gnutls_certificate_credentials_t fb_tls_credentials;
static gnutls_dh_params_t fb_tls_dh_params;
static gnutls_priority_t fb_tls_priorities;



/// @internal Initialize state, load certificates and keys.
bool fb_gnutls_configure (const FB_TLS_CONFIG_FILENAMES *paths) {
    assert (paths);
    assert (!fb_tls_initialized);

    int status = gnutls_global_init ();
    if (status != GNUTLS_E_SUCCESS) {
        fb_log (FB_WHERE (FB_LOG_ERROR),
                "gnutls_global_init: ", gnutls_strerror (status));
        return false;
    }

    status = gnutls_certificate_allocate_credentials(&fb_tls_credentials);
    if (status == GNUTLS_E_SUCCESS) {
        status = gnutls_certificate_set_x509_key_file (fb_tls_credentials, paths->x509_certificate_name,
                                                       paths->public_key_name, GNUTLS_X509_FMT_PEM);
        if (status == GNUTLS_E_SUCCESS) {
            // Initialize Diffie-Hellman things; see p. 190.
            unsigned int bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_DH, GNUTLS_SEC_PARAM_LEGACY);
            if ((status = gnutls_dh_params_init (&fb_tls_dh_params)) != GNUTLS_E_SUCCESS) {
                fb_log (FB_WHERE (FB_LOG_ERROR), "gnutls_dh_params_init: %s", gnutls_strerror (status));
            } else if ((status = gnutls_dh_params_generate2 (fb_tls_dh_params, bits)) != GNUTLS_E_SUCCESS) {
                fb_log (FB_WHERE (FB_LOG_ERROR), "gnutls_dh_params_generate2: %s", gnutls_strerror (status));
            } else {
                gnutls_certificate_set_dh_params (fb_tls_credentials, fb_tls_dh_params);
                status = gnutls_priority_init (&fb_tls_priorities,
                                               "PERFORMANCE:%SERVER_PRECEDENCE", NULL);
                if (status == GNUTLS_E_SUCCESS) {
                    fb_tls_initialized = true;
                    return true;
                } else {
                    fb_log (FB_WHERE (FB_LOG_ERROR), "gnutls_priority_init: %s", gnutls_strerror (status));
                }
            }
        } else {
            fb_log (FB_WHERE (FB_LOG_ERROR), "%s / %s: gnutls_certificate_set_x509_key_file: %s",
                    paths->x509_certificate_name, paths->public_key_name, gnutls_strerror (status));
        }
        gnutls_certificate_free_credentials (fb_tls_credentials);
        memset (&fb_tls_credentials, 0, sizeof (fb_tls_credentials));
    } else {
        fb_log (FB_WHERE (FB_LOG_ERROR),
                "gnutls_certificate_allocate_credentials: ", gnutls_strerror (status));
    }

    gnutls_global_deinit ();
    return false;
}



/** @internal
 Initialize the TLS stuff for a new connection.
 @param connection The connection to initialize.
 @return true on success, false on error. */
bool fb_gnutls_init (FB_CONNECTION *connection) {
    assert (fb_tls_initialized);
    if (!fb_tls_initialized) {
        fb_log (FB_WHERE (FB_LOG_ERROR), "TLS credentials not set.  Call fb_init_tls_support().");
        return false;
    }
    char *func="gnutls_init";
    int status = gnutls_init (&connection->tls, GNUTLS_SERVER);
    if (status == GNUTLS_E_SUCCESS) {
        func = "gnutls_priority_set";
        status = gnutls_priority_set(connection->tls, fb_tls_priorities);
        if (status == GNUTLS_E_SUCCESS) {
            func = "gnutls_credentials_set";
            status = gnutls_credentials_set (connection->tls, GNUTLS_CRD_CERTIFICATE, fb_tls_credentials);
            if (status == GNUTLS_E_SUCCESS) {
                /* No return value from these next guys... */
                gnutls_certificate_server_set_request (connection->tls, GNUTLS_CERT_IGNORE);
                gnutls_transport_set_int (connection->tls, connection->socket);
                return true;
            }
        }
    }
    fb_log (FB_WHERE (FB_LOG_TLS_ERROR), "%s: %s", func, gnutls_strerror(status));
    return false;
}


/// Perform TLS handshaking on a new connection.  Return incomplete, failure, or 0.
ssize_t fb_gnutls_handshake (struct fb_connection_t *connection) {
    int status = gnutls_handshake (connection->tls);
    if (status < 0) {
        if (gnutls_error_is_fatal (status)) {
            fb_log (FB_WHERE (FB_LOG_TLS_ERROR), "#%d: gnutls_handshake: %s",
                    connection->socket, gnutls_strerror (status));
            return FB_TRANSPORT_FAILURE;
        }
        return FB_TRANSPORT_INCOMPLETE;
    }
    return 0;
}



/// Query number of bytes in TLS buffers.
ssize_t fb_gnutls_buffering (struct fb_connection_t *connection) {
    return gnutls_record_check_pending (connection->tls);
}


/// @internal Read data from a TLS connection using GNU TLS.
ssize_t fb_gnutls_read (struct fb_connection_t *connection, char *data, ssize_t byte_count) {
    assert (connection);
    assert (data);
    assert (byte_count >= 0);


    ssize_t bytes_read = gnutls_record_recv (connection->tls, data, byte_count);
    if (bytes_read < 0 && bytes_read != GNUTLS_E_AGAIN && bytes_read != GNUTLS_E_INTERRUPTED) {
        fb_log (FB_WHERE (FB_LOG_TLS_ERROR),
                "#%d: %s: %s", connection->socket, "gnutls_record_recv", gnutls_strerror ((int) bytes_read));
        return FB_TRANSPORT_FAILURE;
    };
    return bytes_read >= 0 ? bytes_read : FB_TRANSPORT_INCOMPLETE;
}

/// @internal Write data to a TLS connection using GNU TLS.
ssize_t fb_gnutls_write (struct fb_connection_t *connection, const char *data, ssize_t byte_count) {
    assert (connection);
    assert (data);
    assert (byte_count >= 0);

    ssize_t written = gnutls_record_send (connection->tls, data, byte_count);
    if (written < 0 && written != GNUTLS_E_AGAIN && written != GNUTLS_E_INTERRUPTED) {
        fb_log (FB_WHERE (FB_LOG_TLS_ERROR),
                "#%d: %s: %s", connection->socket, "gnutls_record_send", gnutls_strerror ((int) written));
        return FB_TRANSPORT_FAILURE;
    }
    return written >= 0 ? written : FB_TRANSPORT_INCOMPLETE;
}

void fb_gnutls_done (FB_CONNECTION *connection) {
    gnutls_bye (connection->tls, GNUTLS_SHUT_WR);
    gnutls_deinit (connection->tls);
}

void fb_gnutls_cleanup () {
    gnutls_global_deinit ();
}

const FB_TRANSPORT_FUNCS fb_transport_encrypted = {
    .configure = fb_gnutls_configure,
    .cleanup = fb_gnutls_cleanup,
    .init = fb_gnutls_init,
    .handshake = fb_gnutls_handshake,
    .buffering = fb_gnutls_buffering,
    .read = fb_gnutls_read,
    .write = fb_gnutls_write,
    .done = fb_gnutls_done
};
