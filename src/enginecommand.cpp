///
/// Command handlers for audio engine.
/// Control playback, select music, rate songs and playlists,
/// create/remove playlists and add/remove seeds.
/// @file       enginecommand.cpp - pianod
/// @author     Perette Barella
/// @date       2014-12-08
/// @copyright  Copyright 2014-2023 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <cstdlib>

#include <exception>
#include <memory>
#include <initializer_list>
#include <iomanip>
#include <sstream>

#include <football/football.h>
#include <parsnip/parsnip.h>

#include "fundamentals.h"
#include "lookup.h"
#include "utility.h"
#include "interpreter.h"
#include "engine.h"
#include "predicate.h"
#include "tuner.h"
#include "connection.h"
#include "response.h"
#include "filter.h"
#include "querylist.h"
#include "servicemanager.h"
#include "mediaunit.h"
#include "mediaplayer.h"
#include "mediamanager.h"
#include "audiooptionsparser.h"

/// Audio engine commands such as start, stop, pause, change playlists or seeds.
typedef enum engine_commands_t {
    TIMESTATUS = CMD_RANGE_ENGINE,
    GETVOLUME,
    SETVOLUME,
    GETCROSSFADETIME,
    SETCROSSFADETIME,
    GETCROSSFADELEVEL,
    SETCROSSFADELEVEL,
    ADJUSTVOLUME,
    GETHISTORYSIZE,
    SETHISTORYSIZE,
    WAITFORENDOFSONG,
    WAITFORNEXTSONG,
    QUERYSTATUS,
    QUERYHISTORY,
    QUERYQUEUE,
    NEXTSONG,
    STOPPLAYBACK,
    PAUSEPLAYBACK,
    RESUMEPLAYBACK,
    TOGGLEPLAYBACK,
    PLAY,
    SELECT,
    PLAYLISTRENAME,
    PLAYLISTDELETE,
    PLAYLISTCREATE,
    PLAYLISTFROMFILTER,
    PLAYLISTALTERFILTER,
    GETSUGGESTIONS,
    LISTSONGSBYFILTER,
    LISTSONGSBYPLAYLIST,
    REQUESTMUSIC,
    REQUESTCLEAR,
    REQUESTCANCEL,
    RATE,
    RATEPLAYLIST,
    SEEDLIST,
    SEEDALTER,
    ALTERAUDIOCONFIG,
// EXPLAINSONGCHOICE, Not implemented/future
// CREATEBOOKMARK, Not implemented/future
#ifndef NDEBUG
    FILTERECHO
#endif
} ENGINECOMMAND;

#define KEY_VERB "verb"
#define KEY_QUEUEMODE "queueMode"
#define KEY_PLAYBACK "playback"
#define KEY_SELECTION "select"

#define SEEDVERB " <" KEY_VERB ":add|delete|toggle>"

const Parsnip::OptionParser::Definitions &AudioEngine::playback_selection_option_definitions() {
    /** Parse definitions for playback control */
    // clang-format off
    static const Parsnip::OptionParser::Definitions playback_selection_options {
        "<" KEY_SELECTION ":mix|auto|everything>",
        "<" KEY_SELECTION ":playlist>" LIST_PLAYLIST,
        "<" KEY_SELECTION ":from>" SIMPLE_LIST_PREDICATE,
        "<" KEY_QUEUEMODE ":request|random>",
        "<" KEY_QUEUEMODE ":stop> [when:now|defer]",
        "<" KEY_PLAYBACK ":resume|pause|toggle>"
    };
    // clang-format on
    return playback_selection_options;
}

CommandReply AudioEngine::controlPlayback (const Parsnip::Data &options, bool start_playback, PianodConnection &conn) {
    if (quit_requested) {
        throw CommandError (E_WRONG_STATE, "Shutdown pending");
    }
    CommandReply response (S_OK);
    if (options.contains (KEY_SELECTION)) {
        PianodPlaylist *new_playlist = nullptr;
        if (optionIs (options, KEY_SELECTION, "everything")) {
            new_playlist = conn.source()->getEverythingPlaylist();
        } else if (optionIs (options, KEY_SELECTION, "playlist")) {
            new_playlist = Predicate::getSpecifiedPlaylist (conn, options.getOr (KEY_PLAYLIST, EmptyDictionary));
            mix.automatic (false);
        } else if (optionIs (options, KEY_SELECTION, "from")) {
            require (conn, REQUIRE_SOURCE | REQUIRE_EXPAND);
            auto filter = Predicate::getPredicate (conn, options.getOr (KEY_PREDICATE, EmptyDictionary));
            if (!filter->canPersist()) {
                throw CommandError (E_PERSISTENT);
            }
            new_playlist = conn.source()->getTransientPlaylist (*filter.get());
            if (!new_playlist || (new_playlist->source() != media_manager && new_playlist->songs().empty())) {
                throw CommandError (E_NOTFOUND);
            }
        } else {
            assert (optionIs (options, KEY_SELECTION, "mix") || optionIs (options, KEY_SELECTION, "auto"));
            new_playlist = conn.source()->getMixPlaylist();
        }
        if (!new_playlist) {
            // In case a source doesn't have a mix or everything playlist.
            throw CommandError (E_MEDIA_ACTION);
        }
        mix.automatic (optionIs (options, KEY_SELECTION, "auto"));
        current_playlist = new_playlist;
        response.room_events (current_playlist, A_SELECTED_PLAYLIST);
        response.room_events (gatherSelectedPlaylist());
        mix.recalculatePlaylists();
    }
    if (start_playback || options.contains (KEY_PLAYBACK)) {
        media_manager->resetLockout();
        PlaybackState new_state = PlaybackState::PLAYING;
        if (optionIs (options, KEY_PLAYBACK, "pause")) {
            new_state = PlaybackState::PAUSED;
        } else if (optionIs (options, KEY_PLAYBACK, "toggle")) {
            new_state = (playback_state == PlaybackState::PLAYING ? PlaybackState::PAUSED : PlaybackState::PLAYING);
        }
        response.room_events (playbackState (new_state, &conn));
    }
    if (start_playback || options.contains (KEY_QUEUEMODE)) {
        if (optionIs (options, "when", "now") && player) {
            abort_playpoint = 1;
            player->abort();
        }
        response.room_events (queueMode (optionIs (options, KEY_QUEUEMODE, "request") ? QueueMode::REQUESTS
                                         : optionIs (options, KEY_QUEUEMODE, "stop")  ? QueueMode::STOPPED
                                                                                      : QueueMode::RANDOMPLAY,
                                         &conn));
    }
    return response;
}

