///
/// Essential data structures and support.
/// @file       fundamentals.h - pianod project
/// @author     Perette Barella
/// @date       2012-03-10
/// @copyright  Copyright 2012–2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <ctime>
#include <cassert>
#include <cstring>

#include <string>
#include <vector>
#include <stdexcept>

/** kept_assert: An assert with important code inside it.
    In debugging mode, the result must be true/non-0.
    In release mode, the result is unchecked but the
    expression is still evaluated. */
#ifdef NDEBUG
#define kept_assert(x) (x)
#else
#define kept_assert(x) assert(x)
#endif

// Works out to 2038 in 4-byte time, surely after the apocolypse for 8-byte
#define FAR_FUTURE ((time_t) (((time_t) 0x7fffffff) << ((sizeof (time_t) - 4) * 8)))
#define A_LONG_TIME (999999)

enum class SearchRange {
    EXHAUSTIVE,     ///< Get everything matching, and all their contents.
    SHALLOW,        ///< Get matching things, but *not* their contents.
    KNOWN,          ///< Get whatever we know about; don't perform external searches.
    REQUESTABLE,    ///< Search requestable sources for matches; return all items matching.
    REQUESTS        ///< Search requestable sources for matches, but *not* their contents.
};
inline bool forRequest (SearchRange range) {
    return (range == SearchRange::REQUESTABLE || range == SearchRange::REQUESTS);
}
inline bool deepSearch (SearchRange range) {
    return (range == SearchRange::EXHAUSTIVE ||
            range == SearchRange::REQUESTABLE);
}


/// Audio output device & driver parameters
typedef struct AudioSettings_t {
    /// Specify which output library for pianod to use.  If empty, uses default.
    std::string output_library;
    /// Specify output driver, for use by audio output library.
    std::string output_driver;
    /// Specify a specific output device, if there are multiple instances or
    /// channels.  Precise meaning is up to audio output library.
    std::string output_device;
    /// For use by audio output library.  Numeric but stored as string to accommodate unset value.
    std::string output_id;
    /// For use by audio output library.
    std::string output_options;
    /// For output to network services such as IceCast, the target server info.
    std::string output_server;
    /// Initial volume level, managed by pianod.
    int volume;
    /// @internal Range of adjustment to use when crossfading, in dB.
    float crossfade_level = 6.0f;
    /// @internal Duration of crossfade, in seconds, or 0 to disable crossfading.
    float crossfade_time = 0.0f;
    /// @internal Number of seconds to preroll next song, allowing buffering before starting playback.
    float preroll_time = 5.0f;
} AudioSettings;

