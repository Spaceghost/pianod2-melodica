///
/// Public interface for media players.
/// Media players are responsible for playing audio.
/// @file       mediaplayer.h - pianod project
/// @author     Perette Barella
/// @date       2015-02-04
/// @copyright  Copyright 2012–2020 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <ctime>

#include <thread>
#include <mutex>
#include <condition_variable>
// #include <tardis>
#include <future>

#include "fundamentals.h"

#ifdef WITH_AVFOUNDATION
// This should be in osxplayer.h, but needs to be in a more public header file.
extern void wrapInNSApplication (void real_application(void));
#endif

namespace Media {
    /// Initialize the media engines; uninitialize on destruction.
    class Initializer {
    public:
        Initializer ();
        ~Initializer ();
    };

    extern void reportLibrariesAndVersions(int verbose);

    /** Base class for playing audio from some source.
        Use as follows:
        - Construct the player.
        - cue() to start buffering media stream, open outputs, and get ready to play.
        - play() when it is time to start playing.
        - Use other functions to monitor or interact with player.
        - When playbackComplete(), it is safe to destruct the player. */
    class Player {
    public:
        typedef enum {
            DoesNotExist, ///< Player doesn't exist.
            Initializing, ///< Player has been created but isn't doing anything yet
            Cueing,  ///< Player has been created and is ready to go, but not playing.
            Playing, ///< Player is playing
            Done ///< Playback has completed or been aborted
        } State;

    public:
        virtual ~Player (void);
        // Regulate playback
        virtual void pause (void) = 0; ///< Pause playback.  If already paused, do nothing.
        virtual void abort (void) = 0; ///< Don't play the rest of the song.
        virtual void setVolume (float volume) = 0; ///< Adjust volume in decibels.
        virtual void cue (void) = 0; ///< Starts thread in paused mode
        virtual void play (void) = 0; ///< Start playback or resume playback if paused.  If already playing, continue.

        // Data retrieval functions
        virtual float trackDuration (void) const = 0; ///< Length of track in seconds, or a negative number if unknown.
        virtual float playPoint (void) const = 0; ///< Seconds from start, or a negative value if unknown.
        virtual float playRemaining (void) const; ///< Seconds left in playback, or a negative number if unknown
        virtual RESPONSE_CODE completionStatus (void) = 0;
        
        virtual State currentState (void) const = 0; /// @see State
        /// Indicate if player has finished initializing
        inline bool ready(void) const {
            return (currentState() >= Cueing);
        };
        /// Indicate if playback is complete and ready for cleanup
        inline bool playbackComplete (void) const {
            return (currentState() == Done);
        };

        /// When paused, get the time at which the song will expire.
        virtual time_t getPauseTimeout (void);

        static Media::Player *getPlayer (const AudioSettings &settings,
                                         const std::string &media_url,
                                         float initial_gain);
    };

    /// Base class for audio players that are spawned to a new thread.
    class ThreadedPlayer: public Player {
    private:
        volatile bool do_quit = false; ///< protected by pauseMutex
        volatile bool do_pause = true; ///< protected by pauseMutex
        std::mutex pause_mutex;
        std::condition_variable pause_changed;

        std::thread player_thread;
        std::future<RESPONSE_CODE> player_response;
        void setThreadState (bool pause, bool quit);

        /// Implementations
        virtual void pause (void)override; ///< Pause playback.  If already paused, do nothing.
        virtual void abort ()override; ///< Don't play the rest of the song.

        virtual void cue (void) override; ///< Starts thread in paused mode
        virtual void play (void) override;
        virtual RESPONSE_CODE completionStatus (void) override;

    protected:
        bool checkForPauseOrQuit (void);
        virtual RESPONSE_CODE playerThread (void) = 0;

    public:

        virtual void pausing (void); ///< Invoked when pausing.
        virtual void resuming (void); ///< Invoked when when resuming.

        ThreadedPlayer (void);
        virtual ~ThreadedPlayer (void);
    };
}

