///
/// Football transport implementations for Secure Transport (OS X).
/// @file       fb_transport_osx.c - Football socket abstraction layer
/// @author     Perette Barella
/// @date       2016-03-11
/// @copyright  Copyright 2016-2017 Devious Fish. All rights reserved.
///

/**
 Procedure for creating a certificate and key for a Mac.
 This is based on OS X 10.9 Mavericks.
 
 Open KeyChain Access.
 Use "KeyChain Access -> Certificate Assistant -> Create a Certificate Authority..."
 to create a self-signed root CA.
 Use "KeyChain Access -> Certificate Assistant -> Create a Certificate..."
 to create a leaf for SSL server use.  Select the CA you previously created
 as the issuing authority.

 Note: The certificates  are good for a year by default.  If you enable custom
 options, you can adjust the longevity, but then there are a lot of other questions.
 
 Select the leaf/SSL server certificate you just made, and "File -> Export Items..."
 Save the file in your home directory as Certificates.cer using Certificate
 file format.
 
 On the command line:
 openssl x509 -inform der -in ~/Certificates.cer -out ~/.config/pianod2/x509-server.pem

 Start pianod.  On the command line again:
 openssl s_client -host localhost -port 4447
 
 An authorization dialog should pop up asking if pianod is allowed to access
 your keychain item.  Select "Allow Always".
*/

#include <config.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "fb_public.h"
#include "fb_transport.h"
#include "fb_service.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecIdentity.h>
#include <Security/SecureTransport.h>


static bool fb_tls_initialized;
CFArrayRef cert_array;

#define SSLHasClosed(status) (((status) == errSSLClosedAbort || (status) == errSSLClosedGraceful || (status) == errSSLClosedNoNotify))



/** Decode 4 bytes of base-64 encoded data into 3 bytes of binary data.
    @param in Four characters.
    @param out 1-3 bytes of decoded data.
    @return 0 on failure, or number of binary bytes produced. */
static int decodeblock(char *in, unsigned char *out) {
    static const char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const char *ch [4];
    for (int i = 0; i <= 3; i++) {
        ch [i] = strchr (cb64, in [i]);
    }
    if (!ch [0] || !ch [1] || (!ch [2] && ch[3]))
        return 0;
    out [0] = ((ch [0] - cb64) << 2) | ((ch [1] - cb64) >> 4);
    if (!ch [2])
        return 1;
    out [1] = (((ch [1] - cb64) & 0xf) << 4) | ((ch [2] - cb64) >> 2);
    if (!ch [3])
        return 2;
    out [2] = (((ch [2] - cb64) & 0x3) << 6) | (ch [3] - cb64);
    return 3;
}

/** Decode some base-64 encoded data.
    @param data The start of the data.  Whitespace in data is ignored.
    @param end_data The end of the data.
    @param length The length of the decoded binary data.
    @return A pointer to malloc'ed data blocked, or nullptr on error.
    @warning The returned data block must be freed. */
char *fb_base64_decode (const char *data, const char *end_data, int *length) {
    *length = 0;
    long possible_length = end_data - data;
    assert (possible_length > 0);
    unsigned char *result = malloc (possible_length);
    if (result) {
        char bytes [4];
        int have = 0;
        unsigned char *out = result;
        while (data < end_data) {
            if (!isspace (*data)) {
                bytes [have++] = *data;
            }
            data++;
            if (have == 4) {
                int l = decodeblock (bytes, out);
                if (!l) {
                    break;
                }
                have = 0;
                out += l;
            }
        }
        *length = (int) (out - result);
        if (have != 0) {
            free (result);
            result = NULL;
        }
    }
    assert (*length <= possible_length);
    return (char *) result;
}


/** Extract a base 64 encoded, delimited data block from a PEM formatted file.
    @param filename The file.
    @param object_type The type of block to extract.
    @param length The length of the resulting data block.
    @return A pointer to the extracted data blocked, or nullptr on error.
    @warning The returned data block must be freed. */