// This should go home to response once that's refactored 
typedef enum server_status_t {
    NO_REPLY = 0, ///< Do not transmit any reply.
    // Informational messages, occur anytime
    // Playback status
    V_PLAYING = 1, // 101-109 Playback status
    V_PAUSED = 2,
    V_STALLED = 3,
    V_TRACK_COMPLETE = 4,
    V_BETWEEN_TRACKS = 5,
    V_IDLE = 6,
    V_QUEUE_STOPPED = 7,
    V_QUEUE_REQUEST = 8,
    V_QUEUE_RANDOM = 9,
    // Selected stuff status
    V_SELECTEDSOURCE = 11,
    V_SELECTEDPLAYLIST = 12,
    // Change notifications
    V_MIX_CHANGED = 21,
    V_PLAYLISTS_CHANGED = 22,
    V_PLAYLISTRATING_CHANGED = 23, // Also actions changed
    V_SOURCES_CHANGED = 24,
    V_SONGRATING_CHANGED = 25,
    V_QUEUE_CHANGED = 26,
    V_YELL = 31, // Misc
    V_USERACTION = 32,
    V_SERVER_STATUS = 33,
    V_SOURCE_STATUS = 34,
    // Queue randomization/Selection method stuff
    V_SELECTIONMETHOD = 41,

    // Data fields, may occur spontaneously or in a data response
    I_WELCOME = 100,
    I_ID = 111, /* Song ID */ // 111-129 Playlist/artist/track field ids
    I_ALBUM = 112,
    I_ARTIST = 113,
    I_SONG = 114,
    I_PLAYLIST = 115,
    I_RATING = 116,
    I_INFO_URL = 117,
    I_COVERART = 118,
    I_GENRE = 119,
    I_PLAYLISTRATING = 120,
    I_CHOICEEXPLANATION = 121,
    I_OWNER = 122,
    I_SOURCE = 123,
    I_NAME = 124,
    I_YEAR = 125,
    I_DURATION = 126,
    I_ACTIONS = 127,
    I_INFO = 132,
    I_USER_PRIVILEGES = 136,
    // pianod settings
    I_VOLUME = 141,
    I_HISTORYSIZE = 142,
    I_AUDIOQUALITY = 143,
    I_AUTOTUNE_MODE = 144,
    I_PAUSE_TIMEOUT = 146,
    I_PLAYLIST_TIMEOUT = 147,
    I_ROOM = 148,
    // Pandora communication settings 
    I_PROXY = 161,
    I_CONTROLPROXY = 162,
    // 163..169 available for reuse mid 2021.
    I_SERVICE_USER = 170,
    I_SERVICE_PASSWORD = 171,
    I_CACHE_MINIMUM = 172,
    I_CACHE_MAXIMUM = 173,
    I_PATHNAME = 174,
    I_SELECTION_ALGORITHM = 175,
    
    I_OUTPUT_DRIVER = 181,
    I_OUTPUT_DEVICE = 182,
    I_OUTPUT_ID = 183,
    I_OUTPUT_SERVER = 184,
    
    I_STATISTICS_ATTEMPTS = 190,
    I_STATISTICS_PLAYS = 191,
    I_STATISTICS_FAILURES = 192,
    I_STATISTICS_SEQUENTIAL_FAILS = 193,
    I_STATISTICS_REPLACEMENTS = 194,
    I_STATISTICS_DONATIONS = 195,
    
    I_ATTACHED_THING = 198, ///< Transmit the attached thing instead of a response message.
    I_ATTACHED_SONG = 199, ///< Transmit the essentials of the attached song (but not ratings, etc).

    // Success/status messages, exactly one occurs (except for lists, as noted below)
    // in response to commands.
    S_OK = 200,
    S_ANSWER_YES = 201,
    S_ANSWER_NO = 202,
    S_DATA = 203, // For multi line items: Occurs 0 or more times, once before each listed group. 
    // For single line items: occurs once 
    S_DATA_END = 204, // Occurs exactly once after last list item 
    S_SIGNOFF = 205,
    S_MATCH = 206, ///< Matches for criteria were found
    S_ROUNDING = 207, ///< Success but with rounding
    S_NOOP = 208, ///< Nothing to do; default success.
    S_PARTIAL = 209, ///< Command partially succeeded, but there were failures.
    S_PENDING = 210, ///< Request is pending but will succeed eventually
    
    /*  300-399 are diagnostic messages, paralleling 400-499 error
        messages.  Diagnostics (possibly multiples) are followed by
        single E_* error message, which concludes the response; the
        individual diagnostics do not. */

    /*  Errors for user failures, unauthorized actions, bad commands,
        the connection is down, etc.  Always in response to a command,
        one per command. */
    E_BAD_COMMAND = 400,
    E_UNAUTHORIZED = 401,
    E_NAK = 402, // negative acknowledgement 
    E_DUPLICATE = 403,
    E_NOTFOUND = 404,
    E_WRONG_STATE = 405,
    E_CREDENTIALS = 406,
    E_INVALID = 407,
    E_TRANSFORM_FAILED = 408,
    E_CONFLICT = 409,
    E_REQUESTPENDING = 410, ///< Request couldn't be completed now, we'll try again later.
    E_QUOTA = 411, ///< Quota restriction encountered.
    E_LOGINREQUIRED = 412, ///< Command/feature requires user be logged in
    E_UNSUPPORTED = 413, // Not allowed by media source/media player
    E_RESOURCE = 414, ///< Inadequate memory, disk, etc.
    E_RANGE = 415, ///< Request puts value out of range
    E_METAPLAYLIST = 416, ///< Require a real playlist
    E_WRONGTYPE = 417, ///< Operand was of wrong type
    E_PERSISTENT = 418, ///< Require persistent data
    E_AMBIGUOUS = 419, ///< Ambiguous expression
    E_TYPE_DISALLOWED = 420, ///< Type not allowed with expression
    E_PARTIAL = 421, ///< Partial failure, but a portion succeeded.
    E_VARIOUS = 422, ///< All failures, but a varity of reasons.
    E_NO_ASSOCIATION = 423, ///< Missing association data, i.e., song has no playlist.
    E_EXPRESSION = 424, ///< Invalid expression
    E_TIMEOUT = 425, ///< Event did not occur within specified duration
    E_PLAYLIST_REQUIRED = 426, ///< Song must have a playlist for this action.
    E_BAD_SCHEMA = 427, ///< JSON command is structured incorrectly.
    /* Server failures, like out of memory, etc.
     Not a response to a particular command, although a command may initiate what causes them */
    E_MEDIA_ACTION = 460, ///< Source doesn't do that
    E_MEDIA_VALUE = 461, ///< Source doesn't accept a certain value
    E_MEDIA_MANAGER = 462, ///< Can't do that on media manager
    E_MEDIA_FAILURE = 463, ///< Source failure
    E_MEDIA_TRANSIENT = 464, ///< Transient playlist
    E_BUG = 498, ///< A bug was encountered (likely hit default in a case statement.)
    E_NOT_IMPLEMENTED = 499,

    // Failures: Spontaneous problems, unrelated to a command.
    F_FAILURE = 500,
    F_PLAYER_EMPTY = 501,
    F_NETWORK_FAILURE = 502,
    F_SHUTDOWN = 503,
    F_AUTHENTICATION = 504,
    F_RESOURCE = 505,
    F_PANDORA = 507,
    F_INCOMPLETE = 508,
    F_PERMISSION = 509,
    F_EXCEPTION = 510,
    F_NETWORK_TIMEOUT = 511,
    F_CANNOT_OUTPUT = 512,
    F_AUDIO_FAILURE = 513,

    // Action messages.  These are used to lookup text, in case we want to internationalize messaging. 
    A_SIGNED_IN = 1000,
    A_SIGNED_OUT = 1001,
    A_KICKED = 1002,
    A_IMBECILE = 1003,
    A_SKIPPED = 1010,
    A_STOPPED = 1011,
    A_PAUSED = 1012,
    A_RESUMED = 1013,
    A_CHANGED_MIX = 1014,
    A_MIX_ADDED = 1015,
    A_MIX_REMOVED = 1016,
    A_REQUESTS = 1017,
    A_RANDOMPLAY = 1018,
    A_ADJUSTAUDIO = 1019,
    A_SELECTED_PLAYLIST = 1020,
    A_CREATED_PLAYLIST = 1021,
    A_RENAMED_PLAYLIST = 1022,
    A_DELETED_PLAYLIST = 1023,
    A_SOURCE_ADD = 1030,
    A_SOURCE_BORROW = 1031,
    A_SOURCE_REMOVE = 1032,
    A_REQUEST_ADD = 1040,
    A_REQUEST_CLEAR = 1041,
    A_REQUEST_CANCEL = 1042,
    A_SHUTDOWN = 1100,
    FUTURE_CATEGORY = 3000
} RESPONSE_CODE;