//clang-format off
const Parsnip::Parser::Definitions &AudioEngine::parser_definitions (void) {
    static const Parsnip::Parser::Definitions engine_statements = {
#ifndef NDEBUG
        { FILTERECHO, "test filter echo" SIMPLE_LIST_PREDICATE },  // Test predicate filter generation
#endif

        // Status, queue and history
        { TIMESTATUS, "" },                                       // On null input, display time.
        { QUERYSTATUS, "status" },                                // Current song, etc.
        { QUERYHISTORY, "history list [{#index:-9999-9999}]" },   // Previously played songs
        { QUERYQUEUE, "queue list [{#index:-9999-9999}]" },       // Show upcoming songs
        { GETHISTORYSIZE, "get history length" },                 // Read the length of the history
        { SETHISTORYSIZE, "set history length {#length:1-50}" },  // Set the length of the history

        { GETVOLUME, "volume" },                                            // Query volume level
        { SETVOLUME, "volume level {#level:-100-100}" },                    // Directly set the volume level
        { ADJUSTVOLUME, "volume <adjustment:up|down> [{#change:1-100}]" },  // Adjust the volume level
        { GETCROSSFADETIME, "crossfade duration" },                         // Query song overlap
        { SETCROSSFADETIME, "crossfade duration {#seconds:0.0-15}" },       // How long to overlap songs
        { GETCROSSFADELEVEL, "crossfade level" },                           // Query crossfade volume adjustment
        { SETCROSSFADELEVEL, "crossfade level {#level:0.0-50}" },  // How much volume adjustment when crossfading
        { WAITFORENDOFSONG,
          "wait for end of [which:current] song [{" KEY_WAITOPTIONS ":" KEY_WAITOPTIONS
          "}] ..." },  // Delay further input until after song ends
        { WAITFORNEXTSONG,
          "wait for next song [{" KEY_WAITOPTIONS ":" KEY_WAITOPTIONS
          "}] ..." },  // Delay further input until next song begins

        { NEXTSONG, "skip [manner:normal|abrupt]" },   // Skip the rest of the current song
        { PAUSEPLAYBACK, "pause" },      // Pause playback
        { STOPPLAYBACK, "stop [when:now|defer]" },  // Stop playback when song finished
        { RESUMEPLAYBACK, "resume" },
        { TOGGLEPLAYBACK, "pause toggle" },  // Toggle music playback
        { PLAY,
          "play [{" KEY_PLAYBACKSELECTION ":" KEY_PLAYBACKSELECTION
          "}] ..." },  /// Play/select music, queue mode, playback
        { SELECT,
          "select {" KEY_PLAYBACKSELECTION ":" KEY_PLAYBACKSELECTION
          "} ..." },  /// Play/select music, queue mode, playback

        // Playlist management commands
        { PLAYLISTRENAME, "playlist rename" SINGLE_PLAYLIST " to {newname}" },
        { PLAYLISTDELETE, "playlist delete" LIST_PLAYLIST },
        { PLAYLISTCREATE, "playlist create [smart:smart|seeded] from" LIST_PREDICATE },
        { PLAYLISTCREATE, "playlist create [smart:smart|seeded] name {name} from" LIST_PREDICATE },
        { PLAYLISTFROMFILTER, "playlist create name {name} where {expression...}" },
        { PLAYLISTALTERFILTER, "playlist alter " SINGLE_PLAYLIST " filter where {expression...}" },
        // Content control -- ratings and seeds
        { GETSUGGESTIONS, "find [range:all|suggestion|request]" LIST_PREDICATE },
        { RATE, "rate song {rating}" },  // Rate song
        { RATE, "rate song {rating}" LIST_PREDICATE },
        { RATEPLAYLIST, "rate playlist {rating}" },  // Rate playlists
        { RATEPLAYLIST, "rate playlist {rating}" LIST_PLAYLIST },
        { SEEDLIST, "seed list" },
        { SEEDLIST, "seed list playlist" LIST_PLAYLIST },  // List the seeds and ratings
        { SEEDALTER, "seed" SEEDVERB },                    // Manipulate one seed on multiple playlists
        { SEEDALTER, "seed" SEEDVERB SEED_PREDICATE },     // Manipulate one seed on multiple playlists
        { SEEDALTER,
          "playlist modify" SINGLE_PLAYLIST SEEDVERB
          " seed" LIST_PREDICATE },  // Manipulate multiple seeds on one playlist

        // Song info and requests
        { LISTSONGSBYFILTER, "song list" LIST_PREDICATE },  // List songs matching the expression
        { LISTSONGSBYPLAYLIST,
          "playlist song list" LIST_PLAYLIST },                     // List songs from playlist matching the expression
        { REQUESTCLEAR, "request clear" },                          // Clear request queue
        { REQUESTMUSIC, "request" LIST_PREDICATE },                 // Request some music
        { REQUESTCANCEL, "request cancel" SIMPLE_LIST_PREDICATE },  // Remove items from request queue
        { ALTERAUDIOCONFIG,
          "room reconfigure [{" KEY_AUDIOOPTIONS ":" KEY_AUDIOOPTIONS "}] ..." }  // Change audio settings
    };
    return engine_statements;
};

const Parsnip::Parser::Definitions &AudioEngine::getParserDefinitions() {
    return parser_definitions();
}

/** Retrieve names for our JSON requests.
    @return Request name to command ID mappings. */
const PianodSchema::CommandIds &AudioEngine::json_request_names() {
    static const PianodSchema::CommandIds mappings{
#ifndef NDEBUG
        { "filterEchoTest", FILTERECHO },
#endif
        { "getPlaybackStatus", TIMESTATUS },
        { "getStatus", QUERYSTATUS },
        { "getHistory", QUERYHISTORY },
        { "getQueue", QUERYQUEUE },
        { "getHistoryLength", GETHISTORYSIZE },
        { "setHistoryLength", SETHISTORYSIZE },
        { "getVolume", GETVOLUME },
        { "setVolume", SETVOLUME },
        { "adjustVolume", ADJUSTVOLUME },
        { "getCrossfadeTime", GETCROSSFADETIME },
        { "setCrossfadeTime", SETCROSSFADETIME },
        { "getCrossfadeLevel", GETCROSSFADELEVEL },
        { "setCrossfadeLevel", SETCROSSFADELEVEL },
        { "waitForEndOfTrack", WAITFORENDOFSONG },
        { "waitForNextTrack", WAITFORNEXTSONG },
        { "skip", NEXTSONG },
        { "pause", PAUSEPLAYBACK },
        { "stop", STOPPLAYBACK },
        { "resume", RESUMEPLAYBACK },
        { "pauseToggle", TOGGLEPLAYBACK },
        { "play", PLAY },
        { "select", SELECT },
        { "renamePlaylist", PLAYLISTRENAME },
        { "deletePlaylist", PLAYLISTDELETE },
        { "createPlaylist", PLAYLISTCREATE },
        { "createPlaylistFromFilter", PLAYLISTFROMFILTER },
        { "alterPlaylistFilter", PLAYLISTALTERFILTER },
        { "getSuggestions", GETSUGGESTIONS },
        { "rateTrack", RATE },
        { "ratePlaylist", RATEPLAYLIST },
        { "getSeeds", SEEDLIST },
        { "alterSeeds", SEEDALTER },
        { "getTracks", LISTSONGSBYFILTER },
        { "getPlaylistTracks", LISTSONGSBYPLAYLIST },
        { "request", REQUESTMUSIC },
        { "clearRequests", REQUESTCLEAR },
        { "cancelRequests", REQUESTCANCEL },
        { "adjustRoomAudio", ALTERAUDIOCONFIG }
    };
    return mappings;
}

