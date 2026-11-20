///
/// Debug/event logging interface.
/// @file       logging.h - pianod project
/// @author     Perette Barella
/// @date       2012-03-16
/// @copyright  Copyright 2012–2023 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <cstring>
#include <cstdarg>
#include <iostream>
#include <utility>

enum class LogType {
    ERROR = 0, // Nonmaskable 
    LOG_000 = 0x01, // Corresponding to response message groups 
    LOG_100 = 0x02,
    LOG_200 = 0x04,
    LOG_300 = 0x08,
    LOG_400 = 0x10,
    LOG_500 = 0x20,
    // 0x40 not used yet
    GENERAL = 0x80,
    // 0x100 EVENT no longer used
    WARNING = 0x200,
    QUEUE = 0x400,
    // 0x800 not used yet
    USERACTION = 0x1000,
    PROTOCOL = 0x2000,
    ALLOCATIONS = 0x4000,
    CACHES = 0x8000,
    TUNING = 0x10000, ///< Tables of station rating averages for automatic station selection.
    BIASING = 0x20000, ///< Tables of pick probabilities after applying ratings & recent biases.
    METADATA = 0x40000,
    AUDIO = 0x80000,
    PANDORA = 0x100000,
    FILESYSTEM = 0x2000000
};
using Log = LogType;

// Logging
extern LogType flogging_level;
extern bool flog_include_timestamp;
extern void set_logging (LogType logtype);
extern LogType get_logging();
extern void flog_timestamp ();

/// Allow LogType values to be OR'ed together.
inline constexpr LogType operator |(const LogType a, const LogType b) {
    return static_cast <LogType> (static_cast <unsigned> (a) | static_cast <unsigned> (b));
}

/// Allow LogType values to be AND'ed apart.
inline constexpr LogType operator &(const LogType a, const LogType b) {
    return static_cast <LogType> (static_cast <unsigned> (a) & static_cast <unsigned> (b));
}

/// Allow LogType values to be toggled.
inline constexpr LogType operator ^(const LogType a, const LogType b) {
    return static_cast <LogType> (static_cast <unsigned> (a) ^ static_cast <unsigned> (b));
}

/// Allow LogType values to have bits removed..
inline constexpr LogType operator -(const LogType a, const LogType b) {
    return static_cast <LogType> (static_cast <unsigned> (a) & ~static_cast <unsigned> (b));
}


static inline const char *trim_filename (const char *path) {
    const char *name = strrchr (path, '/');
    return name ? name + 1 : path;
}

// LOG_WHERE macro inserts file, line and function for C++ flog calls
// when in debug mode.  In release mode, file, line, and function are omitted.
// LOG_FUNCTION Is similar, but function name is retained in release mode.
// Unlike Football, where the FB_WHERE macro is required, these are optional.
#ifdef NDEBUG
#define LOG_WHERE(level) (level)
#define LOG_FUNCTION(level) (level), __func__, ": "
#else
#define LOG_WHERE(level) (level), trim_filename(__FILE__), ":", __LINE__, " ", __func__, " (", #level, "): "
#define LOG_FUNCTION(level) (level), trim_filename(__FILE__), ":", __LINE__, " ", __func__, " (", #level, "): "
#endif

/// Implement empty argument list for flogging.
inline void flog_details (void) {
    std::clog << std::endl;
};

inline bool logging_enabled (LogType level) {
    return (level == Log::ERROR || static_cast <unsigned> (flogging_level & level));
}

/// Recurse while there are arguments for flogging.
template<typename OutputItem, typename... MoreItems>
void flog_details (OutputItem message, MoreItems&&... more) {
    std::clog << message;
    flog_details (std::forward <MoreItems> (more)...);
}

/// Log messages when their logging category is enabled.
/// Errors are always logged.  Message to to standard out.
template<typename... MoreItems>
void flog (LogType level, MoreItems&&... more) {
    if (logging_enabled (level)) {
        if (flog_include_timestamp)
            flog_timestamp();
        flog_details (std::forward<MoreItems> (more)...);
        std::clog.flush();
    }
}