inline bool isStatusChange (RESPONSE_CODE code) {
    return code >= 1 && code < 100;
}

inline bool isDataField (RESPONSE_CODE code) {
    return code >= 100 && code < 200;
}

inline bool isSuccess (RESPONSE_CODE code) {
    return code >= 200 && code < 300;
}

inline bool isCommandError (RESPONSE_CODE code) {
    return code >= 400 && code < 500;
}

inline bool isServerFailure (RESPONSE_CODE code) {
    return code >= 500 && code < 600;
}

inline bool isUserAction (RESPONSE_CODE code) {
    return code >= 1000 && code < 2000;
}

/** Exception for command execution problems.  If not caught, the connection's
    exception handler sends out the contained response. */
class CommandError final : public std::exception {
private:
    const RESPONSE_CODE _reason;
    char *detail = nullptr;
public:
    CommandError (RESPONSE_CODE r) : _reason (r) { };
    CommandError (RESPONSE_CODE r, const char *message)
    : _reason (r), detail (strdup (message)) { };
    CommandError (RESPONSE_CODE r, const std::string &message)
    : CommandError (r, message.c_str()) {};
    CommandError (const CommandError &) = delete;
    CommandError (CommandError &&from)
    : _reason (from._reason), detail (from.detail) {
        from.detail = nullptr;
    }
    CommandError &operator = (const CommandError &) = delete;
    CommandError &operator = (CommandError &&from) {
        *const_cast<RESPONSE_CODE *> (&_reason) = from._reason;
        detail = from.detail;
        from.detail = nullptr;
        return *this;
    }
    inline RESPONSE_CODE reason (void) const { return _reason; };
    virtual inline const char *what() const throw() override {
        return detail ? detail : "";
    };
};