static const LookupTable<SearchRange> SearchRanges{ { "all", SearchRange::EXHAUSTIVE },
                                                    { "suggestion", SearchRange::SHALLOW },
                                                    { "request", SearchRange::REQUESTABLE },
                                                    { "known", SearchRange::KNOWN } };

enum class SkipManner {
    NORMAL,
    ABRUPT
};
static const LookupTable<SkipManner> SkipManners{ { "normal", SkipManner::NORMAL },
                                                  { "abrupt", SkipManner::ABRUPT } };
                                                  
enum class StopTime {
    NOW,
    SONGEND
};
static const LookupTable<StopTime> StopTimes{ { "now", StopTime::NOW },
                                              { "defer", StopTime::SONGEND } };


void AudioEngine::registerInterpreter (PianodDispatcher &dispatcher) {
    PianodInterpreter::registerInterpreter (dispatcher);
    mix.registerInterpreter (dispatcher);
}

/** Check a list of somethings for authorized use.
    @tparam ListType The type of the list.
    @param items_to_check The list of items.
    @param user The user requesting authorization.
    @param diagnostics A diagnostics collector for failed authorizations.
    @param usage The manner in which the items are to be used.
    @return A list of somethings for which use is authorized. */
template <typename ListType>
ListType AuthorizedUse (const ListType &items_to_check, User *user, CommandReply *diagnostics, Ownership::Action usage) {
    ListType valid;
    for (auto item : items_to_check) {
        if (item->hasPermission (user, usage)) {
            valid.push_back (item);
        } else {
            diagnostics->fail (item, E_UNAUTHORIZED);
        }
    }
    return valid;
}

bool AudioEngine::authorizedCommand (Parsnip::Parser::CommandId command, PianodConnection &conn) {
    switch (command) {
        case TIMESTATUS:
        case QUERYSTATUS:
        case QUERYHISTORY:
        case QUERYQUEUE:
        case GETHISTORYSIZE:
        case GETVOLUME:
        case GETCROSSFADETIME:
        case GETCROSSFADELEVEL:
        case WAITFORENDOFSONG:
        case WAITFORNEXTSONG:
        case RATEPLAYLIST:
        // These are based on ownership.  A listener with service may be an owner
        case PLAYLISTRENAME:
        case PLAYLISTDELETE:
        case PLAYLISTCREATE:
        case PLAYLISTFROMFILTER:
        case PLAYLISTALTERFILTER:
        case LISTSONGSBYPLAYLIST:
        case SEEDLIST:
        case SEEDALTER:
        // Needed by potential owners and for a future "Request" privilege.
        case GETSUGGESTIONS:
        case LISTSONGSBYFILTER:
        // Owner-based, but also sources like filesystem allow all real users:
        case RATE:
            return conn.haveRank (Rank::Listener);

        case REQUESTMUSIC:
        case REQUESTCLEAR:
        case REQUESTCANCEL:
            return conn.haveRank (Rank::Standard) || conn.havePrivilege (Privilege::Queue);

        case SETVOLUME:
        case ADJUSTVOLUME:
        case NEXTSONG:
        case STOPPLAYBACK:
        case PAUSEPLAYBACK:
        case RESUMEPLAYBACK:
        case TOGGLEPLAYBACK:
        case PLAY:
        case SELECT:
        // Crossfades should be a user setting, like volume.
        // But if the audio output doesn't support it (libsdl), it can cause trouble,
        // so it should be admin-only.  Errr... taking a reasonable capability away
        // for this reason seems egregious, so it remains available.
        case SETCROSSFADETIME:
        case SETCROSSFADELEVEL:
            return conn.haveRank (Rank::Standard);
        default:
            return conn.haveRank (Rank::Administrator);
    }
};

/** Check that required state/privileges are available.
    @throw CommandError if requirements are not met. */
void AudioEngine::require (PianodConnection &conn, unsigned long requirements) const {
    assert (conn.source());
    assert (current_playlist);
    // Source-related requirements
    if (requirements & REQUIRE_SOURCE) {
        if (!conn.source()->isReady()) {
            throw CommandError (E_WRONG_STATE, "Source is not ready.");
        }
        if ((requirements & REQUIRE_EXPAND) && !conn.source()->canExpandToAllSongs()) {
            throw CommandError (E_MEDIA_ACTION, "Song lists from current source");
        }
    }
    // Playlist-related requirements
    if (requirements & REQUIRE_PLAYLIST) {
        if ((requirements & REQUIRE_EXPAND) && !current_playlist->source()->canExpandToAllSongs()) {
            throw CommandError (E_MEDIA_ACTION, "Song lists from current playlist");
        }
    }
    // Other requirements
    if ((requirements & REQUIRE_PLAYER) && !player) {
        throw CommandError (E_WRONG_STATE, "Not playing");
    }
}

/** Get a thing from a predicate and ensure it is usable.
    @param conn The connection requesting the items.
    @param options Predicate details from the command line.
    @param usage The manner in which the object is to be accessed.
    @return The thing
    @throw CommandError on invalid request, not found, or insufficient privileges. */
MusicThingie *AudioEngine::getThingOrCurrent (const PianodConnection &conn,
                                              const Parsnip::Data &options,
                                              Ownership::Action usage) const {
    // Get a thing by ID or current song
    MusicThingie *thing = nullptr;
    if (Predicate::havePredicate (options)) {
        thing = Predicate::getSpecifiedThing (conn, options);
    } else if (current_song) {
        thing = current_song.get();
    } else {
        throw CommandError (E_WRONG_STATE, "No current song");
    }
    assert (thing);
    if (!thing->hasPermission (conn.user, usage))
        throw CommandError (E_UNAUTHORIZED);
    return thing;
};

/** Validate and/or convert a thingie to a requested type.
    @param thing The thing to convert
    @param want The type to convert it to.
    @return A pointer to the object, or nullptr if it does not convert.
    @throw CommandError When converting a song to a playlist, but the song has no playlist. */
static MusicThingie *thingie_cast (MusicThingie *thing, MusicThingie::Type want) {
    // Adjust the type, such as getting playlist from song, and validate type.
    switch (want) {
        case MusicThingie::Type::Artist:
            return thing->asArtist();
        case MusicThingie::Type::Album:
            return thing->asAlbum();
        case MusicThingie::Type::Song:
            return thing->asSong();
        case MusicThingie::Type::Playlist: {
            PianodPlaylist *pl;
            PianodSong *song;
            if ((pl = thing->asPlaylist())) {
                return pl;
            } else if ((song = thing->asSong())) {
                thing = song->playlist();
                if (!thing) {
                    throw CommandError (E_INVALID, "Song does not have a playlist.");
                }
                return thing;
            };
            break;
        }
        default:
            assert (!"Switch/case unmatched");
            break;
    }
    return nullptr;
}

/** Get a thing from a predicate, ensure usability and it conforms to a type.
    @param conn The connection requesting the items.
    @param options Predicate details from the command line.
    @param want Indicates the type of thing wanted.
    @param usage The manner in which the object is to be accessed.
    @return The thing
    @throw CommandError on invalid request, not found, or insufficient privileges. */