char *load_pem_thing (const char *filename,
                            const char *object_type,
                            int *length) {
    char *begin, *end, *result = NULL;
    if ((asprintf (&begin, "-----BEGIN %s-----", object_type) < 0)) {
        fb_perror("asprintf");
        begin = NULL;
    }
    if ((asprintf (&end, "-----END %s-----", object_type) < 0)) {
        fb_perror("asprintf");
        end = NULL;
    }

    FILE *in = fopen (filename, "r");
    if (begin && end && in) {
        if (fseek (in, 0, SEEK_END) >= 0) {
            long size = ftell (in);
            if (size > 0 && (fseek (in, 0, SEEK_SET) >= 0)) {
                char *buffer = malloc (size + 1);
                if (buffer && fread (buffer, 1, size, in) == size) {
                    buffer [size] = '\0';
                    char *start = strstr (buffer, begin);
                    if (start) {
                        start += strlen (begin);
                        char *stop = strstr (start, end);
                        if (stop) {
                            result = fb_base64_decode (start, stop, length);
                        }
                    }
                }
                free (buffer);
            }
        }
    }
    free (begin);
    free (end);
    return result;
}

/// @internal Initialize state, load certificates and keys.
bool fb_securetransport_configure (const FB_TLS_CONFIG_FILENAMES *paths) {
    assert (paths);
    assert (!fb_tls_initialized);

    int cert_length;
    char *certificate = load_pem_thing (paths->x509_certificate_name, "CERTIFICATE", &cert_length);
    if (certificate) {
        CFDataRef cert_ref = CFDataCreate (kCFAllocatorDefault, (const UInt8 *) certificate, cert_length);
        if (cert_ref) {
            SecCertificateRef tls_certificate = SecCertificateCreateWithData (NULL, cert_ref);
            if (tls_certificate) {
                // A SecIdentityRef object contains a SecKeyRef object and an associated SecCertificateRef object.
                // I.e., it contains the certificate-key pair.
                SecIdentityRef secure_identity;
                OSStatus result = SecIdentityCreateWithCertificate (NULL, tls_certificate, &secure_identity);
                if (result == 0) {
                    // Need to create public key
                    cert_array = CFArrayCreate (kCFAllocatorDefault, (const void **) &secure_identity, 1, NULL);
                    if (cert_array) {
                        fb_tls_initialized = true;
                    }
                }
            }
        }
        CFRelease (cert_ref);
    }
    free (certificate);
    return fb_tls_initialized;
}

/** @internal Read callback function conforming to SecureTransport requirements. */
static OSStatus fb_SSLReadFunc (SSLConnectionRef connection, void *data, size_t *dataLength) {
    ssize_t status = fb_transport_unencrypted.read ((FB_CONNECTION *) connection, data, *dataLength);
    *dataLength = (status >= 0 ? status : 0);
    return (status > 0 ? 0 :
            status == FB_TRANSPORT_INCOMPLETE ? errSSLWouldBlock :
            status == FB_TRANSPORT_CLOSED ? errSSLClosedGraceful : errSSLFatalAlert);
}

/** @internal Write callback function conforming to SecureTransport requirements. */
OSStatus fb_SSLWriteFunc ( SSLConnectionRef connection, const void *data, size_t *dataLength ) {
    ssize_t status = fb_transport_unencrypted.write ((FB_CONNECTION *) connection, data, *dataLength);
    *dataLength = (status >= 0 ? status : 0);
    return (status > 0 ? 0 :
            status == 0 ? errSSLWouldBlock :
            status == FB_TRANSPORT_INCOMPLETE ? errSSLWouldBlock : errSSLFatalAlert);
}


/** @internal
 Initialize the TLS stuff for a new connection.
 @param connection The connection to initialize.
 @return true on success, false on error. */
