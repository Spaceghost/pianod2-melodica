///
/// Send response messages over the line-oriented protocol.
/// @file       responseline.cpp - pianod project
/// @author     Perette Barella
/// @date       2014-10-27.  Split from response.cpp 2021-02-06.
/// @copyright  Copyright 2012–2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cstdarg>
#include <cassert>
#include <sstream>
#include <iomanip>

#include <football/fb_public.h>

#include "logging.h"
#include "fundamentals.h"
#include "musictypes.h"
#include "response.h"
#include "connection.h"
#include "utility.h"
#include "user.h"

/* Log levels 0x01, 0x02, 0x04, 0x08, 0x10, and 0x20 correspond
   to statuses 0xx, 1xx,  2xx,  3xx,  4xx, and 5xx. */
static inline LogType loglevel_of (RESPONSE_CODE response) {
    int level = ((int) response) / 100;
    return (LogType) (1 << ((level < 0 || level > 5) ? 5 : level));
}

/** Transmit a response using line protocol.
    @param destination The place to send it.
    @param offset If unspecified, sends messages as-is.  Set to
    `TransmitAsDiagnostic` to send command error messages as diagnostics. */
void Response::transmitLine (Football::Thingie &destination, int offset) const {
    assert (message != NO_REPLY);
    assert (offset == 0 || isCommandError());
    // Drop JSON-only fields.
    std::ostringstream msg;  ///< Assemble message before tranmission so we can log it too.
    RESPONSE_CODE effective_message = (isUserAction() ? V_USERACTION : message);
    msg << std::setfill ('0') << std::setw (3) << effective_message + offset << ' ';
    if (effective_message == V_YELL) {
        msg << (user ? user->username() + " " : "A visitor ") << ResponseText (effective_message);
    } else {
        msg << ResponseText (effective_message);
        if (isUserAction()) {
            msg << ": " << (user ? user->username() + " " : "A visitor ") << ResponseText (message);
        }
    }
    if (type != Type::EMPTY) {
        msg << ": ";
    }
    switch (type) {
        case Type::EMPTY:
            break;
        case Type::STRING:
            msg << value;
            break;
        case Type::LONG:
            msg << long_value;
            break;
        case Type::DOUBLE:
            msg << std::fixed << std::setprecision (double_precision) << double_value;
            break;  
        case Type::WORDLIST: {
            if (list.empty()) {
                return;
            }
            msg << join_strings (list);
            break;
        }
    }

    if (type == Type::WORDLIST) {
        // Append nothing
    } else if (!regarding.empty()) {
        msg << ": " << regarding;
    } else if (related) {
        msg << ": " << related->name();
    }
    flog (LOG_WHERE (loglevel_of (effective_message)), msg.str());
    msg << std::endl;
    destination.conditional_print (&PianodConnection::line_connections_only, msg.str());
};

/** Use << as output operator for responses.
    @param there The place to send the response.
    @param response The response or data to send.
    @note This needs to be out-of-line as the header file doesn't know PianodConnection
    can be used as a Football::Thingie. */
PianodConnection &operator<< (PianodConnection &there, const Response &response) {
    response.transmitLine (there);
    return there;
}

/** Transmit a group of responses to the client.
    @param there The target to send to.
    @param offset If unspecified, sends messages as-is.  Set to
    `TransmitAsDiagnostic` to send command error messages as diagnostics. */
void ResponseGroup::transmitLine (PianodConnection &there, int offset) const {
    for (const auto &item : *this) {
        if (item.message == I_ATTACHED_THING) {
            assert (item.type == Response::Type::EMPTY);
            assert (item.related);
            item.related->transmitCommon (there);
            item.related->transmitPrivate (there);
        } else if (item.message == I_ATTACHED_SONG) {
            // When dumping a playlist's seeds or ratings, omit details
            // as they are provided in the seed list response.
            assert (item.type == Response::Type::EMPTY);
            assert (item.related);
            assert (item.related->asSong());
            item.related->transmitCommon (there);
        } else {
            item.transmitLine (there, offset);
        }
    }
}

/** Broadcast a group of responses to an entire service.
    @param service The service whose clients to send to. */
void ResponseGroup::transmitLine (PianodService &service) const {
    for (const auto &item : *this) {
        if (item.message == I_ATTACHED_THING) {
            // Ratings and action-privileges can vary by user.
            assert (item.type == Response::Type::EMPTY);
            assert (item.related);
            item.related->transmitCommon (service);
            for (const auto connection : service) {
                item.related->transmitPrivate (*connection);
            }
        } else {
            service << item;
        }
    }
}

/** Transmit a data response OR command reply to a specific node,
    followed by any client-specific add-on information.
    @param destination The node to which to send the response
    or data. */
void ResponseCollector::transmitLine (PianodConnection &destination) const {
    static Response DATA{ S_DATA };
    static Response DATA_END{ S_DATA_END };
    assert (isNoResponse() || noop() == dataResponse());
    if (isNoResponse()) {
        assert (noop());
        assert (!dataResponse());
    } else if (dataResponse()) {
        assert (noop());
        if (!data_reply.empty()) {
            destination << DATA;
            data_reply.transmitLine (destination);
        }
        for (const auto &record : data_groups) {
            destination << DATA;
            record.transmitLine (destination);
        }
        destination << DATA_END;
    } else if (noop()) {
        destination << Response (S_NOOP);
    } else if (allSuccess()) {
        if (successes.size() > 1 && reason == S_OK) {
            destination << Response (S_MATCH, successes.size());
        } else {
            destination << Response (reason);
        }
    } else {
        assert (!diagnostics.empty());
        // There were 1 or more failures.
        if (diagnostics.size() > 1) {
            diagnostics.transmitLine (destination, Response::TransmitAsDiagnostic);
            destination << Response (reason);
        } else {
            diagnostics.transmitLine (destination);
        }
    }

    information.transmitLine (destination);
};