MusicThingie *AudioEngine::getThingOrCurrent (const PianodConnection &conn,
                                              const Parsnip::Data &options,
                                              MusicThingie::Type want,
                                              Ownership::Action usage) const {
    // Get a thing by ID or current song
    MusicThingie *orig_thing = getThingOrCurrent (conn, options, usage);
    assert (orig_thing);
    MusicThingie *thing = thingie_cast (orig_thing, want);
    if (!thing)
        throw CommandError (E_WRONGTYPE, (*orig_thing)());
    return thing;
};

/** Get some things from a predicate and ensure they is usable.
    @param conn The connection requesting the things.
    @param options Predicate details from the command line.
    @param diagnostics Status aggregator for problems encountered.
    @param usage The manner in which the object is to be accessed.
    @return The things; may be the empty set.
    @throw CommandError on invalid request, not found, or insufficient privileges
    for a single-item predicate. */
ThingieList AudioEngine::getThingsOrCurrent (const PianodConnection &conn,
                                             const Parsnip::Data &options,
                                             CommandReply *diagnostics,
                                             Ownership::Action usage) const {
    assert (diagnostics);
    ThingieList candidates;
    if (!Predicate::havePredicate (options)) {
        candidates.push_back (getThingOrCurrent (conn, options, usage));
        return candidates;
    }
    return AuthorizedUse (Predicate::getSpecifiedThings (conn, options), conn.user, diagnostics, usage);
};

/** Get things from a predicate, ensure usability and it conforms to a type.
    @param conn The connection requesting the things.
    @param options Predicate details from the command line.
    @param want Indicates the type of thing wanted.
    @param diagnostics Status aggregator for problems encountered.
    @param usage The manner in which the object is to be accessed.
    @return The things; may be the empty set.
    @throw CommandError on invalid request, not found, or insufficient privileges
    for a single-item predicate. */
ThingieList AudioEngine::getThingsOrCurrent (const PianodConnection &conn,
                                             const Parsnip::Data &options,
                                             MusicThingie::Type want,
                                             CommandReply *diagnostics,
                                             Ownership::Action usage) const {
    assert (diagnostics);
    ThingieList candidates;
    if (!Predicate::havePredicate (options)) {
        candidates.push_back (getThingOrCurrent (conn, options, usage));
        return candidates;
    }
    candidates = Predicate::getSpecifiedThings (conn, options);
    ThingieList results;
    for (auto item : candidates) {
        try {
            MusicThingie *thing = thingie_cast (item, want);
            if (!thing) {
                diagnostics->fail (item, E_WRONGTYPE);
            } else if (thing->hasPermission (conn.user, usage)) {
                results.push_back (thing);
            } else {
                diagnostics->fail (thing, E_UNAUTHORIZED);
            }
        } catch (const CommandError &err) {
            diagnostics->fail (item, err);
        }
    }
    return results;
}

/** Get a songlist predicate, or use current song.
    @param conn The connection requesting the songs.
    @param options Predicate details from the command line.
    @param diagnostics Status aggregator for problems encountered.
    @param usage The manner in which the object is to be accessed.
    @return The songs; may be the empty set.
    @throw CommandError on invalid request, not found, or insufficient privileges
    for a single-item predicate. */

SongList AudioEngine::getSongsOrCurrent (const PianodConnection &conn,
                                         const Parsnip::Data &options,
                                         CommandReply *diagnostics,
                                         Ownership::Action usage) const {
    assert (diagnostics);
    // Privilege checks done by called function.
    ThingieList things = getThingsOrCurrent (conn, options, MusicThingie::Type::Song, diagnostics, usage);
    SongList songs;
    songs.reserve (things.size());
    for (auto song : things) {
        assert (song->asSong());
        songs.push_back (song->asSong());
    }
    return songs;
}

/** Get a playlist predicate, or use current playlist.
    @param conn The connection requesting the things.
    @param options Predicate details from the command line.
    @param usage The manner in which the object is to be accessed.
    @return The things; may be the empty set.
    @throw CommandError on invalid request, not found, or insufficient privileges
    for a single-item predicate. */
PianodPlaylist *AudioEngine::getPlaylistOrCurrent (const PianodConnection &conn,
                                                   const Parsnip::Data &options,
                                                   Ownership::Action usage) const {
    PianodPlaylist *candidate;
    if (Predicate::havePredicate (options)) {
        candidate = Predicate::getSpecifiedPlaylist (conn, options);
    } else {
        if (!current_song && !current_playlist)
            throw CommandError (E_AMBIGUOUS, "No current playlist");
        if (current_song && current_song->playlist() != current_playlist.get())
            throw CommandError (E_AMBIGUOUS);
        candidate = current_song ? current_song->playlist() : current_playlist.get();
        if (candidate->playlistType() != PianodPlaylist::SINGLE)
            throw CommandError (E_METAPLAYLIST);
    }
    if (!candidate->hasPermission (conn.user, usage))
        throw CommandError (E_UNAUTHORIZED, candidate->id());
    return candidate;
}

/** Get a playlist predicate, or use current playlist.
    @param conn The connection requesting the things.
    @param options Predicate details from the command line.
    @param diagnostics Status aggregator for problems encountered.
    @param usage The manner in which the object is to be accessed.
    @return The things; may be the empty set.
    @throw CommandError on invalid request, not found, or insufficient privileges
    for a single-item predicate. */
PlaylistList AudioEngine::getPlaylistsOrCurrent (const PianodConnection &conn,
                                                 const Parsnip::Data &options,
                                                 CommandReply *diagnostics,
                                                 Ownership::Action usage) const {
    PlaylistList candidates;
    if (Predicate::havePredicate (options)) {
        candidates = Predicate::getSpecifiedPlaylists (conn, options);
    } else {
        candidates.push_back (getPlaylistOrCurrent (conn, options, usage));
    }
    return AuthorizedUse (candidates, conn.user, diagnostics, usage);
}

/** Get a playlist predicate, or use one implied by a thing.
    @param conn The connection requesting the things.
    @param options Predicate details from the command line.
    @param diagnostics Status aggregator for problems encountered.
    @param usage The manner in which the object is to be accessed.
    @param default_playlist A list of songs, from which playlist
    will be taken (all must have the same playlist).
    @return The things; may be the empty set.
    @throw CommandError on invalid request, not found, or insufficient privileges
    for a single-item predicate. */
PlaylistList AudioEngine::getPlaylistsOrCurrent (const PianodConnection &conn,
                                                 const Parsnip::Data &options,
                                                 CommandReply *diagnostics,
                                                 Ownership::Action usage,
                                                 const ThingieList &default_playlist) const {
    PlaylistList candidates;
    if (Predicate::havePredicate (options)) {
        candidates = Predicate::getSpecifiedPlaylists (conn, options);
    } else {
        // Use implied playlist from a list of things; all must be songs and all have the same playlist.
        PianodPlaylist *play = nullptr;
        if (default_playlist.size() > 0) {
            auto first = default_playlist.front()->asSong();
            if (first)
                play = first->playlist();
            if (play) {
                for (auto thing : default_playlist) {
                    auto song = thing->asSong();
                    if (!song)
                        throw CommandError (E_NO_ASSOCIATION, thing->id());
                    if (song->playlist() != play) {
                        throw CommandError (E_CONFLICT, "Songs have inconsistent playlists");
                    }
                }
            }
        }
        if (!play)
            throw CommandError (E_NO_ASSOCIATION);
        candidates.push_back (play);
    }

    return AuthorizedUse (candidates, conn.user, diagnostics, usage);
}

