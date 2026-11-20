///
/// Common methods for media sources that rely on C++11 threads.
/// @file       threadedmediaplayer.cpp - pianod project
/// @author     Perette Barella
/// @date       2014-10-26
/// @copyright  Copyright 2012-2021 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cstdio>
#include <cstdint>
#include <cstring>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>

#include "fundamentals.h"
#include "mediaplayer.h"
#include "logging.h"


namespace Media {

    // Inform the player thread that it should pause/resume/quit.
    void ThreadedPlayer::setThreadState (bool pause, bool quit) {
        std::unique_lock<std::mutex> lock (pause_mutex);
        if (quit)
            do_quit = true;
        do_pause = do_quit ? false : pause;
        pause_changed.notify_all();
    }

    void ThreadedPlayer::pause (void) {
        setThreadState (true, false);
    };
    
    void ThreadedPlayer::abort () {
        setThreadState (false, true);
    }

    bool ThreadedPlayer::checkForPauseOrQuit (void) {
        bool quit = false;

        std::unique_lock<std::mutex> lock (pause_mutex);
        while (true) {
            if (do_quit) {
                // To avoid headaches, no partial initialization abortions. 
                if (ready ()) {
                    quit = true;
                }
                break;
            }
            if (!do_pause) {
                break;
            }
            pausing();
            // Note: condition variable releases the mutex while waiting
            pause_changed.wait (lock);
            resuming();
        }
        return quit;
    };

    /** Start the player thread.  Starts in a paused state. */
    void ThreadedPlayer::cue (void) {
        std::packaged_task<RESPONSE_CODE (void)> package (std::bind (&ThreadedPlayer::playerThread, this));
        player_response = package.get_future();
        std::thread player (std::move (package));
        player_thread = std::move (player);
    }
    /** Calls resume() to starting playback. */
    void ThreadedPlayer::play (void) {
        setThreadState (false, false);
    }
    /** Get the player completion status, when it is done.
        @throw If the player thread threw an exception, the promise/
        future holds it and get() rethrows it when asked for it. */
    RESPONSE_CODE ThreadedPlayer::completionStatus() {
        assert (player_thread.joinable());
        assert (player_response.valid());
        player_response.wait();
        RESPONSE_CODE response = player_response.get();
        return response;
    }
    /** Called when the playback thread is pausing. */
    void ThreadedPlayer::pausing (void) {
    }
    /** Called when the playback thread is resuming playback. */
    void ThreadedPlayer::resuming (void) {
    }

    ThreadedPlayer::ThreadedPlayer (void) {
    }
    
    ThreadedPlayer::~ThreadedPlayer (void) {
        if (player_thread.joinable()) {
            player_thread.join();
        }
    }
}