class InitializationException : public std::exception {
    std::string _detail;
    const char *detail = nullptr;
public:
    InitializationException () { };
    InitializationException (const char *reason) : detail (reason) { };
    InitializationException (const char *library, const char *reason) :
    _detail (std::string (library) + ": " + reason) { detail = _detail.c_str(); };
    virtual inline const char *what() const throw() override {
        return detail ? detail : "No explanation";
    };
};

#define CMD_RANGE_SERVICE (1000)
#define CMD_RANGE_ENGINE (2000)
#define CMD_RANGE_USER (3000)
#define CMD_RANGE_MEDIA_MANAGER (4000)
#define CMD_RANGE_TUNING (5000)
#define CMD_RANGE_PANDORA (10000)
#define CMD_RANGE_TONEGENERATOR (11000)
#define CMD_RANGE_FILESYSTEM (12000)
#define CMD_RANGE_SPOTIFY (13000)

const int CMD_INVALID = 0;



using UserList = std::vector<const class User *>;

/** Privilege management for media sources. */
class Ownership {
public:
    /// Access levels for an object.
    /// @warning Enumeration values and order matter (code compares access & actions).
    enum class Type {
        DISOWNED,       ///< Object has no owner
        PRIVATE,        ///< Visible by owner only
        SHARED,         ///< Others can use this, but not derive from it
        PUBLISHED,      ///< Use and derive by anyone but only owner can manipulate
        PUBLIC          ///< Anyone can manipulate this
    };
    /// Access actions for an object.
    /// @warning Enumeration values and order matter (code compares access & actions).
    enum class Action {
        SEE,            ///< View existence of item
        USE,            ///< Use the item to do something
        READ,           ///< Read the item's contents/configuration
        ALTER           ///< Change the item
    };

    virtual bool isOwnedBy (const User *user) const = 0;
    virtual bool hasPermission (const User *user, Action action) const = 0;

    inline bool isVisibleBy (const User *user) const {
        return hasPermission (user, Action::SEE);
    }
    inline bool isUsableBy (const User *user) const {
        return hasPermission (user, Action::USE);
    }
    inline bool isDecendableBy (const User *user) const  {
        return hasPermission (user, Action::READ);
    }
    inline bool isReadableBy (const User *user) const  {
        return hasPermission (user, Action::READ);
    }
    inline bool isEditableBy (const User *user) const  {
        return hasPermission (user, Action::ALTER);
    }
};