/** Add, remove, or toggle some seeds to/from some playlists.
    @param action Indicates add, remove or toggle.
    @param seeds The list of seeds to add/remove.
    @param targets the playlists to add/remove from.
    @param fixed_type If true, typecast seeds to `seed_type` when adding/removing.
    @param seed_type Type of seed to typecast to.
    @param diagnostics Status aggregator for problems encountered. */
static void seedAction (const CartsVerb action,
                        const ThingieList &seeds,
                        const PlaylistList &targets,
                        const bool fixed_type,
                        MusicThingie::Type seed_type,
                        CommandReply *diagnostics) {
    for (auto seed : seeds) {
        for (auto target : targets) {
            assert (target);
            assert (target->playlistType() == PianodPlaylist::SINGLE);
            MusicThingie *source_seed = seed;
            try {
                if (!fixed_type)
                    seed_type = seed->primaryType();
                if (seed->source() != target->source()) {
                    source_seed = target->source()->getSuggestion (
                            seed,
                            seed_type,
                            action == CartsVerb::ADD ? SearchRange::SHALLOW : SearchRange::KNOWN);
                }
                target->seed (seed_type,
                              source_seed,
                              action == CartsVerb::ADD      ? true
                              : action == CartsVerb::REMOVE ? false
                                                            :
                                                            /* Toggle */ !target->seed (seed_type, seed));
                diagnostics->succeed (seed, S_OK);
            } catch (const CommandError &err) {
                diagnostics->fail (seed, err);
            }
        }
    }
}

