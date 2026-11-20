///
/// Audio engine -- manage current audio player and sources.
/// @file       engine.h - pianod
/// @author     Perette Barella
/// @date       2014-11-30
/// @copyright  Copyright (c) 2014-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <ctime>

#include "interpreter.h"
#include "musictypes.h"
#include "retainer.h"
#include "retainedlist.h"
#include "connection.h"
#include "response.h"
#include "mediaunit.h"
#include "tuner.h"

/** Audio engine, responsible for one "room" worth of audio.
    Each audio engine has its own queue, selected playlist(s),
    and playback status.
 */
class AudioEngine : public PianodInterpreter {
private:
    /// Prerequisites that may be necessary to execute a particular command.
    typedef enum requirement_t {
        REQUIRE_SOURCE = 0x01, ///< Marks other flags as source-related
        REQUIRE_PLAYLIST = 0x02, ///< Marks other flags as playlist-related.
        REQUIRE_PLAYER = 0x04, ///< Require an audio player exist
        REQUIRE_EXPAND = 0x10, ///< Require expand to song list capability
    } REQUIREMENT;
    
    enum class PlaybackState {
        PAUSED,
        PLAYING
    };

    /// States player progresses through when segueing between tracks.
    enum class TransitionProgress {
        Playing,    ///< the audio player is not transitioning
        Purged,     ///< the queue has een purged in preparation for end-of-song
        Cueing,     ///< a second audio player has been initialized and is initializing/prefetching.
        Crossfading,///< the audio players are both playing as audio is cross-faded
        Done        ///< transition is done, but old song keeps right on going.
    };

    /// Behaviors possible when the player is idle.
    enum class QueueMode {
        STOPPED,        ///< Don't start playing anything.
        REQUESTS,       ///< Play only requested songs
        RANDOMPLAY,     ///< Random selections from the current source if no requests.
    };

    /// Whether and how completely track information was announced at start of playback.
    enum class Announced {
        NEVER,          // Track was never announced
        PARTIALLY,      // Track was announced but playpoint/duration incomplete.
        COMPLETELY      // Track was announced with full and accurate data.
    };

    /// Structure used to track player progress/status.
    struct {
        time_t playback_effective_start = 0; ///< When not stalled, current time minus playback point.
        time_t onset = 0; ///< Clock time at which a playback stall was detected.
        time_t onset_playpoint = 0; ///< Playback point at which a playback stall was detected.
    } stall;

    static const int pause_timeout = 1800; ///< Pause timeout applied to all sources/songs
    static const int aquire_tracks_retry = 600; ///< Retry period if unable to get random tracks.

    const float prefetch_time = 5; ///< Number of seconds before cuing at which queue is refreshed.

    SongList::size_type history_size = 10; ///< Maximum number of tracks retained in history.

    volatile bool quit_requested = false; ///< Flag for signal handlers to request shutdown.
    bool quit_initiated = false; ///< Flag set once a shutdown request has been initiated.

    time_t track_acquisition_time = 0; ///< The next time at which random track aquisition may be attempted.

    PianodService *service = nullptr;

    Retainer <PianodSong *> current_song = nullptr;
    Retainer <PianodSong *> cueing_song = nullptr;
    SongList requests;
    SongList random_queue;
    SongList song_history;

    bool lockout_announced = false;
    Retainer <PianodPlaylist *> current_playlist = nullptr;
    Media::Player *player = nullptr;
    Media::Player *cueing_player = nullptr;
    TransitionProgress transition_state = TransitionProgress::Playing;

    Announced track_announced = Announced::NEVER;
    PlaybackState playback_state = PlaybackState::PAUSED;
    QueueMode queue_mode = QueueMode::STOPPED;
    time_t pause_expiration = 0;
    signed long abort_playpoint = 0; ///< Playpoint where crossfaded skip will abort, or 0 for not aborted.
    bool empty_warning_given = false;

    AudioSettings audio;
    Tuner::Tuner mix;

    // Standard parser things
public:
    static const Parsnip::OptionParser::Definitions &playback_selection_option_definitions();
    static const Parsnip::Parser::Definitions &parser_definitions();
    virtual void registerInterpreter (PianodDispatcher &dispatcher) override;
    virtual const Parsnip::Parser::Definitions &getParserDefinitions () override;
    static const PianodSchema::CommandIds &json_request_names ();
private:
private:
    virtual bool authorizedCommand (Parsnip::Parser::CommandId command, PianodConnection &conn) override;
    virtual ResponseCollector handleCommand (Parsnip::Parser::CommandId command, const Parsnip::Data &options, PianodConnection &conn) override;

