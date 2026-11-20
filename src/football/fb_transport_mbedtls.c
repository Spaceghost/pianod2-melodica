///
/// Football transport implementations for mBed TLS (formerly Polar SSL).
/// @file       fb_transport_mbedtls.c - Football socket abstraction layer
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

#include <mbedtls/net.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/pem.h>
#include <mbedtls/pk.h>
#include <mbedtls/error.h>

#ifndef NDEBUG
#include <mbedtls/debug.h>
#endif

static bool fb_tls_initialized;
static mbedtls_x509_crt tls_certificate;
static mbedtls_pk_context tls_public_key;
static mbedtls_x509_crt root_ca_certificate;
static mbedtls_x509_crl root_revokation_certificate;

static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context random_context;

static mbedtls_ssl_config server_config;



#ifndef NDEBUG
static void mbedtls_debug( void *ctx, int level,
                          const char *file, int line, const char *str )
{
    (void) level;
    (void) ctx;
    fb_log (FB_WHERE (FB_LOG_TLS_STATUS), "%s:%04d: %s", file, line, str);
}
#endif


static void fb_mbedtls_error (struct fb_connection_t *connection, const char *func, int error) {
    char buffer [128];
    mbedtls_strerror(error, buffer, sizeof (buffer));
    fb_log (FB_WHERE (FB_LOG_TLS_ERROR), "#%d: %s: %s", connection->socket, func, buffer);
}

static void fb_mbedtls_config_error (const char *func, int error) {
    char buffer [128];
    mbedtls_strerror(error, buffer, sizeof (buffer));
    fb_log (FB_WHERE (FB_LOG_ERROR), "%s: %s", func, buffer);
}



/// @internal Load certificates and keys.
bool fb_mbedtls_configure (const FB_TLS_CONFIG_FILENAMES *path) {
    assert (path);
    assert (!fb_tls_initialized);

    mbedtls_entropy_init (&entropy);
    mbedtls_ctr_drbg_init (&random_context);
    mbedtls_ctr_drbg_seed (&random_context, mbedtls_entropy_func, &entropy, (unsigned char *) "libfootball", 11);

    int status = mbedtls_x509_crt_parse_file (&tls_certificate, path->x509_certificate_name);
    if (status == 0) {
        status = mbedtls_pk_parse_keyfile (&tls_public_key, path->public_key_name, NULL);
        if (status == 0) {
            mbedtls_x509_crt_init (&root_ca_certificate);
            fb_tls_initialized = true;
            return true;
        } else {
            fb_mbedtls_config_error ("mbedtls_x509_crt_parse_file", status);
        }
    } else {
        fb_mbedtls_config_error ("mbedtls_x509_crt_parse_file", status);
    }
    return false;
}



/** @internal
    Initialize the TLS stuff for a new connection.
    @param connection The connection to initialize.
    @return true on success, false on error. */
