///
/// Football HTTP/Websocket functions.
/// @file       fb_http.c - Football socket abstraction layer
/// @author     Perette Barella
/// @date       2014-04-08
/// @copyright  Copyright 2014-2021 Devious Fish. All rights reserved.
///

/* Paste this (including blank line) into a telnet/nc session for testing:
GET /pianod HTTP/1.1
Host: bogus.data
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==
Sec-WebSocket-Version: 13

 */

#include <config.h>

#ifndef __FreeBSD__
#define _POSIX_C_SOURCE 200112L /* fileno() */
#define _BSD_SOURCE /* strdup(), realpath() */
#define _DARWIN_C_SOURCE /* strdup() on OS X */
#define _DEFAULT_SOURCE
#endif
#ifdef __linux__
#include <linux/posix_types.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <assert.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>

#include "fb_public.h"
#include "fb_service.h"
#include "sha1.h"

#define countof(x) (sizeof (x) / sizeof (*x))

typedef enum ws_opcodes_t {
	WSOC_CONTINUATION = 0x00,
	WSOC_TEXT   = 0x01,
	WSOC_BINARY = 0x02,
	WSOC_CLOSE  = 0x08,
	WSOC_PING   = 0x09,
	WSOC_PONG   = 0x0a,
	WSOC_MASK	= 0x0f
} OPCODE;

static const unsigned int WS_OPCODE = 0;
static const unsigned int WS_PAYLOAD = 1;
// static const unsigned int WS_PAYLOAD_16BIT = 2;
// static const unsigned int WS_PAYLOAD_64BIT = 2;
/* 2 bytes opcode/size, optional 8 byte size, optional 4 bytes masking
   = 14 really, but paranoia so 32 */
static const unsigned int WS_HEADER_MAXIMUM = 32;

static const unsigned char WS_FIN = 0x80;
static const unsigned char WS_MASK = 0x80;
static const unsigned char WS_PAYLOAD_MASK = 0x7f;
static const unsigned int WS_PAYLOAD_MAX_8BIT = 125;
static const unsigned int WS_PAYLOAD_MAGIC_16BIT = 126;
static const unsigned int WS_PAYLOAD_MAGIC_64BIT = 127;
static const unsigned int WS_PAYLOAD_MAX_16BIT = 0xffff;

// static const unsigned int WS_ERROR_PROTO = 1002;

#define WEBSOCKET_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WEBSOCKET_VERSION "13"
#define HTTP_VERSION "HTTP/1.1"

/* ------------------- Basic input & output and HTTP ------------------ */
/** @internal
    Same as fb_queue_http, but does not flush output after queueing. */
static bool fb_queue_http_noflush (FB_CONNECTION *connection, char *message, size_t length) {
    FB_MESSAGE *output = fb_messagealloc ();
    if (output) {
        output->message = (char *)message;
        output->length = length;
        if (fb_queue_add (&connection->out, output)) {
            return true;
        }
        fb_messagefree (output); /* Frees message with it */
    } else {
        free (message);
    }
    return false;
}

/** @internal
    Add a message to the output queue.
    The message must be dynamically allocated by caller and is added to the
    connections queue.  On failure. it is freed by this function.
    @param connection the destination of the message
    @param message the message body, dynamically allocated by the caller.
    @param length the length of message.
    @return true on success, false on failure. */
inline static bool fb_queue_http (FB_CONNECTION *connection, char *message, size_t length) {
    bool ok = fb_queue_http_noflush(connection, message, length);
    if (ok) {
        fb_send_output (NULL, connection);
    }
    return ok;
}

/** @internal
    Add a message to the output queue, dynamically allocating the message.
    @param connection Where to send the message.
    @param message The message to send.
    @param length The length of message. 
    @return true on success, false on failure. */
static bool fb_queue_http_alloc (FB_CONNECTION *connection, const char *message, size_t length) {
    char *m = malloc (length);
    if (m) {
        memcpy(m, message, length);
        return fb_queue_http (connection, m, length);
    }
    fb_perror ("malloc");
    return false;
}

/** @internal
    Send some raw text to a connection.
    @param connection Where to send the message.
    @param message The message to send.
 */
static bool write_message (FB_CONNECTION *connection, const char *message) {
	return fb_queue_http_alloc (connection, message, strlen (message));
}

/** @internal
    Send an HTTP response header to a client.  Closes the connection if anything goes wrong.
    @param connection Where to send the header.
    @param message The HTTP response code/text.
    @param extra_header Extra line(s) to include with the header, or NULL.
    @param detail Text to provide to the user, or NULL to go with the default.
    @return true on success, false on failure.
 */
static bool http_header (FB_CONNECTION *connection, const char *message, const char *extra_header, const char *detail) {
    fb_log (FB_WHERE (FB_LOG_HTTP_TRAFFIC), "#%d: Response: %s", connection->socket, message);

    char *body;
    int body_length = asprintf (&body,
                                "<!doctype html>\r\n"
                                "<html><head>\r\n"
                                "<title>%s</title>\r\n"
                                "</head><body>\r\n"
                                "<h1>%s</h1>\r\n"
                                "<p>%s</p>\r\n"
                                "</body></html>\r\n", message, message, detail ? detail : "See Figure 1.");
    if (body_length >= 0) {
        /* Date formatted per RFC2616 sec 3.3.1: Sun, 06 Nov 1994 08:49:37 GMT */
        char date [30];
        struct tm modtime;
        time_t now = time (NULL);;
        gmtime_r (&now, &modtime);
        strftime (date, sizeof (date), "%a, %d %b %Y %H:%M:%S GMT", &modtime);

        char *header;
        int length = asprintf (&header,
                               HTTP_VERSION " %s\r\n"
                               "Date: %s\r\n"
                               "Content-length: %d\r\n"
                               "Content-type: text/html; charset=utf-8\r\n"
                               "Server: pianod-" PACKAGE_VERSION "\r\n"
                               "%s%s"
                               "\r\n%s",
                               message, date, body_length,
                               extra_header ? extra_header : "", extra_header ? "\r\n" : "",
                               body);
        free (body);
        if (length >= 0) {
            if (fb_queue_http (connection, header, length)) {
                return true;
            };
        } else {
            fb_perror("asprintf");
        }
    } else {
        fb_perror ("asprintf");
    }
    fb_close_connection(connection);
    return false;
}

/** @internal
    Create a properly formatted HTTP response header.
    @param connection Where to send the response.
    @param message The HTTP response code/text.
    @return True on success, false on failure. */
static bool http_response (FB_CONNECTION *connection, const char *message) {
    return http_header (connection, message, NULL, NULL);
}

/** @internal
    Create a properly formatted HTTP redirect response.
    @param connection Where to send the response.
    @param location The URL to redirect to.
    @return True on success, false on failure. */