bool fb_securetransport_init (FB_CONNECTION *connection) {
    assert (fb_tls_initialized);
    if (!fb_tls_initialized) {
        fb_log (FB_WHERE (FB_LOG_ERROR), "TLS credentials not set.  Call fb_init_tls_support().");
        return false;
    }

    connection->tls = SSLCreateContext (kCFAllocatorDefault, kSSLServerSide, kSSLStreamType);
    if (!connection->tls) {
        return false;
    }

    char *func;
    OSStatus status = SSLSetCertificate (connection->tls, cert_array);
    if (status == 0) {
        status = SSLSetConnection (connection->tls, (SSLConnectionRef) connection);
        if (status == 0) {
            status = SSLSetIOFuncs (connection->tls, fb_SSLReadFunc, fb_SSLWriteFunc);
            if (status == 0) {
                fb_tls_initialized = true;
                return true;
            } else {
                func = "SSLSetIOFuncs";
            }
        } else {
            func = "SSLSetConnection";
        }
    } else {
        func = "SSLSetCertificate";
    }
    fb_log (FB_WHERE (FB_LOG_TLS_ERROR), "#%d: %s: Error %d", connection->socket, func, (int) status);

    return false;
}


/// Perform TLS handshaking on a new connection.  Return incomplete, failure, or 0.
ssize_t fb_securetransport_handshake (struct fb_connection_t *connection) {
    OSStatus status = SSLHandshake (connection->tls);

    if (status == errSSLWouldBlock) {
        return FB_TRANSPORT_INCOMPLETE;
    }
    if (status != 0) {
        fb_log (FB_WHERE (FB_LOG_TLS_ERROR), "#%d: SSLHandshake: Error %d", connection->socket, (int) status);
        return FB_TRANSPORT_FAILURE;
    }
    return 0;
}



/// Query number of bytes in TLS buffers.
ssize_t fb_securetransport_buffering (struct fb_connection_t *connection) {
    size_t bytes;
    OSStatus status = SSLGetBufferedReadSize (connection->tls, &bytes);
    return (status == 0 ? bytes : 1);
}


/// @internal Read data from a TLS connection using OpenSSL.
ssize_t fb_securetransport_read (struct fb_connection_t *connection, char *data, ssize_t byte_count) {
    assert (connection);
    assert (data);
    assert (byte_count >= 0);

    size_t bytes_read;
    OSStatus status = SSLRead (connection->tls, data, byte_count, &bytes_read);
    if (bytes_read > 0)
        return bytes_read;
    if (status == errSSLWouldBlock) {
        return FB_TRANSPORT_INCOMPLETE;
    }
    if (SSLHasClosed(status)) {
        return FB_TRANSPORT_CLOSED;
    }
    fb_log (FB_WHERE (FB_LOG_TLS_ERROR), "#%d: SSLRead: Error %d", connection->socket, (int) status);
    return FB_TRANSPORT_FAILURE;
}

/// @internal Write data to a TLS connection using OpenSSL.
ssize_t fb_securetransport_write (struct fb_connection_t *connection, const char *data, ssize_t byte_count) {
    assert (connection);
    assert (data);
    assert (byte_count >= 0);

    size_t written;
    OSStatus status = SSLWrite (connection->tls, data, byte_count, &written);
    if (written > 0)
        return written;
    if (status == errSSLWouldBlock) {
        return 0;
    }
    if (SSLHasClosed(status)) {
        return FB_TRANSPORT_CLOSED;
    }
    fb_log (FB_WHERE (FB_LOG_TLS_ERROR), "#%d: SSLWrite: Error %d", connection->socket, (int) status);
    return FB_TRANSPORT_FAILURE;
}

void fb_securetransport_done (FB_CONNECTION *connection) {
    SSLClose(connection->tls);
    CFRelease(connection->tls);
}

void fb_securetransport_cleanup () {

}


const FB_TRANSPORT_FUNCS fb_transport_encrypted = {
    .configure = fb_securetransport_configure,
    .cleanup = fb_securetransport_cleanup,
    .init = fb_securetransport_init,
    .handshake = fb_securetransport_handshake,
    .buffering = fb_securetransport_buffering,
    .read = fb_securetransport_read,
    .write = fb_securetransport_write,
    .done = fb_securetransport_done
};