ResponseCollector AudioEngine::handleCommand (Parsnip::Parser::CommandId command,
                                              const Parsnip::Data &options,
                                              PianodConnection &conn) {
    assert (conn.source());
    assert (current_playlist);
    const Parsnip::Data &predicate = options.getOr (KEY_PREDICATE, EmptyDictionary);
    const Parsnip::Data &playlist_predicate = options.getOr (KEY_PLAYLIST, EmptyDictionary);

    // This first batch of commands may be used at anytime.
    switch (command) {
        case TIMESTATUS: {
            CommandReply response (NO_REPLY);
            response.information (gatherPlaybackStatus());
            return std::move (response);
        }
        case QUERYSTATUS: {
            if (conn.transmitJSON()) {
                CommandReply response;
                response.information (assembleStatus());
                response.information (conn.sendSelectedSource());
                return std::move (response);
            }
            DataResponse response;
            if (current_song) {
                response.data (current_song);
            }
            response.information (gatherQueueMode());
            response.information (gatherPlaybackStatus());
            response.information (gatherSelectedPlaylist());
            response.information (conn.sendSelectedSource());
            return std::move (response);
        }
        case QUERYHISTORY:
        case QUERYQUEUE:
            return sendSongLists (conn, options, command == QUERYHISTORY);
        case SETHISTORYSIZE: {
            history_size = options ["length"].asInteger();
            CommandReply response (S_OK);
            response.room_events (Response (I_HISTORYSIZE, history_size));
            return std::move (response);
        }
        case GETHISTORYSIZE:
            return Response (I_HISTORYSIZE, history_size);
        case WAITFORENDOFSONG:
            if (optionIs (options, "which", "current") && !current_song) {
                throw CommandError (E_WRONG_STATE);
            }
            conn.waitForEventWithOptions (WaitEvent::Type::TrackEnded,
                                          options.getOr (KEY_WAITOPTIONS, EmptyDictionary),
                                          this);
            return NO_REPLY;
        case WAITFORNEXTSONG:
            conn.waitForEventWithOptions (WaitEvent::Type::TrackStarted,
                                          options.getOr (KEY_WAITOPTIONS, EmptyDictionary),
                                          this);
            return NO_REPLY;
        case GETVOLUME:
            return Response (I_VOLUME, audio.volume);
        case SETVOLUME: {
            audio.volume = options ["level"].asInteger();
            if (player) {
                player->setVolume (audio.volume);
            }
            CommandReply response (S_OK);
            response.room_events (Response (I_VOLUME, audio.volume));
            response.room_events (Response (A_ADJUSTAUDIO, "volume"));
            return std::move (response);
        }
        case ADJUSTVOLUME: {
            int change = options.getOr ("change", 1);
            // Use a ±100 scale. 0 is "standard" level; >0 causes distortion.
            if (optionIs (options, "adjustment", "up")) {
                if (audio.volume >= 100) {
                    throw CommandError (E_RANGE, "Already at maximum volume");
                }
                audio.volume += change;
                if (audio.volume > 100) {
                    audio.volume = 100;
                }
            } else {
                if (audio.volume <= -100) {
                    throw CommandError (E_RANGE, "Already at minimum volume");
                }
                audio.volume -= change;
                if (audio.volume < -100) {
                    audio.volume = -100;
                }
            }
            if (player) {
                player->setVolume (audio.volume);
            }
            CommandReply response (S_OK);
            response.room_events (Response (I_VOLUME, audio.volume));
            response.room_events (Response (A_ADJUSTAUDIO, "volume"));
            return std::move (response);
        }
        case GETCROSSFADETIME:
            return Response (I_INFO, audio.crossfade_time, 1);
        case SETCROSSFADETIME: {
            AudioSettings result = audio;
            result.crossfade_time = options ["seconds"].asDouble();
            AudioOptions::validate (result);
            audio.crossfade_time = result.crossfade_time;
            CommandReply response (S_OK);
            response.room_events (Response (A_ADJUSTAUDIO, "crossfade duration"));
            return std::move (response);
        }
        case GETCROSSFADELEVEL:
            return Response (I_INFO, audio.crossfade_level, 1);
        case SETCROSSFADELEVEL: {
            audio.crossfade_level = options ["level"].asDouble();
            CommandReply response (S_OK);
            response.room_events (Response (A_ADJUSTAUDIO, "crossfade level"));
            return std::move (response);
        }
        case RESUMEPLAYBACK:
        case PAUSEPLAYBACK:
        case TOGGLEPLAYBACK: {
            CommandReply response (S_OK);
            response.room_events (playbackState (command == RESUMEPLAYBACK                  ? PlaybackState::PLAYING
                                                 : command == PAUSEPLAYBACK                 ? PlaybackState::PAUSED
                                                 : playback_state == PlaybackState::PLAYING ? PlaybackState::PAUSED
                                                                                            : PlaybackState::PLAYING,
                                                 &conn));
            return std::move (response);
        }
        case STOPPLAYBACK: {
            StopTime when = optionalValue (options, "when", StopTimes, StopTime::SONGEND);
            if (when == StopTime::NOW && player) {
                abort_playpoint = 1;
                player->abort();
            }
            CommandReply response (S_OK);
            response.room_events (queueMode (QueueMode::STOPPED, &conn));
            return std::move (response);
        }
        case PLAY:
        case SELECT:
            return std::move (
                    controlPlayback (options.getOr (KEY_PLAYBACKSELECTION, EmptyDictionary), command == PLAY, conn));
        case PLAYLISTRENAME: {
            auto play = Predicate::getSpecifiedPlaylist (conn, playlist_predicate);
            assert (play);

            if (!play->isEditableBy (conn.user)) {
                throw CommandError (E_UNAUTHORIZED);
            }
            std::string name = options ["newname"].asString();
            play->rename (name);
            CommandReply response (S_OK);
            response.broadcast_events (play, A_RENAMED_PLAYLIST);
            response.broadcast_events (V_PLAYLISTS_CHANGED);
            return std::move (response);
        }
        case PLAYLISTDELETE: {
            CommandReply response (CommandReply::Aggregation::OPTIMISTIC);
            auto playlists = AuthorizedUse (Predicate::getSpecifiedPlaylists (conn, playlist_predicate),
                                            conn.user,
                                            &response,
                                            Ownership::Action::ALTER);

            for (auto play : playlists) {
                try {
                    play->erase();
                    response.succeed (play, S_OK);
                } catch (const CommandError &err) {
                    response.fail (play, err);
                }
            }
            if (response.anySuccess()) {
                response.broadcast_events (A_DELETED_PLAYLIST);
                response.broadcast_events (V_PLAYLISTS_CHANGED);
            }
            mix.updatePlaylists();
            return std::move (response);
        }
        case RATE: {
            // Determine the assigned rating
            std::string rating_text = options ["rating"].asString();
            Rating rating = Rating::UNRATED;
            bool overplayed = (strcasecmp (rating_text, "overplayed") == 0);

            // Validating rating value before doing real work.
            if (!overplayed)
                rating = RATINGS [rating_text];

            CommandReply reply;
            SongList songs = getSongsOrCurrent (conn, predicate, &reply);
            for (auto song : songs) {
                RESPONSE_CODE status = E_BUG;
                try {
                    if (song->ratingScheme() == RatingScheme::NOBODY) {
                        if (song->isSeed() || song->isSuggestion()) {
                            status = E_WRONGTYPE;
                        } else {
                            status = E_MEDIA_ACTION;
                        }
                    } else if (song->ratingScheme() == RatingScheme::INDIVIDUAL && !conn.user) {
                        status = E_LOGINREQUIRED;
                    } else if (song->ratingScheme() != RatingScheme::INDIVIDUAL && !song->isEditableBy (conn.user)) {
                        status = E_UNAUTHORIZED;
                    } else if (overplayed) {
                        status = song->rateOverplayed (conn.user);
                    } else {
                        status = song->rate (rating, conn.user);
                        if (isSuccess (status)) {
                            if (song->ratingScheme() == RatingScheme::OWNER) {
                                reply.user_events (song, V_SONGRATING_CHANGED);
                            } else {
                                reply.broadcast_events (song, V_SONGRATING_CHANGED);
                            }
                            if (song == current_song.get()) {
                                sendUpdatedRatings (conn, song, &reply);
                            }
                        }
                    }
                    reply (song, status);
                } catch (const CommandError &err) {
                    reply.fail (song, err);
                }
            }
            return std::move (reply);
        }
        case RATEPLAYLIST: {
            Rating rating = RATINGS [options ["rating"].asString()];
            CommandReply reply;
            PlaylistList playlists = getPlaylistsOrCurrent (conn, playlist_predicate, &reply);

            for (auto play : playlists) {
                RESPONSE_CODE result = play->rate (rating, conn.user);
                if (isSuccess (result)) {
                    reply.user_events (play, V_PLAYLISTRATING_CHANGED);
                    mix.recalculatePlaylists();
                    track_acquisition_time = 0;
                }
                reply (play, result);
            }
            return std::move (reply);
        }
        case SEEDLIST: {
            PianodPlaylist *play = getPlaylistOrCurrent (conn, playlist_predicate, Ownership::Action::READ);
            assert (play);
            return constructSeedlist (play->getSeeds(), play);
        }
        case SEEDALTER: {
            // Determine kind of seed
            CartsVerb action = CartsWord [options [KEY_VERB].asString()];
            const char *type
                    = predicate.contains (PREDKEY_SEEDTYPE) ? predicate [PREDKEY_SEEDTYPE].asString().c_str() : nullptr;
            MusicThingie::Type seed_kind = MusicThingie::Type::Song;
            ThingieList seeds;
            CommandReply response;
            if (type) {
                // Acquire thing of necessary type, either current or specified
                seed_kind = THINGIETYPES [type];
                seeds = getThingsOrCurrent (conn, predicate, seed_kind, &response);
            } else {
                // Acquire thing and use implied type
                seeds = getThingsOrCurrent (conn, predicate, &response);
            }

            // Acquire the target: song's playlist, current playlist, or specified
            PlaylistList targets = getPlaylistsOrCurrent (conn,
                                                          predicate.getOr (KEY_PLAYLIST, playlist_predicate),
                                                          &response,
                                                          Ownership::Action::ALTER,
                                                          seeds);

            seedAction (action, seeds, targets, type, seed_kind, &response);
            return std::move (response);
        }
        case PLAYLISTCREATE: {
            if (!conn.source()->isEditableBy (conn.user)) {
                throw CommandError (E_UNAUTHORIZED);
            }

            CommandReply response (CommandReply::Aggregation::OPTIMISTIC);

            // Determine kind of starter seeds, then the seeds.
            // Although not usec to create a smart playlist, the first one
            // may be used to assign a name.
            const char *type = predicate.contains ("type") ? options ["type"].asString().c_str() : nullptr;
            MusicThingie::Type seed_kind = MusicThingie::Type::Song;
            ThingieList seeds;
            if (type) {
                // Acquire thing of necessary type, either current or specified
                seed_kind = THINGIETYPES [type];
                seeds = getThingsOrCurrent (conn, predicate, seed_kind, &response);
            } else {
                // Acquire thing and use implied type
                seeds = getThingsOrCurrent (conn, predicate, &response);
            }

            // Get and validate the playlist name
            bool smart = optionIs (options, "smart", "smart");
            std::string name = options.contains ("name") ? options ["name"].asString() : "";
            if (name.empty() && (smart || conn.source()->requireNameForCreatePlaylist())) {
                name = seeds.front()->name() + " Playlist";
            }
            if (!name.empty() && conn.source()->getPlaylistByName (name.c_str())) {
                throw CommandError (E_DUPLICATE);
            }

            PianodPlaylist *playlist;
            if (smart) {
                // Create a smart playlist.
                auto p = Predicate::getPredicate (conn, predicate);
                if (!p->canPersist()) {
                    throw CommandError (E_PERSISTENT);
                };
                playlist = conn.source()->createPlaylist (name.c_str(), *p);
                response.succeed (playlist, S_OK);
            } else {
                // Create a standard playlist.

                // Create the playlist with first item.
                MusicThingie *seed = seeds.front();
                seeds.pop_front();
                if (!type)
                    seed_kind = seed->primaryType();
                if (seed->source() != conn.source())
                    seed = conn.source()->getSuggestion (seed, seed_kind);
                playlist = conn.source()->createPlaylist (name.empty() ? nullptr : name.c_str(), seed_kind, seed);
                response (playlist, name.empty() || playlist->name() == name ? S_OK : S_ROUNDING);

                // Subsequently seeds are handled by seed_add.
                PlaylistList playlist_as_list;
                playlist_as_list.push_back (playlist);
                seedAction (CartsVerb::ADD, seeds, playlist_as_list, type, seed_kind, &response);
            }
            if (response.anySuccess()) {
                response.broadcast_events (playlist, A_CREATED_PLAYLIST);
                response.broadcast_events (playlist, V_PLAYLISTS_CHANGED);
                mix.updatePlaylists();
            }
            return std::move (response);
        }
        case PLAYLISTFROMFILTER: {
            if (!conn.source()->isEditableBy (conn.user)) {
                throw CommandError (E_UNAUTHORIZED);
            }
            Filter f (options ["expression"].asString());
            if (!f.canPersist()) {
                throw CommandError (E_PERSISTENT);
            };
            if (conn.source()->getPlaylistByName (options ["name"].asString().c_str())) {
                throw CommandError (E_DUPLICATE);
            }
            PianodPlaylist *playlist = conn.source()->createPlaylist (options ["name"].asString().c_str(), f);
            CommandReply response (S_OK);
            response.broadcast_events (playlist, A_CREATED_PLAYLIST);
            response.broadcast_events (playlist, V_PLAYLISTS_CHANGED);
            mix.updatePlaylists();
            return std::move (response);
        }
        case PLAYLISTALTERFILTER: {
            Filter f (options ["expression"].asString());
            if (!f.canPersist()) {
                throw CommandError (E_PERSISTENT);
            };
            PianodPlaylist *playlist = Predicate::getSpecifiedPlaylist (conn, playlist_predicate);
            assert (playlist);
            if (!playlist->isEditableBy (conn.user)) {
                throw CommandError (E_UNAUTHORIZED);
            }
            playlist->updateSelector (f);
            CommandReply response (S_OK);
        }
        case LISTSONGSBYPLAYLIST: {
            require (conn, REQUIRE_SOURCE | REQUIRE_EXPAND);
            PianodPlaylist *play = getPlaylistOrCurrent (conn, playlist_predicate, Ownership::Action::READ);
            // A predicate specifying a source could return a non-request playlist.
            if (!play->source()->canExpandToAllSongs()) {
                throw CommandError (E_MEDIA_ACTION, "Requests from specified source");
            }
            return play->songs();
        }
        case LISTSONGSBYFILTER:
            return Predicate::getSpecifiedSongs (conn, predicate, SearchRange::REQUESTS);
        case REQUESTMUSIC: {
            auto requested = Predicate::getSpecifiedSongs (conn, predicate, SearchRange::REQUESTS);
            SongList::size_type size = requested.size();
            requests.join (std::move (requested));
            CommandReply response (Response (S_MATCH, size));
            if (size > 1) {
                response.room_events (A_REQUEST_ADD);
            } else {
                response.room_events (requested.front(), A_REQUEST_ADD);
            }
            response.room_events (V_QUEUE_CHANGED);
            return std::move (response);
        }
        case REQUESTCLEAR: {
            CommandReply response;
            if (!requests.empty()) {
                requests.clear();
                response.room_events (A_REQUEST_CLEAR);
                response.room_events (V_QUEUE_CHANGED);
            }
            response.succeed();
            return std::move (response);
        }
        case REQUESTCANCEL: {
            auto filter = Predicate::getPredicate (conn, predicate);
            CommandReply response (CommandReply::Aggregation::OPTIMISTIC);
            SongList retained_requests;
            // Check cueing songs
            if (cueing_song && filter->matches (cueing_song.get())) {
                time_t when;
                if (cueing_song->canSkip (&when)) {
                    response.succeed (cueing_song, S_OK);
                    cueing_player->abort();
                } else {
                    response.fail (cueing_song, E_QUOTA);
                }
            }
            // Check the request queue
            retained_requests.reserve (requests.size());
            for (auto song : requests) {
                if (filter->matches (song)) {
                    response.succeed (song, S_OK);
                } else {
                    retained_requests.push_back (song);
                }
            }
            // Check the random queue
            SongList retained_random;
            for (auto song : random_queue) {
                if (filter->matches (song)) {
                    time_t when;
                    if (song->canSkip (&when)) {
                        response.succeed (song, S_OK);
                    } else {
                        response.fail (song, E_QUOTA);
                    }
                } else {
                    retained_random.push_back (song);
                }
            }
            if (response.noop()) {
                response.fail (E_NOTFOUND);
            } else if (response.anySuccess()) {
                requests = retained_requests;
                random_queue = retained_random;
                response.room_events (A_REQUEST_CANCEL);
                response.room_events (V_QUEUE_CHANGED);
            }
            return std::move (response);
        }
        case GETSUGGESTIONS: {
            SearchRange range = optionalValue (options, "range", SearchRanges, SearchRange::SHALLOW);
            return Predicate::getSpecifiedThings (conn, predicate, range);
        }
        case ALTERAUDIOCONFIG: {
            AudioSettings altered = audio;
            AudioOptions::extract_options (options [KEY_AUDIOOPTIONS], altered);
            audio = altered;
            CommandReply response (S_OK);
            response.room_events (Response (I_VOLUME, audio.volume));
            response.room_events (A_ADJUSTAUDIO);
            return std::move (response);
        }
#ifndef NDEBUG
        case FILTERECHO: {
            auto f = Predicate::getPredicate (conn, predicate);
            std::string expr = f->toString();
            return Response (I_INFO, expr);
        }
#endif
        default:
                /* Continue to next switch statement, after requirements check. */;
    }

    // From here on, commands require a player.
    require (conn, REQUIRE_PLAYER);
    assert (current_song);
    switch (command) {
        case NEXTSONG: {
            time_t next_skip = 0;
            CommandReply response;
            if (current_song->canSkip (&next_skip)) {
                float crossfade_time = audio.crossfade_time + audio.preroll_time;
                float remaining = player->playRemaining();
                if (remaining >= 0.0f && remaining < crossfade_time) {
                    crossfade_time = audio.crossfade_time;
                }
                SkipManner manner = optionalValue (options, "abrupt", SkipManners, SkipManner::NORMAL);
                if (audio.crossfade_time == 0.0f || remaining < crossfade_time
                                                 || playback_state == PlaybackState::PAUSED
                                                 || abort_playpoint != 0 || manner == SkipManner::ABRUPT) {
                    player->abort();
                    abort_playpoint = 1;
                } else {
                    // Crossfade the skip
                    abort_playpoint = player->playPoint() + crossfade_time;
                }
                response.succeed();
                response.room_events (A_SKIPPED, current_song);
            } else {
                response.fail (E_QUOTA);
                if (next_skip) {
                    std::ostringstream out;
                    next_skip -= time (nullptr);
                    out << "Next skip in " << (next_skip / 60) << ":" << std::setfill ('0') << std::setw (2)
                        << (next_skip / 60);
                    response.information (V_SOURCE_STATUS, out.str());
                }
            }
            return std::move (response);
        }
        default:
            flog (LOG_WHERE (Log::WARNING), "Unimplemented command ", command);
            throw CommandError (E_NOT_IMPLEMENTED, std::to_string (command));
    }
}