static bool http_redirect (FB_CONNECTION *connection, const char *location) {
    char *extra_header;
    char *detail;
    bool result = true;
    if ((asprintf (&extra_header, "Location: %s", location) < 0)) {
        extra_header = NULL;
        fb_perror ("asprintf");
        result = false;
    }
    if ((asprintf (&detail, "Try <a href='%s'>here</a>.", location) < 0)) {
        detail = NULL;
        fb_perror("asprinf");
        result = false;
    }
    result = http_header (connection, "301 Permanently moved", extra_header, detail) && result;
    free (extra_header);
    free (detail);
    return result;
}



/*
 encodeblock snagged from http://base64.sourceforge.net/b64.c:

 LICENCE:        Copyright (c) 2001 Bob Trower, Trantor Standard Systems Inc.

 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated
 documentation files (the "Software"), to deal in the
 Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall
 be included in all copies or substantial portions of the
 Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
 OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 *
 ** encodeblock
 **
 ** encode 3 8-bit binary bytes as 4 '6-bit' characters
 */
/* Translation Table as described in RFC1113 */
static const char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void encodeblock( unsigned char *in, unsigned char *out, int len )
{
    out[0] = (unsigned char) cb64[ (int)(in[0] >> 2) ];
    out[1] = (unsigned char) cb64[ (int)(((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4)) ];
    out[2] = (unsigned char) (len > 1 ? cb64[ (int)(((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6)) ] : '=');
    out[3] = (unsigned char) (len > 2 ? cb64[ (int)(in[2] & 0x3f) ] : '=');
}

/*

 Fette & Melnikov             Standards Track                   [Page 29]

 RFC 6455                 The WebSocket Protocol            December 2011


 5.2.  Base Framing Protocol

 This wire format for the data transfer part is described by the ABNF
 [RFC5234] given in detail in this section.  (Note that, unlike in
 other sections of this document, the ABNF in this section is
 operating on groups of bits.  The length of each group of bits is
 indicated in a comment.  When encoded on the wire, the most
 significant bit is the leftmost in the ABNF).  A high-level overview
 of the framing is given in the following figure.  In a case of
 conflict between the figure below and the ABNF specified later in
 this section, the figure is authoritative.

 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-------+-+-------------+-------------------------------+
 |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 | |1|2|3|       |K|             |                               |
 +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 |     Extended payload length continued, if payload len == 127  |
 + - - - - - - - - - - - - - - - +-------------------------------+
 |                               |Masking-key, if MASK set to 1  |
 +-------------------------------+-------------------------------+
 | Masking-key (continued)       |          Payload Data         |
 +-------------------------------- - - - - - - - - - - - - - - - +
 :                     Payload Data continued ...                :
 + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 |                     Payload Data continued ...                |
 +---------------------------------------------------------------+

 FIN:  1 bit

 Indicates that this is the final fragment in a message.  The first
 fragment MAY also be the final fragment.

 RSV1, RSV2, RSV3:  1 bit each

 MUST be 0 unless an extension is negotiated that defines meanings
 for non-zero values.  If a nonzero value is received and none of
 the negotiated extensions defines the meaning of such a nonzero
 value, the receiving endpoint MUST _Fail the WebSocket
 Connection_.

 Opcode:  4 bits

 Defines the interpretation of the "Payload data".  If an unknown
 opcode is received, the receiving endpoint MUST _Fail the
 WebSocket Connection_.  The following values are defined.

 *  %x0 denotes a continuation frame
 *  %x1 denotes a text frame
 *  %x2 denotes a binary frame
 *  %x3-7 are reserved for further non-control frames
 *  %x8 denotes a connection close
 *  %x9 denotes a ping
 *  %xA denotes a pong
 *  %xB-F are reserved for further control frames

 Mask:  1 bit

 Defines whether the "Payload data" is masked.  If set to 1, a
 masking key is present in masking-key, and this is used to unmask
 the "Payload data" as per Section 5.3.  All frames sent from
 client to server have this bit set to 1.

 Payload length:  7 bits, 7+16 bits, or 7+64 bits

 The length of the "Payload data", in bytes: if 0-125, that is the
 payload length.  If 126, the following 2 bytes interpreted as a
 16-bit unsigned integer are the payload length.  If 127, the
 following 8 bytes interpreted as a 64-bit unsigned integer (the
 most significant bit MUST be 0) are the payload length.  Multibyte
 length quantities are expressed in network byte order.  Note that
 in all cases, the minimal number of bytes MUST be used to encode
 the length, for example, the length of a 124-byte-long string
 can't be encoded as the sequence 126, 0, 124.  The payload length
 is the length of the "Extension data" + the length of the
 "Application data".  The length of the "Extension data" may be
 zero, in which case the payload length is the length of the
 "Application data".

 Masking-key:  0 or 4 bytes

 All frames sent from the client to the server are masked by a
 32-bit value that is contained within the frame.  This field is
 present if the mask bit is set to 1 and is absent if the mask bit
 is set to 0.  See Section 5.3 for further information on client-
 to-server masking.

 Payload data:  (x+y) bytes

 The "Payload data" is defined as "Extension data" concatenated
 with "Application data".

 Extension data:  x bytes

 The "Extension data" is 0 bytes unless an extension has been
 negotiated.  Any extension MUST specify the length of the
 "Extension data", or how that length may be calculated, and how
 the extension use MUST be negotiated during the opening handshake.
 If present, the "Extension data" is included in the total payload
 length.

 Application data:  y bytes

 Arbitrary "Application data", taking up the remainder of the frame
 after any "Extension data".  The length of the "Application data"
 is equal to the payload length minus the length of the "Extension
 data".

 The base framing protocol is formally defined by the following ABNF
 [RFC5234].  It is important to note that the representation of this
 data is binary, not ASCII characters.  As such, a field with a length
 of 1 bit that takes values %x0 / %x1 is represented as a single bit
 whose value is 0 or 1, not a full byte (octet) that stands for the
 characters "0" or "1" in the ASCII encoding.  A field with a length
 of 4 bits with values between %x0-F again is represented by 4 bits,
 again NOT by an ASCII character or full byte (octet) with these
 values.  [RFC5234] does not specify a character encoding: "Rules
 resolve into a string of terminal values, sometimes called
 characters.  In ABNF, a character is merely a non-negative integer.
 In certain contexts, a specific mapping (encoding) of values into a
 character set (such as ASCII) will be specified."  Here, the
 specified encoding is a binary encoding where each terminal value is
 encoded in the specified number of bits, which varies for each field.

 ws-frame                = frame-fin           ; 1 bit in length
 frame-rsv1          ; 1 bit in length
 frame-rsv2          ; 1 bit in length
 frame-rsv3          ; 1 bit in length
 frame-opcode        ; 4 bits in length
 frame-masked        ; 1 bit in length
 frame-payload-length   ; either 7, 7+16,
 ; or 7+64 bits in
 ; length
 [ frame-masking-key ]  ; 32 bits in length
 frame-payload-data     ; n*8 bits in
 ; length, where
 ; n >= 0

 frame-fin               = %x0 ; more frames of this message follow
 / %x1 ; final frame of this message
 ; 1 bit in length

 frame-rsv1              = %x0 / %x1
 ; 1 bit in length, MUST be 0 unless
 ; negotiated otherwise

 frame-rsv2              = %x0 / %x1
 ; 1 bit in length, MUST be 0 unless
 ; negotiated otherwise

 frame-rsv3              = %x0 / %x1
 ; 1 bit in length, MUST be 0 unless
 ; negotiated otherwise

 frame-opcode            = frame-opcode-non-control /
 frame-opcode-control /
 frame-opcode-cont

 frame-opcode-cont       = %x0 ; frame continuation

 frame-opcode-non-control= %x1 ; text frame
 / %x2 ; binary frame
 / %x3-7
 ; 4 bits in length,
 ; reserved for further non-control frames

 frame-opcode-control    = %x8 ; connection close
 / %x9 ; ping
 / %xA ; pong
 / %xB-F ; reserved for further control
 ; frames
 ; 4 bits in length

 frame-masked            = %x0
 ; frame is not masked, no frame-masking-key
 / %x1
 ; frame is masked, frame-masking-key present
 ; 1 bit in length

 frame-payload-length    = ( %x00-7D )
 / ( %x7E frame-payload-length-16 )
 / ( %x7F frame-payload-length-63 )
 ; 7, 7+16, or 7+64 bits in length,
 ; respectively

 frame-payload-length-16 = %x0000-FFFF ; 16 bits in length

 frame-payload-length-63 = %x0000000000000000-7FFFFFFFFFFFFFFF
 ; 64 bits in length

 frame-masking-key       = 4( %x00-FF )
 ; present only if frame-masked is 1
 ; 32 bits in length

 frame-payload-data      = (frame-masked-extension-data
 frame-masked-application-data)
 ; when frame-masked is 1
 / (frame-unmasked-extension-data
 frame-unmasked-application-data)
 ; when frame-masked is 0

 frame-masked-extension-data     = *( %x00-FF )
 ; reserved for future extensibility
 ; n*8 bits in length, where n >= 0

 frame-masked-application-data   = *( %x00-FF )
 ; n*8 bits in length, where n >= 0

 frame-unmasked-extension-data   = *( %x00-FF )
 ; reserved for future extensibility
 ; n*8 bits in length, where n >= 0

 frame-unmasked-application-data = *( %x00-FF )
 ; n*8 bits in length, where n >= 0

 */

/* ------------------- Websocket input & output functions ------------------ */

/** @internal
    Mask or umask a websocket packet.
    @param message the message to be masked/unmasked.
    @param length the length of the message.
    @param mask Pointer to a 4 bytes containing mask . */
static void perform_unmask (unsigned char *message, const unsigned long length, const unsigned char *mask)
{
	for (unsigned long i = 0; i < length; i++) {
		message [i] = message [i] ^ mask [i % 4];
	}
}

/** @internal
    Ensure that input buffer contains required number of bytes.
    Read more input if necessary.
    @param connection the connection read from.
    @param message_size The amount of data we need to collect.
    @return true when the buffering requirements are met. */
static bool fb_get_http_bytes (FB_CONNECTION *connection, size_t message_size) {
    assert (connection);
    assert (message_size >= connection->in.size); /* Make sure we don't try to unread anything */
    if (connection->in.size == message_size) {
        return true;
    }
    if (connection->in.capacity < message_size) {
        if (!fb_set_input_buffer_size (connection, message_size + 128)) {
            return false;
        }
    }
    return fb_recv_input (connection, message_size - connection->in.size);
}


/** Reset the next-ping interval. */
void reset_ping_interval (FB_CONNECTION *connection) {
    time_t interval = connection->service->options.websocket_ping_interval;
    connection->next_ping = (interval ? time (NULL) + interval : FAR_FUTURE);
}


/** @internal
    Read data from WebSocket connection.
    Reads a WebSocket packet and decodes it, and returns an event for the payload.
    @param event a partially filled event applicable to the connection.
    @param connection the connection to read from.
    @return an FB_EVENT_INPUT for the message received, or NULL if the packet is incomplete. */
FB_EVENT *fb_read_websocket_input (FB_EVENT *event, FB_CONNECTION *connection) {
    /* We need at least 2 bytes to determine header size */
    if (!fb_get_http_bytes (connection, 2)) return NULL;

    unsigned char *buffer = (unsigned char *) connection->in.message;
    OPCODE opcode = buffer [WS_OPCODE] & WSOC_MASK;
    bool is_masked = buffer [WS_PAYLOAD] & WS_MASK;
    if (!is_masked) {
        fb_log (FB_WHERE (FB_LOG_HTTP_ERROR), "#%d: Received unmasked packet from %s.",
                connection->socket, fb_connection_info (connection));
        fb_close_connection (connection);
        return NULL;
    }
    uint32_t data_length = buffer [WS_PAYLOAD] & WS_PAYLOAD_MASK;
    unsigned length_size = (data_length == WS_PAYLOAD_MAGIC_16BIT) ? 2 :
                           (data_length == WS_PAYLOAD_MAGIC_64BIT) ? 8 : 0;
    size_t header_size = 2 + length_size + (is_masked ? 4 : 0);

    /* Get more, then recalculate pointers that may have changed from realloc */
    if (!fb_get_http_bytes (connection, header_size)) return NULL;
    buffer = (unsigned char *) connection->in.message;

    unsigned char *parse = buffer + 2;
    reset_ping_interval (connection);

    /* If there's more header, read and extract that */
    switch (length_size) {
        case 2:
            data_length = ntohs (*(uint16_t *) parse);
            parse += 2;
            break;
        case 8:
            if (*(uint32_t *) parse != 0) {
                fb_log (FB_WHERE (FB_LOG_HTTP_ERROR), "#%d: Websocket packet from %s exceeds 32-bit size.",
                        connection->socket, fb_connection_info (connection));
                return NULL;
            }
            parse += 4;
            data_length = ntohl (*(uint32_t *) parse);
            parse += 4;
            break;
        case 0:
            /* Already got it */
            break;
        default:
            assert (!"Switch/case unhandled.");
            return NULL;
    }

    /* Get more, then recalculate pointers that may have changed from realloc */
    if (!fb_get_http_bytes (connection, header_size + data_length)) return NULL;
    parse = (unsigned char *) connection->in.message + (parse - buffer);
    buffer = (unsigned char *) connection->in.message;
    connection->in.size = 0;

    const unsigned char *mask = parse;
    parse += 4;

    /* We have a message!  Do something with it. */
    switch (opcode) {
        case WSOC_CLOSE:
            fb_close_connection (connection);
            return NULL;
        case WSOC_PING:
            /* Change message type to PONG */
            buffer [WS_OPCODE] = WS_FIN | WSOC_PONG;
            /* Unmask the message */
            perform_unmask ((unsigned char *) parse, data_length, mask);
            /* Move original message 4 to the left. This effectively deletes the mask. */
            memmove (parse-4, parse, data_length);
            /* Delete the masking bit */
            buffer [WS_PAYLOAD] &= ~WS_MASK;
            /* Send PONG */
            fb_queue_http_alloc (connection, (char *) buffer, header_size + data_length - 4);
            fb_log (FB_WHERE (FB_LOG_HTTP_TRAFFIC), "#%d: replied to ping (size=%d)", connection->socket,
                    (int) (header_size + data_length));
            return NULL;
        case WSOC_PONG:
            fb_log (FB_WHERE (FB_LOG_HTTP_TRAFFIC), "#%d: got pong", connection->socket);
            return NULL;
        case WSOC_TEXT:
        case WSOC_BINARY:
            /* Unmask the message */
            perform_unmask ((unsigned char *) parse, data_length, mask);
            /* Extract the message. */

            event->command = malloc (data_length + 1);
            if (!event->command) {
                fb_perror ("malloc");
                return NULL;
            }
            memcpy (event->command, parse, data_length);
            *(event->command + data_length) = '\0';
            event->argc = fb_create_argv(event->command, &event->argv);
            if (event->argc < 0) {
                event->argc = 0;
            }
            event->type = FB_EVENT_INPUT;
            return (event);
        default:
            fb_log (FB_WHERE (FB_LOG_HTTP_ERROR), "#%d: Unknown opcode 0x%02x from %s",
                    connection->socket, opcode, fb_connection_info (connection));
            fb_close_connection (connection);
            return NULL;
    }
    assert (!"Reached end of function.");
}



/** @internal
    Send a WebSocket ping packet.
    Some browsers have timeouts, so we need to send pings as keep-alives.
    @param connection The connection to check.
    @return true on success, false on failure. */
bool fb_websocket_ping (FB_CONNECTION *connection) {
    unsigned char ping [2];
    ping [0] = WS_FIN | WSOC_PING;
    ping [1] = 0;
    reset_ping_interval (connection);
    return fb_queue_http_alloc (connection, (char *) ping, sizeof (ping));
}


/** @internal
    Send a WebSocket close packet.
    This should be called as last thing prior to closing the connection.
    @param connection The connection to write the close to.
    @return true on success, false on failure. */
bool fb_websocket_announce_close (FB_CONNECTION *connection) {
    unsigned char close_msg [2];
    close_msg [0] = WS_FIN | WSOC_CLOSE;
    close_msg [1] = 0;
    return (write (connection->socket, (char *) close_msg, sizeof (close_msg)) == sizeof (close_msg));
}


/** @internal
    Build WebSocket packets from output.
    Checks if there's a complete packet (line) in the assembly queue.
    When there is, assemble a WebSocket packet and put it in the output queue.
    @param connection The connection to check.
    @return true on success, false on failure. */
bool fb_websocket_encode (FB_CONNECTION *connection) {
    assert (connection);
    FB_IOQUEUE *ass = &connection->assembly;
    assert (ass->first);
    assert (ass->last);

    /* See if there's a newline ready in the last block of the output assembly queue. */
    /* If it's there, there's work to do; if not, we're done. */
    FB_MESSAGE *last = ass->last->message;
    const char *start = last->message + (ass->first == ass->last ? ass->consumed : 0);
    const char *newline = strchr (start, '\n');
    if (!newline) {
        return true;
    }

    /* Calculate the size of the message to be sent. */
    size_t message_size = newline - last->message;
    for (FB_MESSAGELIST *m = ass->first; m != ass->last; m = m->next) {
        message_size += m->message->length;
    }
    message_size -= ass->consumed;

    unsigned char *message = malloc (message_size + WS_HEADER_MAXIMUM);
    if (!message) {
        return false;
    }

    /* Construct the header */
    unsigned char *header = (unsigned char *) message;
    size_t header_size = 2;

    const bool fin = true; /* We never fragment headers currently. */
    header [WS_OPCODE] = (fin ? WS_FIN : 0) | WSOC_TEXT;

    unsigned char length_byte;
    if (message_size <= WS_PAYLOAD_MAX_8BIT) {
        length_byte = message_size;
    } else if (message_size <= WS_PAYLOAD_MAX_16BIT) {
        length_byte = WS_PAYLOAD_MAGIC_16BIT;
        *(uint16_t *) (header + header_size) = htons (message_size);
        header_size += 2;
    } else {
        length_byte = WS_PAYLOAD_MAGIC_64BIT;
        *(uint32_t *) (header + header_size) = 0;
        *(uint32_t *) (header + header_size + 4) = htonl ((uint32_t) message_size);
        header_size += 8;
    }
    header [WS_PAYLOAD] = length_byte;

    /* Assemble the message. */
    unsigned char *msg = header + header_size;

    /* Assemble the pieces. */
    long chunk_size;
    long message_left;
    for (message_left = message_size; message_left > 0; message_left -= chunk_size) {
        assert (ass->first);
        chunk_size = ass->first->message->length - ass->consumed;
        if (chunk_size > message_left) {
            chunk_size = message_left;
        }
        memcpy (msg, ass->first->message->message + ass->consumed, chunk_size);
        msg += chunk_size;
        fb_queue_consume (ass, chunk_size);
    }

    /* Skip the newline */
    fb_queue_consume (ass, 1);

    /* Queue it. */
    if (fb_queue_http_noflush (connection, (char *) message, header_size + message_size)) {
        /* Recurse in case there were 2 newlines in this block. */
        if (!fb_queue_empty (&connection->assembly)) {
            return fb_websocket_encode (connection);
        }
        reset_ping_interval (connection);
        return true;
    }
    return false;
}

/** @internal
    Respond to the HTTP WebSocket request with the connection upgrade response.
    This requires crafting a custom SHA1 key to confirm we're really speaking WebSocket.
    @param connection The connection to greet.
    @return true on success, false on failure. */
static bool fb_greet_websocket (FB_CONNECTION *connection) {
    FB_HTTPREQUEST *request = &connection->request;
    assert (request->websocket_key);

    fb_log (FB_WHERE (FB_LOG_HTTP_TRAFFIC),
            "#%d: %s WebSocket session initiated", connection->socket,
            connection->service->options.name ? connection->service->options.name : "Unnamed service");

	ssize_t encode_length = strlen (request->websocket_key) + strlen (WEBSOCKET_GUID);
	char *buffer = malloc (encode_length + 1);
	uint8_t accept_code [SHA1HashSize + 3];
	unsigned char code_as_text [SHA1HashSize * 2 + 7];
	memset (accept_code, 0, sizeof (accept_code));
	memset (code_as_text, 0, sizeof (code_as_text));
	bool okay;

	if (buffer) {
		/* Create the string and generate SHA1 hash code from it */
		SHA1Context chaka_kahn;
		strcat (strcpy (buffer, request->websocket_key), WEBSOCKET_GUID);

		okay = (SHA1Reset (&chaka_kahn) == shaSuccess &&
				SHA1Input (&chaka_kahn, (uint8_t *) buffer, (int32_t) strlen (buffer)) == shaSuccess &&
				SHA1Result (&chaka_kahn, accept_code) == shaSuccess);
		free (buffer);
	}
	if (!buffer || !okay) {
		return false;
	}
	/* Base 64 encode the SHA1 hash string */
	for (int i = 0, j=0; i < SHA1HashSize; i+=3, j+=4) {
		encodeblock (&accept_code [i], &code_as_text [j], SHA1HashSize - i >= 3 ? 3 : SHA1HashSize - i);
	}
	bool ok = (write_message (connection, "HTTP/1.1 101 Switching Protocols\r\n"
                              "Upgrade: websocket\r\n"
                              "Connection: Upgrade\r\n"
                              "Sec-WebSocket-Accept: ") &&
               write_message (connection, (char *) code_as_text) &&
               write_message (connection, "\r\n"));
	if (ok && request->websocket_protocol) {
		ok = (write_message (connection, "Sec-WebSocket-Protocol: ") &&
              write_message (connection, request->websocket_protocol) &&
              write_message (connection, "\r\n"));
	}
	ok = ok && write_message (connection, "\r\n");
    reset_ping_interval (connection);
	return ok;
}



/* ---------------- Non-WebSocket HTTP request handling -------------- */


/** @internal
    Reset a request structure.
    Frees dynamically allocated elements and clears structure.
    @param request The request structure to reset. */
void fb_destroy_httprequest (FB_HTTPREQUEST *request) {
    assert (request);
    free (request->http);
    free (request->host);
    free (request->service_name);
    // free (request->filename); Built from chopped-up service_name; don't free!
    free (request->upgrade_type);
    free (request->websocket_key);
    free (request->websocket_protocol);
    free (request->websocket_version);
    free (request->if_modified_since);
    free (request->language);
    memset (request, 0, sizeof (*request));
}


/** @internal
    Redirect to a subdirectory.
    Given a request pointing to a directory (without trailing slash),
    request the contents (with slash).
    @param connection The client to redirect.
    @return True on success, false on failure. */
static bool redirect_to_subdirectory (FB_CONNECTION *connection) {
    assert (connection);

    FB_HTTPREQUEST *request = &connection->request;
    char *destination;
    if ((asprintf (&destination, "http%s://%s/%s%s%s%s", connection->encrypted ? "s" : "",
                   request->host,
                   connection->service->options.name ? connection->service->options.name : "",
                   connection->service->options.name ? "/" : "",
                   request->filename ? request->filename : "",
                   request->filename ? "/" : "") >= 0)) {
        bool ok = http_redirect (connection, destination);
        free (destination);
        return ok;
    }
    fb_perror ("asprintf");
    http_response (connection, "500 Internal server error");
    return false;
}

#define get_hex_digit(c) (((c) >= '0' && (c) <= '9') ? (c) - '0' : \
                          ((c) >= 'a' && (c) <= 'f') ? (c) - '0' + 10 : \
                          (c) - 'A' + 10)
/** @internal
    Decode URL encoding.
    @param request the URL to decode.  Contents are overwritten with decoded URL.
    @return true if the URL decodes okay, false if it is encoded incorrectly. */
static bool url_decode (char *request) {
    char *src = request, *dest = request;
    while (*src) {
        if (*src == '%') {
            src++;
            if (isxdigit(*src) && isxdigit(*(src+1))) {
                *dest = get_hex_digit (*src) * 16 + get_hex_digit (*(src+1));
                if (*dest < 0x20) return false;
                src += 2;
            } else {
                return false;
            }
        } else {
            *(src++) = *(dest++);
        }
    }
    *dest = '\0';
    return true;
}

/** @internal
    Collect the relevant bits of 'get', 'head' or other HTTP method.
    @param event an event, complete with argv structure.
    @param request HTTP request structure into which details will be placed. */
void fb_collect_http_request (FB_EVENT *event, FB_HTTPREQUEST *request) {
    assert (event->argc >= 1);
    if (strcasecmp (event->argv [0], "head") == 0) {
        request->headonly = true;
    } else if (strcasecmp (event->argv [0], "get") != 0) {
        request->unsupported = true;
        return;
    }

    if (event->argc < 3) return;
    if (event->argv [1][0] != '/') return;
    request->http = strdup (event->argv [2]);
    request->service_name = strdup (event->argv [1] + 1);
    if (request->service_name) {
        /* HTTP parameters are not used; get rid of them */
        char *parameters = strchr (request->service_name, '?');
        if (parameters) {
            *parameters = '\0';
            request->query_string = parameters + 1;
        }
        if (event->connection->service->options.name) {
            /* If there's a filename after the service name, store it. */
            char *name_end = strchr (request->service_name, '/');
            if (name_end) {
                request->filename = name_end + 1;
                *name_end = '\0';
                request->invalid = request->invalid && url_decode (request->filename);
            }
        } else {
            /* Unnamed service, so the whole service name is the filename */
            request->filename = request->service_name;
        }
        request->invalid = request->invalid && url_decode (request->service_name);
    } else {
        fb_perror ("strdup");
        request->failure = true;
    }
}

/** @internal
    Store a value in the request structure, or consider it an error if it's duplicate.
    @param value The destination of the value.
    @param newvalue The value store.
    @param request The request structure.
    @return True on success, false on failure. */
static bool store (char **value, const char *newvalue, FB_HTTPREQUEST *request) {
    if (*value) {
        request->invalid = true;
    } else {
        *value = strdup (newvalue);
        if (!*value) {
            fb_perror ("strdup");
            return false;
        }
    }
    return true;
}

/** @internal
    Collect HTTP method parameters that are relevant.
    @param line the HTTP header line received.
    @param request the request structure into which details will be stored. */
void fb_collect_http_parameter (char *line, FB_HTTPREQUEST *request) {
    const char *name = line;
    char *value = strchr (line, ':');
    if (!value) {
        request->invalid = true;
        return;
    }
    *(value++) = '\0';
    while (isspace (*value)) {
        value++;
    }
    bool ok = true;
    if (strcmp (name, "Host") == 0) {
        ok = store (&request->host, value, request);
    } else if (strcmp (name, "Sec-WebSocket-Protocol") == 0) {
        ok = store (&request->websocket_protocol, value, request);
    } else if (strcmp (name, "Sec-WebSocket-Version") == 0) {
        ok = store (&request->websocket_version, value, request);
    } else if (strcmp (name, "Sec-WebSocket-Key") == 0) {
        ok = store (&request->websocket_key, value, request);
    } else if (strcmp (name, "Upgrade") == 0) {
        ok = store (&request->upgrade_type, value, request);
    } else if (strcmp (name, "If-Modified-Since") == 0) {
        ok = store (&request->if_modified_since, value, request);
    } else if (strcmp (name, "Accept-Language") == 0) {
        ok = store (&request->language, value, request);
    };
    if (!ok) {
        request->failure = true;
    }
}

/** @internal
    Determine if a request is an HTTP one.
    @param command the HTTP request (GET, HEAD, etc.)
    @return true if command is an HTTP request, false otherwise. */
bool fb_http_command (const char *command) {
    const char *cmds[] = {
        "GET",
        "HEAD",
        "POST",
        "OPTIONS",
        "PUT",
        "DELETE",
        "TRACE",
        "CONNECT"
    };
    for (size_t i = 0; i < countof (cmds); i++) {
        if (strcasecmp (command, cmds [i]) == 0) {
            return true;
        }
    }
    return false;
}

/** @internal Map file extensions to MIME types reported by HTTP server. */
typedef struct media_dictionary_t {
    char *extension;
    char *name;
} MEDIA_DICTIONARY;
/** @internal
    Guess the media type from a file's extension.
    @param filename the name of a file.
    @return a media type string corresponding to the file's extension,
            or the default "text/plain". */
static const char *get_media_type (const char *filename) {
    const MEDIA_DICTIONARY media_types[] = {
        { "jpg", "image/jpeg" },
        { "jpeg", "image/jpeg" },
        { "gif", "image/gif" },
        { "png", "image/png" },
        { "htm", "text/html" },
        { "html", "text/html" },
        { "txt", "text/plain" },
        { "js", "application/javascript" },
        { "jsm", "application/javascript" },
        { "pdf", "application/pdf" },
        { "css", "text/css" },
    };
    char *extension = NULL, *next;
    while ((next = strchr (extension ? extension + 1 : filename, '.'))) {
        extension = next;
    }
    if (extension) {
        extension++;
        for (size_t i = 0; i < countof (media_types); i++) {
            if (strcasecmp (media_types [i].extension, extension) == 0) {
                return media_types [i].name;
            }
        }
    }
    return "text/plain";
}


/** @internal
    Handle a complete and valid GET or HEAD request.
    @param connection the connection issuing the request.
    @param name the name of the file being served.
    @param file an open file handle for the file being served.
    @param sendbody true if the file should be served (GET request), false if not (HEAD) */
static bool http_serve_data (FB_CONNECTION *connection, char *name, FILE *file, bool sendbody) {
    struct stat info;
    char servedate [30], expiration [30], lastmodified [30];
    struct tm servetime, exptime, modtime;

    if (fstat (fileno (file), &info) == -1) {
        http_response (connection, "500 Internal server error");
        return false;
    }
    if (info.st_mode & S_IFDIR) {
        return redirect_to_subdirectory (connection);
    }

    /* Create the date strings for the header */
    /* Date formatted per RFC2616 sec 3.3.1: Sun, 06 Nov 1994 08:49:37 GMT */
    gmtime_r (&info.st_mtime, &modtime);
    strftime (lastmodified, sizeof (lastmodified), "%a, %d %b %Y %H:%M:%S GMT", &modtime);

    time_t when = time (NULL);
    gmtime_r (&when, &servetime);
    strftime (servedate, sizeof (servedate), "%a, %d %b %Y %H:%M:%S GMT", &servetime);

    when += 3600;
    gmtime_r (&when, &exptime);
    strftime (expiration, sizeof (expiration), "%a, %d %b %Y %H:%M:%S GMT", &exptime);

    /* Determine the status (200 Ok or 304 Not Modified if cached) */
    const char *status = "200 Ok";
    if (sendbody && connection->request.if_modified_since) {
        struct tm cachedtime;
        if (strptime(connection->request.if_modified_since,
                     "%a, %d %b %Y %H:%M:%S GMT", &cachedtime)) {
                if ((cachedtime.tm_year == modtime.tm_year) &&
                    (cachedtime.tm_mon == modtime.tm_mon) &&
                    (cachedtime.tm_mday == modtime.tm_mday) &&
                    (cachedtime.tm_hour == modtime.tm_hour) &&
                    (cachedtime.tm_min == modtime.tm_min) &&
                    (cachedtime.tm_sec == modtime.tm_sec)) {
                    status = "304 Not modified";
                    sendbody = false;
                }
        }
    }

    fb_log (FB_WHERE (FB_LOG_HTTP_TRAFFIC), "#%d: %s: HTTP request: %s %s (%s)", connection->socket,
            connection->service->options.name ? connection->service->options.name : "Unnamed service",
            sendbody ? "GET" : "HEAD", name, status);

    /* Send the header */
    char *header;
    int length = asprintf (&header,
            HTTP_VERSION " %s\r\nDate: %s\r\nExpires: %s\r\n"
                         "Last-Modified: %s\r\nContent-length: %u\r\n"
            "Content-type: %s\r\nServer: pianod-" PACKAGE_VERSION "\r\n\r\n",
            status, servedate, expiration, lastmodified, (unsigned int) info.st_size,
            get_media_type (name));
    if (length <= 0) {
        fb_perror ("asprintf");
        http_response (connection, "500 Internal server error");
        return false;
    }
    bool ok = fb_queue_http (connection, header, length);
    if (sendbody) {
        char buffer [4096];
        size_t bytes_read;
        while ((bytes_read = fread (buffer, 1, sizeof (buffer), file))) {
            ok = ok && fb_queue_http_alloc (connection, buffer, bytes_read);
        }
        /* Should deal with changing file size, not that it would occur often. */
    }
    return ok;
}

/** @internal
    Determine request maliciousness.
    Requests for a language with a '.' or '/' characters are malicious.
    Assess the filename of a GET/HEAD request to see if it looks maliciously crafted.
    Currently, disallow hidden files or parent directories.
    @param request the request details
    @param filename the requested pathname.
    @return true if the request appears malicious. */
static bool malicious_request (FB_HTTPREQUEST *request, const char *filename) {
    if (request->language) {
        if (strchr (request->language, '/'))
            return true;
        bool in_language_name = true;
        for (char *c = request->language; *c; c++) {
            if (*c == ';')
                in_language_name = false;
            else if (isspace (*c))
                in_language_name = true;
            else if (in_language_name && *c == '.')
                return true;
        }
    }
    if (*filename == '.')
        return true;
    return strstr (filename, "/.") != NULL;
}


/** @internal
    Determine if request path is valid on the server side.
     - Ensure that the served file is within the served area.  We check earlier
       for malicious requests; this is for erroneous or abusive configuration where
       a symlink redirects outsite the serve directory.
    @param connection the connection issuing the request
    @param file_path the requested pathname.
    @return true if the request is invalid for some reason. */
static bool request_malfunction (FB_CONNECTION *connection, const char *file_path) {
    assert (connection->service->options.serve_directory);
#if defined (HAVE_REALPATH) || defined (HAVE_CANONICALIZE_FILE_NAME)
#ifdef HAVE_CANONICALIZE_FILE_NAME
    char *canonical_path = canonicalize_file_name (file_path);
#else
    char *canonical_path = realpath (file_path, NULL);
#endif
    bool malfunction = true;
    if (canonical_path) {
        malfunction = strncmp (connection->service->options.serve_directory, canonical_path,
                               strlen (connection->service->options.serve_directory)) != 0;
        if (malfunction) {
            fb_log (FB_WHERE (FB_LOG_ERROR),
                    "Requested file resolves outside serve directory.\n    Requested: %s\n    Resolved:  %s",
                    file_path, canonical_path);
        }
    }
    free (canonical_path);
    return malfunction;
#else
#warning Compiling with support for neither realpath() nor canonicalize_file_name.
#warning An incorrect or malicious symbolic link among the HTML files may expose your data.
    static bool warning_given = false;
    if (!warning_given) {
        fb_log (FB_WHERE (FB_LOG_WARNING), "Not verifing file paths.  Subject to security risks from symbolic links.");
        warning_given = true;
    }
    return false;
#endif
}



/** @internal Private structure for language rankings */
typedef struct language_rank {
    float rank; // Strong comparator, higher is better
    int position; // Secondary comparator, lower is better
    char *language;
} LANGUAGE_RANK;

static char *next_language (char *languages) {
    languages += strcspn (languages, ", \t");
    languages += strspn (languages, ", \t");
    return languages;
}

/** @internal
 A qsort() etc compliant callback that compares languages for sorting.
 @param elem1 the first language rank for comparison.
 @param elem2 the other language rank for comparison.
 @return Negative, 0, or positive value in the usual manner. */
static int fb_language_compare (const void *elem1, const void *elem2) {
    LANGUAGE_RANK *lang1 = (LANGUAGE_RANK *) elem1;
    LANGUAGE_RANK *lang2 = (LANGUAGE_RANK *) elem2;
    if (lang1->rank < lang2->rank)
        return 1;
    if (lang1->rank > lang2->rank)
        return -1;
    return lang1->position - lang2->position;
}

/** @internal
    Parse an Accept-Language header into a list of languages.
    @param socket The connection's socket, to which error messages are sent.
    @param languages The Accept-Language header value.  Modified by this function.
    @param count Set to the number of languages found on return.
    @return Pointer to array of LANGUAGE_RANKs, or NULL on error. */
static LANGUAGE_RANK *parse_language_list (int socket, char *languages, int *count) {
    assert (languages);

    // Count the languages
    *count = 0;
    char *c = languages + strspn (languages, " \t");
    while (*c) {
        (*count)++;
        c = next_language (c);
    }

    if (*count == 0) {
        fb_log (FB_WHERE (FB_LOG_HTTP_ERROR),
                "#%d: Null Accept-Language value: '%s'", socket, languages);
        return NULL;
    }

    // Clean up strangeness while we count.
    for (c = languages; *c; c++) {
        if (*c == '-') {
            *c = '_';
        } else if (isupper(*c)) {
            *c = tolower(*c);
        }
    }

    // Allocate an array for the languages.
    LANGUAGE_RANK *preferences = calloc (*count, sizeof (LANGUAGE_RANK));
    if (!preferences) {
        fb_perror ("calloc");
        return NULL;
    }

    // Put the languages into an array
    fb_log (FB_WHERE (FB_LOG_HTTP_STATUS), "#%d: Accept-Language: %s", socket, languages);
    preferences [0].language = languages + strspn (languages, " \t");
    // Put locations of languages into the rest of the array.
    for (int index = 1; index < *count; index++) {
        preferences [index].language = next_language (preferences [index - 1].language);
    }
    // Parse the individual languages and weights
    for (int index = 0; index < *count; index++) {
        LANGUAGE_RANK *lang = &preferences [index];
        lang->rank = 1;
        lang->position = index;
        char *semi = lang->language;
        bool marked_end = false;
        while (*semi && *semi != ';' && !isspace (*semi)) {
            if (*semi == ',' && !marked_end) {
                *semi = '\0';
                marked_end = true;
            }
            semi++;
        }
        if (*semi == ';') {
            if (strncmp (semi, ";q=", 3) == 0) {
                lang->rank = atof (semi + 3);
            } else {
                fb_log (FB_WHERE (FB_LOG_HTTP_ERROR),
                        "#%d: Accept-Language term malformed: %s",
                        socket, lang->language);
            }
        }
        if (!marked_end) {
            *semi = '\0';
        }
    }
    qsort (preferences, *count, sizeof (LANGUAGE_RANK), fb_language_compare);
    return preferences;
}

/** @internal
    Handle opening a file, accounting for preferred language.
    @param connection The connection and request for which file is being opened.
    @param file_name The name of the file to open, before language mangling.
    @param languages The list of languages from Accept-Language header.
    @param language_count The number of languages in the list.
    @param file_path The name of the file that was opened.  It is the caller's
    responsibility to free this, regardless of whether http_file_open succeeded. */
static FILE *http_file_open (FB_CONNECTION *connection,
                             const char *file_name,
                             LANGUAGE_RANK *languages,
                             int language_count,
                             char **file_path) {
    assert (connection);
    assert (file_name);
    assert (language_count == 0 || languages != NULL);

    FILE *file = NULL;
    FB_SERVICE_OPTIONS *options = &connection->service->options;

    if (languages) {
        // Get the longest language name and dump the preferences
        size_t required_length = 0;
        for (int index = 0; index < language_count; index++) {
            LANGUAGE_RANK *lang = &languages [index];
            size_t length = strlen (lang->language);
            if (length > required_length) {
                required_length = length;
            }
#ifndef NDEBUG
            fb_log (FB_WHERE (FB_LOG_HTTP_STATUS),
                    "#%d: Language %d is %s, rank=%f, position=%d",
                    connection->socket, index, lang->language, (double) lang->rank, lang->position);
#endif
        }

        // Worst case filename: path, '/', language subdir, '/', file_name, '.', language (found above) + null;
        required_length += (strlen (options->serve_directory) +
                            (options->locale_directory ? strlen (options->locale_directory) : 0) +
                            strlen (file_name) + 5);
        *file_path = malloc (required_length);
        if (!*file_path) {
            return NULL;
        }
        strcat (strcpy (*file_path, options->serve_directory), "/");
        if (options->locale_directory) {
            strcat (strcat (*file_path, options->locale_directory), "/");
        }
        char *language_start = strchr(*file_path, '\0');

        for (int pass = 0; !file && pass < 2; pass++) {
            for (int index = 0; !file && index < language_count && !file; index++) {
                *language_start = '\0';
                LANGUAGE_RANK *lang = &languages [index];
                if (pass) {
                    // On second pass, remove country (EN_us -> EN).
                    char *c = strchr (lang->language, '_');
                    if (!c) continue; // No country; skip it.
                    *c = '\0';
                }
                if (options->internationalization == FB_INTERNATIONALIZATION_DIRECTORY) {
                    strcat (strcat (strcat (language_start, lang->language), "/"), file_name);
                } else {
                    assert (options->internationalization == FB_INTERNATIONALIZATION_EXTENSION);
                    strcat (strcat (strcat (language_start, file_name), "."), lang->language);
                }
                file = fopen (*file_path, "r");
                if (file) {
                    fb_log (FB_WHERE (FB_LOG_HTTP_STATUS),
                            "#%d: Found language %s, serving file: %s",
                            connection->socket, lang->language, *file_path);
                }
            }
        }
    }
    if (!file) {
        free (*file_path);
        if ((asprintf (file_path, "%s/%s", options->serve_directory, file_name) < 0)) {
            *file_path = NULL;
            return NULL;
        }
        file = fopen(*file_path, "r");
        if (language_count > 0 && file) {
            fb_log (FB_WHERE (FB_LOG_HTTP_STATUS),
                    "#%d: Language-specific files not found, serving file: %s",
                    connection->socket, file_name);
        }
    }
    return file;
}

/** @internal
    Handle a GET or HEAD request.
    Generate appropriate HTTP error messages if the file is inaccessible or does
    not exist. */
static bool http_file_request (FB_CONNECTION *connection) {
    assert (connection);
    bool failure = false;

    FB_HTTPREQUEST *request = &connection->request;
    FB_SERVICE_OPTIONS *options = &connection->service->options;
    char *filename;
    if (request->filename) {
        if (!*request->filename ||
            *(request->filename + strlen (request->filename) - 1) == '/') {
            if ((asprintf (&filename, "%s/index.html", request->filename) < 0)) {
                fb_perror ("asprintf");
                filename = NULL;
            }
        } else {
            filename = strdup (request->filename);
        }
    } else {
        filename = strdup ("index.html");
    }
    if (!filename) {
        http_response (connection, "500 Internal server error");
        failure = true;
    } else if (!options->serve_directory) {
		http_response (connection, "503 Service unavailable");
    } else if (malicious_request (&connection->request, filename)) {
        http_response (connection, "406 Not acceptable");
    } else {
        LANGUAGE_RANK *preferences = NULL;
        int language_count = 0;
        if (connection->service->options.internationalization != FB_INTERNATIONALIZATION_NONE
            && connection->request.language) {
            preferences = parse_language_list (connection->socket,
                                               connection->request.language,
                                               &language_count);
            if (!preferences) {
                failure = true;
            }
        }

        if (!failure) {
            char *file_path = NULL;
            FILE *file = http_file_open (connection, filename, preferences, language_count, &file_path);
            if (file) {
                if (request_malfunction (connection, file_path)) {
                    failure = true;
                } else if (!http_serve_data (connection, filename, file, !request->headonly)) {
                    failure = true;
                }
                fclose (file);
            } else if (errno == ENOENT) {
                http_response (connection, "404 Not found");
            } else if (errno == EACCES) {
                http_response (connection, "401 Unauthorized");
            } else {
                failure = true;
            }
            free (file_path);
        }
        if (failure) {
            http_response (connection, "500 Internal server error");
        }
        free (preferences);
    }
    free (filename);
    return !failure;
}


/** @internal
    Interpret the collected HTTP request.
    Return an HTTP error if the request is invalid.
    If it is a WebSocket request, initiate a WebSocket session.
    Otherwise serve the request and reset the connection.
    @param event a partially filled event applicable to this connection.
    @param connection the connection being served.
    @return An FB_EVENT_CONNECT event if a WebSocket session is initiated, NULL otherwise. */
FB_EVENT *fb_execute_http_request (FB_EVENT *event, FB_CONNECTION *connection) {
    bool try_again = false;
	/* If there's been a problem along the way, report that.
     Otherwise, see if the request indicated a service we know.
     If so, handle it; if not, report it's not found.
     */
    FB_HTTPREQUEST *request = &connection->request;
    if (request->failure) {
        http_response (connection, "500 Internal server error");
    } else if (request->invalid || !request->host) {
        http_response (connection, "400 Bad request");
    } else if (request->unsupported) {
		http_response (connection, "405 Unimplemented");
	} else if (!request->http || (strcmp (request->http, HTTP_VERSION) != 0)) {
		http_response (connection, "505 HTTP Version Not Supported");
    } else if (connection->service->options.name &&
               request->service_name && !request->filename && !request->upgrade_type &&
               (strcasecmp (request->service_name, connection->service->options.name) == 0)) {
        /* http://my.server/service -> http://my.server/service/ */
        redirect_to_subdirectory (connection);
		try_again = true;
    } else if (connection->service->options.name &&
               request->service_name && !request->upgrade_type &&
               strcasecmp (request->service_name, "") == 0) {
        /* http://my.server/ -> http://my.server/service/ */
        redirect_to_subdirectory (connection);
		try_again = true;
    } else if (connection->service->options.name && request->service_name &&
               strcasecmp (request->service_name, connection->service->options.name) != 0) {
        /* http://my.server/some_other_service/whatever (With or without file) */
        if (fb_transfer_by_name (connection, request->service_name)) {
            return fb_execute_http_request(event, connection);
        }
        http_response (connection, "404 Not Found");
    } else if (!request->upgrade_type) {
        try_again = http_file_request (connection);
	} else if (strcasecmp (request->upgrade_type, "websocket") != 0) {
		http_response (connection, "501 Not Implemented");
	} else if (!request->websocket_version || (strcmp (request->websocket_version, WEBSOCKET_VERSION) != 0)) {
		/* Tell the client what version we like. */
		http_header (connection, "400 Bad Request", "Sec-WebSocket-Version: " WEBSOCKET_VERSION,
                     "Unsupported websocket version.  " WEBSOCKET_VERSION " is supported.");
		try_again = true;
	} else if (request->websocket_protocol) {
        /* Pre-RFC WebSocket attempt. */
		http_response (connection, "415 Unsupported Media Type");
	} else if (!request->websocket_key) {
		http_response (connection, "400 Bad Request");
	} else {
        char *params = connection->request.query_string ? connection->request.query_string : "";
        char *name = connection->service->options.name ? connection->service->options.name : "";
        int length = strlen (params) + strlen (name) + 8;
        if ((event->command = malloc (length))) {
            strcpy (event->command, connection->encrypted ? "HTTPS=" : "HTTP=");
            strcat (event->command, name);
            strcat (event->command, "&");
            strcat (event->command, params);
            event->param_count = fb_create_argv_from_query_string (event->command, &event->param_names, &event->param_values);
        }
        if (event->command && event->argc != -1 && fb_greet_websocket(connection)) {
            connection->state = FB_SOCKET_STATE_OPEN;
            connection->expiry_time = FAR_FUTURE;
            event->type = FB_EVENT_CONNECT;
            fb_destroy_httprequest (&connection->request);
            return (event);
        }
    }
    fb_destroy_httprequest (&connection->request);
    if (try_again) {
        if (connection->state < FB_SOCKET_STATE_OPEN) {
            connection->state = FB_SOCKET_STATE_GREETING;
        }
    } else {
        fb_close_connection (connection);
    }
    return NULL;
}


