///
/// Debug/event logging implementation.
/// @file       logging.cpp - pianod project
/// @author     Perette Barella
/// @date       2012-03-16
/// @copyright  Copyright 2012-2013 Devious Fish. All rights reserved.
///

#include <config.h>

#include "logging.h"

LogType flogging_level = Log::WARNING; // Log errors only
bool flog_include_timestamp = true;

/** Set the logging selections.
    @param logtype A bitfield indicating what events to log. */
void set_logging (LogType logtype) {
    flogging_level = logtype;
}

LogType get_logging() {
    return flogging_level;
}


/** @private Logging helper subroutine to format and emit the time. */
void flog_timestamp () {
        char date [24];
        time_t now = time (NULL);
        strftime(date, sizeof (date),"%Y-%m-%d %H:%M:%S: ", localtime(&now));
        std::clog << date;
}
