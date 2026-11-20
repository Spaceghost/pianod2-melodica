///
/// Football transport implementations for OpenSSL.
/// @file       fb_transport_openssl.c - Football socket abstraction layer
/// @author     Perette Barella
/// @date       2016-03-11
/// @copyright  Copyright 2016-2017 Devious Fish. All rights reserved.
///


#include <config.h>

#include <assert.h>
#include <errno.h>

#include "fb_public.h"
#include "fb_transport.h"
#include "fb_service.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

static const SSL_METHOD *server_method;
static SSL_CTX *server_config;


/// @internal Initialize state, load certificates and keys.
bool fb_openssl_configure (const FB_TLS_CONFIG_FILENAMES *paths) {
    assert (paths);
    assert (!server_config);

    if (SSL_library_init() <= 0) {
    	fb_log (FB_WHERE (FB_LOG_ERROR), "SSL_library_init(): Failed");
        return false;
    };
    SSL_load_error_strings();
	// Choose the most modern TLS/SSL supported by the library.
#ifdef HAVE_TLS_SERVER_METHOD
	// This meta-method gives us the most reasonable TLS/SSL.
    server_method = TLS_server_method();
#elif defined(HAVE_SSLV23_SERVER_METHOD)
	// Earlire, poorly named meta-method.
	server_method = SSLv23_server_method();
#elif defined(HAVE_TLSV1_SERVER_METHOD)
	server_method = TLSv1_2_server_method();
#elif defined(HAVE_TLSV1_2_SERVER_METHOD)
	server_method = TLSv1_1_server_method();
#elif defined(HAVE_TLSV1_SERVER_METHOD)
	server_method = TLSv1_server_method();
#elif defined(HAVE_SSLV3_SERVER_METHOD)
    server_method = SSLv3_server_method();
#else
#error No OpenSSL server method available.
#endif
    server_config = SSL_CTX_new (server_method);
    if (server_config) {
        int status = SSL_CTX_use_certificate_file(server_config, paths->x509_certificate_name, SSL_FILETYPE_PEM);
        if (status > 0) {
            status = SSL_CTX_use_PrivateKey_file(server_config, paths->public_key_name, SSL_FILETYPE_PEM);
            if (status > 0) {
                if (SSL_CTX_check_private_key (server_config) == 1) {
                    return true;
                }
            }
        }
        SSL_CTX_free (server_config);
        server_config = NULL;
    }
    ERR_print_errors_fp (stderr);
    return false;
}



/** @internal
 Initialize the TLS stuff for a new connection.
 @param connection The connection to initialize.
 @return true on success, false on error. */
bool fb_openssl_init (FB_CONNECTION *connection) {
    assert (server_config);
    if (!server_config) {
        fb_log (FB_WHERE (FB_LOG_ERROR), "TLS credentials not set.  Call fb_init_tls_support().");
        return false;
    }

    connection->tls = SSL_new (server_config);
    if (!connection->tls) {
        ERR_print_errors_fp (stderr);
        return false;
    }

    SSL_set_fd(connection->tls, connection->socket);
    return true;
}


/// Perform TLS handshaking on a new connection.  Return incomplete, failure, or 0.
ssize_t fb_openssl_handshake (struct fb_connection_t *connection) {
    int status = SSL_accept (connection->tls);
    if (status <= 0) {
        status = SSL_get_error(connection->tls, status);
        assert (status != SSL_ERROR_NONE);
        if (status != SSL_ERROR_WANT_READ && status != SSL_ERROR_WANT_WRITE) {
            ERR_print_errors_fp (stderr);
            return FB_TRANSPORT_FAILURE;
        }
        return FB_TRANSPORT_INCOMPLETE;
    }
    return 0;
}



/// Query number of bytes in TLS buffers.
ssize_t fb_openssl_buffering (struct fb_connection_t *connection) {
    return SSL_pending (connection->tls) > 0;
}


/// @internal Read data from a TLS connection using OpenSSL.
ssize_t fb_openssl_read (struct fb_connection_t *connection, char *data, ssize_t byte_count) {
    assert (connection);
    assert (data);
    assert (byte_count >= 0);

    int bytes_read = SSL_read (connection->tls, data, (int) byte_count);
    if (bytes_read < 0) {
        int status = SSL_get_error(connection->tls, bytes_read);
        assert (status != SSL_ERROR_NONE);
        if (status != SSL_ERROR_WANT_READ && status != SSL_ERROR_WANT_WRITE) {
            ERR_print_errors_fp (stderr);
            return FB_TRANSPORT_FAILURE;
        }
        return FB_TRANSPORT_INCOMPLETE;
    };
    return bytes_read;
}

/// @internal Write data to a TLS connection using OpenSSL.
ssize_t fb_openssl_write (struct fb_connection_t *connection, const char *data, ssize_t byte_count) {
    assert (connection);
    assert (data);
    assert (byte_count >= 0);

    int written = SSL_write (connection->tls, data, (int) byte_count);
    if (written <= 0) {
        int status = SSL_get_error(connection->tls, written);
        assert (status != SSL_ERROR_NONE);
        if (status != SSL_ERROR_WANT_READ && status != SSL_ERROR_WANT_WRITE) {
            ERR_print_errors_fp (stderr);
            return FB_TRANSPORT_FAILURE;
        }
    }
    return written >= 0 ? written : FB_TRANSPORT_INCOMPLETE;
}

void fb_openssl_done (FB_CONNECTION *connection) {
    SSL_shutdown(connection->tls);
    SSL_free (connection->tls);
    connection->tls = NULL;
}

void fb_openssl_cleanup () {
    SSL_CTX_free (server_config);
    server_config = NULL;
    ERR_free_strings ();
}



const FB_TRANSPORT_FUNCS fb_transport_encrypted = {
    .configure = fb_openssl_configure,
    .cleanup = fb_openssl_cleanup,
    .init = fb_openssl_init,
    .handshake = fb_openssl_handshake,
    .buffering = fb_openssl_buffering,
    .read = fb_openssl_read,
    .write = fb_openssl_write,
    .done = fb_openssl_done
};