/* Send the current status, including:
    - Playing, paused, stopped, between tracks
    - Track duration, Current playhead position, track remaining if playing or paused
    @param there The destination of the data.
    @param only_if_accurate Only send data if playpoint and duration are accurate.
    If not, defer output and return false.
    @return true if the data reported was complete, false if playpoint or duration
    were not available and reported as 0. */
bool AudioEngine::gatherPlaybackStatus (ResponseGroup *response, bool only_if_accurate) const {
    bool data_valid = true;
    if (player && player->ready()) {
        RESPONSE_CODE state = (playback_state == PlaybackState::PAUSED ? V_PAUSED
                               : stall.onset                           ? V_STALLED
                                                                       : V_PLAYING);

        float duration_float = player->trackDuration();
        float playpoint_float = player->playPoint();
        data_valid = (duration_float >= 0 && playpoint_float >= 0);

        if (duration_float < 0)
            duration_float = 0;
        if (playpoint_float < 0)
            playpoint_float = 0;

        if (only_if_accurate && !data_valid)
            return false;

        // clang-format on
        Parsnip::Data json_info{ Parsnip::Data::Dictionary,
                                 JSON::Key::PlayDuration,
                                 duration_float,
                                 JSON::Key::PlayPoint,
                                 nullptr,
                                 JSON::Key::PlayRemaining,
                                 nullptr };
        // clang-format on

        if (data_valid) {
            json_info [JSON::Key::PlayPoint] = playpoint_float;
            json_info [JSON::Key::PlayRemaining] = duration_float - playpoint_float;
        }

        long duration = duration_float;
        long playpoint = playpoint_float;
        long song_remaining = data_valid ? player->playRemaining() : 0;

        bool remaining_is_negative = song_remaining < 0;
        if (remaining_is_negative) {
            // song is longer than expected
            song_remaining = -song_remaining;
        }

        std::ostringstream time_info;
        time_info << format_duration (playpoint, 2) << '/' << format_duration (duration, 2) << '/'
                  << (remaining_is_negative ? '+' : '-') << format_duration (song_remaining, 2);
        (*response) (state, Response::List{ time_info.str() }, std::move (json_info));
    } else {
        (*response) ((playback_state == PlaybackState::PAUSED || (queue_mode == QueueMode::REQUESTS && requests.empty())
                      || queue_mode == QueueMode::STOPPED)
                             ? V_IDLE
                             : V_BETWEEN_TRACKS);
    }
    return data_valid;
}