    // Interpreter helpers
    MusicThingie *getThingOrCurrent (const PianodConnection &conn,
                                     const Parsnip::Data &options,
                                     Ownership::Action usage = Ownership::Action::USE) const;
    MusicThingie *getThingOrCurrent (const PianodConnection &conn,
                                     const Parsnip::Data &options,
                                     MusicThingie::Type want,
                                     Ownership::Action usage = Ownership::Action::USE) const;
    ThingieList getThingsOrCurrent (const PianodConnection &conn,
                                    const Parsnip::Data &options,
                                    CommandReply *diagnostics,
                                    Ownership::Action usage = Ownership::Action::USE) const;
    ThingieList getThingsOrCurrent (const PianodConnection &conn,
                                    const Parsnip::Data &options,
                                    MusicThingie::Type want,
                                    CommandReply *diagnostics,
                                    Ownership::Action usage = Ownership::Action::USE) const;
    SongList getSongsOrCurrent (const PianodConnection &conn,
                                const Parsnip::Data &options,
                                CommandReply *diagnostics,
                                Ownership::Action usage = Ownership::Action::USE) const;
    PianodPlaylist *getPlaylistOrCurrent (const PianodConnection &conn,
                                          const Parsnip::Data &options,
                                           Ownership::Action usage) const;
    PlaylistList getPlaylistsOrCurrent (const PianodConnection &conn,
                                        const Parsnip::Data &options,
                                        CommandReply *diagnostics,
                                        Ownership::Action usage = Ownership::Action::USE) const;
    PlaylistList getPlaylistsOrCurrent (const PianodConnection &conn,
                                        const Parsnip::Data &options,
                                        CommandReply *diagnostics,
                                        Ownership::Action usage,
                                        const ThingieList &default_playlist) const;
    void require (PianodConnection &conn, unsigned long requirements) const;
    CommandReply controlPlayback (const Parsnip::Data &options, bool start_playback, PianodConnection &conn);


    // Internal routines
    inline bool queueEmpty (void) const {
        return requests.empty() && random_queue.empty();
    }
    bool startPlayer (void);
    ResponseGroup promotePlayer (void);
    ResponseGroup cleanupPlayer (void);
    float monitorPlayer (void);

    // Status and other senders
    [[nodiscard]] ResponseGroup playbackState (PlaybackState state, PianodConnection *conn = nullptr);
    [[nodiscard]] ResponseGroup queueMode (QueueMode mode, PianodConnection *conn = nullptr);
    bool gatherPlaybackStatus (ResponseGroup *response, bool only_if_accurate = false) const;
    [[nodiscard]] ResponseGroup gatherPlaybackStatus () const;
    [[nodiscard]] Response gatherQueueMode () const;
    [[nodiscard]] Response gatherSelectedPlaylist () const;

    /// Generate response messages
    SongList sendSongLists (PianodConnection &conn, const Parsnip::Data &options, bool historical);
    static DataResponse constructSeedlist (const ThingieList &things, PianodPlaylist *playlist);


    // Helper methods
    void PurgeUnselectedSongs (void);
    bool considerCreatingPlayer (void);
    bool acquireRandomTracks (void);

    // Callback methods and handlers
    void sourceReady (const Media::Source * const );
    bool sourceRemovalCheck (const Media::Source * const source);
    void sourceRemoved (const Media::Source * const source);
    void sourceOffline (const Media::Source * const );
    void sourceStatus (RESPONSE_CODE status, const char *detail);

    void playlistsChanged ();
    void mixChanged (bool automatic, const char *why);

public:
    AudioEngine (PianodService *svc, const AudioSettings &audio_options);
    virtual ~AudioEngine (void);
    void shutdown (bool immediate);
    float periodic(void); /// Should be called intermittently by the main run-loop
    ResponseGroup assembleStatus ();
    ResponseGroup updateStatus (PianodConnection &there);
    void usersChangedNotification();
    UserList getAutotuneUsers ();
    inline const AudioSettings &audioSettings () { return audio; };
};