bool fb_mbedtls_init (FB_CONNECTION *connection) {
    assert (fb_tls_initialized);
    if (!fb_tls_initialized) {
        fb_log (FB_WHERE (FB_LOG_ERROR), "TLS credentials not set.  Call fb_init_tls_support().");
        return false;
    }

    mbedtls_ssl_config_init (&server_config);
    mbedtls_ssl_config_defaults (&server_config, MBEDTLS_SSL_IS_SERVER,
                                 MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_endpoint (&server_config, MBEDTLS_SSL_IS_SERVER);
    mbedtls_ssl_conf_authmode (&server_config, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_ca_chain (&server_config, &root_ca_certificate, &root_revokation_certificate);
    mbedtls_ssl_conf_own_cert (&server_config, &tls_certificate, &tls_public_key);

    mbedtls_ssl_conf_ciphersuites (&server_config, mbedtls_ssl_list_ciphersuites ());

    mbedtls_ssl_conf_rng (&server_config, mbedtls_ctr_drbg_random, &random_context);
#ifndef NDEBUG
    mbedtls_ssl_conf_dbg(&server_config, mbedtls_debug, stderr);
    mbedtls_debug_set_threshold (0);
#endif

    mbedtls_ssl_init (&connection->tls.context);
    mbedtls_ssl_setup (&connection->tls.context, &server_config);

    mbedtls_ssl_set_bio (&connection->tls.context, &connection->socket,
                         mbedtls_net_send, mbedtls_net_recv, NULL);
    return true;
}


/// @internal Perform TLS handshaking on a new connection.  Return incomplete, failure, or 0.
ssize_t fb_mbedtls_handshake (struct fb_connection_t *connection) {
    int status = mbedtls_ssl_handshake (&connection->tls.context);
    if (status < 0 && status != MBEDTLS_ERR_SSL_WANT_READ && status != MBEDTLS_ERR_SSL_WANT_WRITE) {
        fb_mbedtls_error (connection, "mbedtls_ssl_handshake", status);
        return FB_TRANSPORT_FAILURE;
    }
    return (status == 0 ? 0 : FB_TRANSPORT_INCOMPLETE);
}



/** @internal Query number of bytes in TLS buffers.
    mbedtls doesn't seem to have a pending bytes function,
    or possibly I am unable to find it in their documentation.
    To simulate, we read one byte and buffer it, then tack it
    onto the front of the next read. */
ssize_t fb_mbedtls_buffering (struct fb_connection_t *connection) {
    if (!connection->tls.pending_read) {
        int bytes_read = mbedtls_ssl_read (&connection->tls.context, &connection->tls.buffered_char, 1);
        connection->tls.pending_read = (bytes_read == 1);
    }
    return connection->tls.pending_read;
}


/// @internal Read data from a TLS connection using mBedTLS.
ssize_t fb_mbedtls_read (struct fb_connection_t *connection, char *data, ssize_t byte_count) {
    assert (connection);
    assert (data);
    assert (byte_count >= 0);

    if (byte_count > 0 && connection->tls.pending_read) {
        *data = (char) connection->tls.buffered_char;
        connection->tls.pending_read = false;
        if (byte_count == 1)
            return 1;
        int bytes_read = mbedtls_ssl_read (&connection->tls.context, (unsigned char *) data + 1, byte_count - 1);
        return (bytes_read < 0 ? 1 : (bytes_read + 1));
    }

    int bytes_read = mbedtls_ssl_read (&connection->tls.context, (unsigned char *) data, byte_count);
    if (bytes_read < 0 && bytes_read != MBEDTLS_ERR_SSL_WANT_READ && bytes_read != MBEDTLS_ERR_SSL_WANT_WRITE) {
        fb_mbedtls_error (connection, "fb_mbedtls_read", bytes_read);
        return FB_TRANSPORT_FAILURE;
    };
    return bytes_read >= 0 ? bytes_read : FB_TRANSPORT_INCOMPLETE;
}

/// @internal Write data to a TLS connection using mBedTLS.
ssize_t fb_mbedtls_write (struct fb_connection_t *connection, const char *data, ssize_t byte_count) {
    assert (connection);
    assert (data);
    assert (byte_count >= 0);

    int written = mbedtls_ssl_write (&connection->tls.context, (const unsigned char *) data, byte_count);
    if (written < 0 && written != MBEDTLS_ERR_SSL_WANT_READ && written != MBEDTLS_ERR_SSL_WANT_WRITE) {
        fb_mbedtls_error (connection, "fb_mbedtls_write", written);
        return FB_TRANSPORT_FAILURE;
    }
    return written >= 0 ? written : FB_TRANSPORT_INCOMPLETE;
}

void fb_mbedtls_done (FB_CONNECTION *connection) {
    mbedtls_ssl_free (&connection->tls.context);
}

void fb_mbedtls_cleanup () {
    mbedtls_x509_crt_free (&tls_certificate);
    mbedtls_x509_crt_free (&root_ca_certificate);
    mbedtls_x509_crl_free (&root_revokation_certificate);

    mbedtls_ssl_config_free (&server_config);
    mbedtls_entropy_free (&entropy);
    mbedtls_ctr_drbg_free (&random_context);
    mbedtls_pk_free (&tls_public_key);
}



const FB_TRANSPORT_FUNCS fb_transport_encrypted = {
    .configure = fb_mbedtls_configure,
    .cleanup = fb_mbedtls_cleanup,
    .init = fb_mbedtls_init,
    .handshake = fb_mbedtls_handshake,
    .buffering = fb_mbedtls_buffering,
    .read = fb_mbedtls_read,
    .write = fb_mbedtls_write,
    .done = fb_mbedtls_done
};