ResponseGroup AudioEngine::gatherPlaybackStatus() const {
    ResponseGroup response;
    gatherPlaybackStatus (&response);
    return response;
}

/** Pause or resume playback.
    @param state Indicates whether to play or pause.
    @param conn Connection requesting change, or nullptr. */
ResponseGroup AudioEngine::playbackState (PlaybackState state, PianodConnection *conn) {
    ResponseGroup response;
    if (playback_state != state) {
        playback_state = state;

        // Apply that state to the player.
        if (player) {
            if (playback_state == PlaybackState::PLAYING) {
                player->play();
                if (cueing_player && transition_state >= TransitionProgress::Crossfading)
                    cueing_player->play();
            } else {
                player->pause();
                if (cueing_player)
                    cueing_player->pause();
            }
            if (playback_state == PlaybackState::PAUSED && !pause_expiration) {
                pause_expiration = (pause_timeout ? time (nullptr) + pause_timeout : FAR_FUTURE);
            } else if (playback_state == PlaybackState::PLAYING) {
                pause_expiration = 0;
                stall.playback_effective_start = time (nullptr) - (long) (player->playPoint());
                stall.onset = 0;
                stall.onset_playpoint = 0;
            }
            if (conn) {
                response (playback_state == PlaybackState::PLAYING ? A_RESUMED : A_PAUSED);
            }
        } else if (playback_state == PlaybackState::PLAYING && queue_mode != QueueMode::STOPPED) {
            if (conn) {
                response (A_RESUMED);
            }
        }
        response (gatherPlaybackStatus());
    }
    return response;
}

/** Set the queueing mode.
    @param mode Indicates stopped, requests only, or random play.
    @param conn Connection requesting change, or nullptr. */
ResponseGroup AudioEngine::queueMode (QueueMode mode, PianodConnection *conn) {
    ResponseGroup response;
    if (mode != queue_mode) {
        queue_mode = mode;
        response (gatherQueueMode());
        if (conn) {
            response (mode == QueueMode::STOPPED ? A_STOPPED : mode == QueueMode::REQUESTS ? A_REQUESTS : A_RANDOMPLAY);
        }
    }
    return response;
}

Response AudioEngine::gatherQueueMode() const {
    return (queue_mode == QueueMode::STOPPED    ? V_QUEUE_STOPPED
            : queue_mode == QueueMode::REQUESTS ? V_QUEUE_REQUEST
                                                : V_QUEUE_RANDOM);
}

Response AudioEngine::gatherSelectedPlaylist() const {
    const char *mode = "bug"; // NOLINT (dead-initialization)
    assert (current_playlist);
    switch (current_playlist->playlistType()) {
        case PianodPlaylist::EVERYTHING:
            mode = "everything";
            break;
        case PianodPlaylist::MIX:
            mode = mix.automatic() ? "auto" : "mix";
            break;
        case PianodPlaylist::SINGLE:
        case PianodPlaylist::TRANSIENT:
            mode = "playlist";
            break;
        default:
            assert (!"Switch/case unmatched");
            break;
    }

    return Response (current_playlist,
                     V_SELECTEDPLAYLIST,
                     Response::List{ mode, current_playlist->playlistName() },
                     std::move (Response::NoJsonData));
}

/** Send audio engine status to a client.  Used on login. */
ResponseGroup AudioEngine::assembleStatus() {
    ResponseGroup info;
    info (I_VOLUME, audio.volume);
    info (gatherSelectedPlaylist());
    info (gatherQueueMode());
    if (current_song) {
        info (current_song, I_ATTACHED_THING);
    }
    info (gatherPlaybackStatus());
    info (mix.assembleStatus());
    return info;
}

/** Update audio engine status to a client.  Used on authentication. */
ResponseGroup AudioEngine::updateStatus (PianodConnection &there) {
    ResponseGroup update;
    if (current_song) {
        update (current_song, I_ATTACHED_THING);
    }
    return update;
}

UserList AudioEngine::getAutotuneUsers() {
    return mix.getAutotuneUsers();
};
