///
/// Football private transport layer declarations.
/// @file       fb_transport.h - Football socket abstraction layer
/// @author     Perette Barella
/// @date       2016-03-09
/// @copyright  Copyright 2016-2017 Devious Fish. All rights reserved.
///

#ifndef __football__transport__
#define __football__transport__

#include <config.h>

#include "fb_public.h"



#define FB_HAVE_TLS
#if defined(HAVE_LIBGNUTLS)

#include <gnutls/gnutls.h>
typedef gnutls_session_t FB_TLS_CONTEXT;

#elif defined(HAVE_LIBMBEDTLS)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"
#include <mbedtls/ssl.h>
#pragma GCC diagnostic pop

typedef struct fb_tls_context_t {
    mbedtls_ssl_context context;
    bool pending_read;
    unsigned char buffered_char;
} FB_TLS_CONTEXT;

#elif defined(HAVE_LIBTLS)

typedef struct fb_tls_context_t {
    struct tls *context;
    bool pending_read;
    char buffered_char;
} FB_TLS_CONTEXT;

#elif defined(HAVE_LIBSSL)

#include <openssl/ssl.h>

typedef SSL *FB_TLS_CONTEXT;

#elif defined(HAVE_SECURETRANSPORT)

#include <Security/SecureTransport.h>

typedef SSLContextRef FB_TLS_CONTEXT;

#else
#undef FB_HAVE_TLS
#endif



extern const ssize_t FB_TRANSPORT_CLOSED;
extern const ssize_t FB_TRANSPORT_INCOMPLETE;
extern const ssize_t FB_TRANSPORT_FAILURE;

/** Structure with various I/O functions */
typedef struct fb_config_files_t {
    char *public_key_name;
    char *x509_certificate_name;
    char *x509_revokation_name;
} FB_TLS_CONFIG_FILENAMES;

typedef struct fb_transport_funcs_t {
    /// One-time initialization on startup.  *Not* called per-connection.
    bool (*configure) (const FB_TLS_CONFIG_FILENAMES *files);
    /// One-time teardown before exit.  *Not* called per-connection.
    void (*cleanup) ();

    /// Initialize a new connection.  Return true on success, 0 on failure.
    bool (*init) (FB_CONNECTION *connection);
    /// Perform TLS handshaking on a new connection.  Return incomplete, failure, or 0.
    ssize_t (*handshake) (struct fb_connection_t *connection);
    /// Query number of bytes in TLS buffers.
    ssize_t (*buffering) (struct fb_connection_t *connection);
    /// Read data from a connection.  Return transport constant or bytes read.
    ssize_t (*read) (struct fb_connection_t *connection, char *data, ssize_t byte_count);
    /// Write data to a connection.  Return incomplete, failure, or bytes written (0 is not an error).
    ssize_t (*write) (struct fb_connection_t *connection, const char *data, ssize_t byte_count);
    /// Clean up a closing connection.
    void (*done) (FB_CONNECTION *connection);
} FB_TRANSPORT_FUNCS;

extern const FB_TRANSPORT_FUNCS fb_transport_unencrypted;
extern const FB_TRANSPORT_FUNCS fb_transport_read_file;
#ifdef FB_HAVE_TLS
extern const FB_TRANSPORT_FUNCS fb_transport_encrypted;
#endif


#endif /* defined(__football__transport__) */
