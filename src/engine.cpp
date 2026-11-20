///
/// Audio engine -- manage current audio player and sources.
/// The audio engine creates, destroys, and selects audio sources.
/// It creates players and manages their state, cleans up after
/// them, and starts new players when required.
/// @file       engine.cpp - pianod
/// @author     Perette Barella
/// @date       2014-11-30
/// @copyright  Copyright 2014-2023 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <stdexcept>
#include <functional>

#include <football/football.h>

#include "fundamentals.h"
#include "engine.h"

#include "connection.h"
#include "response.h"
#include "servicemanager.h"
#include "mediaunit.h"
#include "mediaplayer.h"
#include "mediamanager.h"
#include "audio/audiooutput.h"
#include "user.h"
#include "users.h"


/** Create a new audio player attached to a service
    with an assigned audio output device.
    @param svc The Football service.
    @param audio_options The audio device settings. */
AudioEngine::AudioEngine (PianodService *svc, const AudioSettings &audio_options) : mix (svc) {
    service = svc;
    audio = audio_options;

    Media::Manager::Callbacks callbacks;
    callbacks.sourceReady = std::bind (&AudioEngine::sourceReady, this, std::placeholders::_1);
    callbacks.sourceOffline = std::bind (&AudioEngine::sourceOffline, this, std::placeholders::_1);
    callbacks.canRemoveSource = std::bind (&AudioEngine::sourceRemovalCheck, this, std::placeholders::_1);
    callbacks.sourceRemoved = std::bind (&AudioEngine::sourceRemoved, this, std::placeholders::_1);
    callbacks.statusNotification
            = std::bind (&AudioEngine::sourceStatus, this, std::placeholders::_1, std::placeholders::_2);
    media_manager->callback.subscribe (this, callbacks);

    try {
        Tuner::Tuner::Callbacks tuner_callbacks;
        tuner_callbacks.mixChanged
                = std::bind (&AudioEngine::mixChanged, this, std::placeholders::_1, std::placeholders::_2);
        tuner_callbacks.playlistsChanged = std::bind (&AudioEngine::playlistsChanged, this);
        mix.callback.subscribe (this, tuner_callbacks);

        current_playlist = media_manager->getMixPlaylist();
    } catch (...) {
        media_manager->callback.unsubscribe (this);
        throw;
    }
}

AudioEngine::~AudioEngine (void) {
    assert (!current_song);
    assert (!cueing_song);
    assert (!player);
    assert (!cueing_player);
    assert (current_playlist);

    mix.callback.unsubscribe (this);
    media_manager->callback.unsubscribe (this);
}

/** Initiate shutdown of the audio engine.
    @param immediate If true, aborts playback.
    If false, allows playback to complete before shutdown. */
void AudioEngine::shutdown (bool immediate) {
    if (player && immediate) {
        player->abort();
        abort_playpoint = 1;
    }
    quit_requested = true;
    service->announceToRoom (queueMode (QueueMode::STOPPED));
}

/** Start playback of a the next song in the queue.
    Preconditons: playlist should have a list in it.
    @return True on player started, false on error. */
bool AudioEngine::startPlayer (void) {
    // Get a song off the playlist
    assert (!cueing_song);
    assert (!cueing_player);
    assert (!queueEmpty());

    if (!requests.empty()) {
        cueing_song = requests.front();
        requests.pop_front();
    } else if (!random_queue.empty()) {
        cueing_song = random_queue.front();
        random_queue.pop_front();
    }

    // Create a player and start it cueing
    std::string message = "Could not start playback";
    RESPONSE_CODE reason = F_FAILURE;
    try {
        cueing_player = cueing_song->play (audio);
        if (cueing_player) {
            // setup player
            flog (LOG_WHERE (Log::QUEUE),
                  "Room ",
                  service->roomName(),
                  " cued up ",
                  cueing_song->title(),
                  " (",
                  cueing_song->id(),
                  ")");
            cueing_player->cue();
            return true;
        }
    } catch (const std::bad_alloc &) {
        reason = F_RESOURCE;
    } catch (const Audio::AudioException &e) {
        reason = F_AUDIO_FAILURE;
        message = e.what();
    } catch (const std::exception &e) {
        message = e.what();
    }
    ResponseGroup announcements;
    announcements (reason, message);
    announcements (V_QUEUE_CHANGED);

    // Update source playback statistics.
    cueing_song->source()->playbackComplete (false, false);
    cueing_song->source()->playbackProblem();
    cueing_song = nullptr;

    // We'll try the next queue item song on the next iteration
    // But if there's not something up next, report we're going to idle.
    if (queue_mode == QueueMode::STOPPED || playback_state == PlaybackState::PAUSED
        || (queue_mode == QueueMode::REQUESTS && requests.empty())) {
        gatherPlaybackStatus (&announcements);
    }
    service->announceToRoom (std::move (announcements));
    return false;
}

/// Promote the cueing player to the primary player. */
ResponseGroup AudioEngine::promotePlayer (void) {
    assert (player == nullptr);
    assert (!current_song);

    std::swap (player, cueing_player);
    std::swap (current_song, cueing_song);
    ResponseGroup announcements;
    if (player) {
        assert (current_song);
        player->setVolume (audio.volume);
        if (playback_state == PlaybackState::PLAYING)
            player->play();
        stall = {};
        abort_playpoint = 0;
        track_announced = Announced::NEVER;
        transition_state = TransitionProgress::Playing;
        pause_expiration = 0;
        empty_warning_given = false;
        announcements (current_song, I_ATTACHED_THING);
    }
    return announcements;
}

/// Clean up when a player completes.
ResponseGroup AudioEngine::cleanupPlayer (void) {
    assert (current_song);
    assert (player);

    bool success = false;
    std::string completion_status = "success";
    ResponseGroup announcements;
    try {
        RESPONSE_CODE status = player->completionStatus();
        assert (status == S_OK || isServerFailure (status));
        if (status == S_OK) {
            success = true;
        } else {
            announcements (status);
        }
    } catch (const std::bad_alloc &e) {
        announcements (E_RESOURCE);
        completion_status = "out of memory";
    } catch (const Audio::AudioException &e) {
        announcements (F_AUDIO_FAILURE, e.what());
        completion_status = e.what();
    } catch (const std::exception &e) {
        announcements (F_EXCEPTION, e.what());
        completion_status = e.what();
    }

    // Update source playback statistics.
    current_song->source()->playbackComplete (track_announced >= Announced::NEVER, success);
    if (track_announced == Announced::NEVER || !success) {
        current_song->source()->playbackProblem();
    }

    // Report time for any stall before player exited
    if (stall.onset) {
        flog (LOG_WHERE (Log::WARNING|Log::AUDIO), "Playback stalled for ", time (NULL) - stall.onset, " seconds");
    }
    delete player;
    player = NULL;

    flog (LOG_WHERE (Log::QUEUE),
          "Room ",
          service->roomName(),
          " reaped ",
          current_song->title(),
          " (",
          current_song->id(),
          "): ",
          completion_status);

    // Move the completed song to the history, then trim history
    song_history.push_front (current_song.get());
    current_song = nullptr;
    while (song_history.size() > history_size) {
        song_history.pop_back();
    }

    announcements (V_TRACK_COMPLETE);
    // If there's not something up next, report we're going to idle.
    if (queue_mode == QueueMode::STOPPED || playback_state == PlaybackState::PAUSED
        || (queue_mode == QueueMode::REQUESTS && requests.empty())) {
        gatherPlaybackStatus (&announcements);
    }
    service_manager->event (WaitEvent::Type::TrackEnded, this);
    return announcements;
}

/// Check/respond to various things with the player
float AudioEngine::monitorPlayer (void) {
    assert (player);
    assert (player->ready());
    long playpoint = player->playPoint();

    ResponseGroup status_messages;
    if (track_announced != Announced::COMPLETELY) {
        if (playpoint >= 0 && !stall.playback_effective_start) {
            stall.playback_effective_start = time (nullptr) - playpoint;
        }
        /* Broadcast status after the start of the song.  Each of these would
         better fit elsewhere, but for various reasons can't be there. */
        bool fully_done = gatherPlaybackStatus (&status_messages, track_announced == Announced::PARTIALLY);
        track_announced = fully_done ? Announced::COMPLETELY : Announced::PARTIALLY;
        service_manager->event (WaitEvent::Type::TrackStarted, this);
        // Update start time to avoid spurrious stall warnings
    }
    time_t now = time (nullptr);
    if (playback_state == PlaybackState::PLAYING && stall.playback_effective_start && playpoint >= 0) {
        // Check for/announce/track stalls
        long jitter = (now - stall.playback_effective_start) - playpoint;
        if (jitter > +2) {
            // We are or have been stalled
            if (stall.onset == 0) {
                // This is a fresh stall
                stall.onset = now - jitter;
                stall.onset_playpoint = playpoint;
                gatherPlaybackStatus (&status_messages);
            } else if (stall.onset_playpoint != playpoint) {
                // Player has recovered and is making progress.
                stall.playback_effective_start = now - playpoint;
                flog (LOG_WHERE (Log::WARNING|Log::AUDIO), "Playback stalled for ", now - stall.onset, " seconds");
                stall.onset = 0;
                stall.onset_playpoint = 0;
                gatherPlaybackStatus (&status_messages);
            }
        } else if (jitter < -2) {
            flog (LOG_WHERE (Log::WARNING|Log::AUDIO), "Jitter ", jitter, "; song playing faster than expected.");
            stall.playback_effective_start = now - playpoint;
        }
    }
    service->announceToRoom (std::move (status_messages));

    if (playback_state == PlaybackState::PLAYING) {
        // Anticipate end of song
        float song_remaining = player->playRemaining();
        if (abort_playpoint) {
            song_remaining = abort_playpoint - player->playPoint();
            if (song_remaining < 0.0f) {
                player->abort();
            }
        }
        float next_check;
        switch (transition_state) {
            case TransitionProgress::Playing:
                if (song_remaining < 0.0f) {
                    next_check = 1.0f;
                    break;
                }
                next_check = song_remaining - (audio.crossfade_time + audio.preroll_time + prefetch_time);
                if (next_check > 0.0f)
                    break;
                if (queue_mode != QueueMode::STOPPED) {
                    PurgeUnselectedSongs();
                    if ((queue_mode == QueueMode::RANDOMPLAY) && queueEmpty())
                        acquireRandomTracks();
                    transition_state = TransitionProgress::Purged;
                }
                // FALLTHRU
            case TransitionProgress::Purged:
                if (audio.preroll_time == 0.0f) {
                    next_check = song_remaining;
                    break;
                }
                next_check = song_remaining - (audio.crossfade_time + audio.preroll_time);
                if (next_check > 0)
                    break;
                considerCreatingPlayer();
                if (!cueing_player)
                    break;
                transition_state = TransitionProgress::Cueing;
                // FALLTHRU
            case TransitionProgress::Cueing:
                assert (cueing_player);
                next_check = song_remaining - audio.crossfade_time;
                if (next_check > 0 || audio.crossfade_time < 0.2f)
                    break;
                cueing_player->setVolume (audio.volume - audio.crossfade_level);
                cueing_player->play();
                transition_state = TransitionProgress::Crossfading;
                // FALLTHRU;
            case TransitionProgress::Crossfading: {
                next_check = 0.1f;
                // Don't die if the crossfade parameters were changed while crossfading
                if (audio.crossfade_time < 0.2f)
                    break;
                float fade_portion_left = song_remaining / audio.crossfade_time;
                if (fade_portion_left > 1) {
                    // Parameter got changed while crossfading
                    fade_portion_left = 1.0f;
                } else if (fade_portion_left < 0) {
                    // Wonky audio file playing overtime
                    fade_portion_left = 0.0f;
                    transition_state = TransitionProgress::Done;
                }
                player->setVolume (float (audio.volume) - audio.crossfade_level * (1.0f - fade_portion_left));
                cueing_player->setVolume (float (audio.volume) - audio.crossfade_level * fade_portion_left);
                break;
            }
            case TransitionProgress::Done:
                // Nothing to do.
                // If song goes into serious overtime, the expected duration is wrong.
                // Stop checking it so often.
                next_check = (song_remaining < -1.1f ? 1.0f : 0.1f);
                break;
        }
        return (next_check > 5.0f ? 5.0f : next_check < 0.0f ? 0.3f : next_check);
    }
    assert (playback_state == PlaybackState::PAUSED);
    if (abort_playpoint || now > pause_expiration || current_song->expired()) {
        /* If we're paused too long, music services will drop connections
         and on resuming, we play out the buffer then crap out.  Instead,
         if we're paused too long, cancel playback. */
        abort_playpoint = 1;
        player->abort();
        return 0.2;
    }
    return pause_expiration - now;
}

/** Retrieves some random tracks and puts them in the queue.
    @return True on success, false otherwise. */
bool AudioEngine::acquireRandomTracks (void) {
    assert (queue_mode == QueueMode::RANDOMPLAY);
    assert (random_queue.empty());

    // If there was a failure, wait until the retry duration passes.
    if (track_acquisition_time > time (nullptr))
        return false;

    try {
        random_queue = mix.getRandomTracks (current_playlist.get(), user_manager->getUsersPresent (*service, true));
        if (!random_queue.empty()) {
            service->announceToRoom (Response (V_QUEUE_CHANGED));
            return true;
        }
    } catch (std::runtime_error &) {
        // Try again in 5 minutes
        track_acquisition_time = time (nullptr) + 300;
    }
    return false;
}

/** Remove songs from the front of the random queue
    (but not songs from the request queue).
    Reasons songs may be removed:
    - The mix or selected playlist has changed, and it no longer applies.
    - The song has expired. */
void AudioEngine::PurgeUnselectedSongs (void) {
    // Requests never get purged.
    if (!requests.empty())
        return;
    // If we're not playing random stuff, leave the queue alone.
    if (queue_mode != QueueMode::RANDOMPLAY)
        return;

    while (!random_queue.empty()) {
        const char *reason = "???"; // NOLINT (dead-initialization)
        Retainer<PianodSong *> front = random_queue.front();
        if (front->expired()) {
            reason = "song expired";
        } else if (front->source()->isLockedOut()) {
            reason = "source is temporarily locked out";
        } else if (front->mustPlay()) {
            return;
        } else if (current_playlist->source() != media_manager && current_playlist->source() != front->source()) {
            reason = "song does not originate from current source";
        } else if (!front->source()->isReady()) {
            reason = "source is presently offline";
        } else {
            // Check if the queue item matches the current playlist selection.
            // Return if the song matches, otherwise remove song and try again.
            switch (current_playlist->playlistType()) {
                case PianodPlaylist::EVERYTHING:
                case PianodPlaylist::TRANSIENT:
                    return;
                case PianodPlaylist::MIX: {
                    PianodPlaylist *songPlaylist = front->playlist();
                    if (!songPlaylist) {
                        reason = "song does not have a playlist";
                    } else if (!mix.includedInMix (songPlaylist)) {
                        reason = "song's playlist is not in the mix";
                    } else {
                        return;
                    }
                    break;
                }
                case PianodPlaylist::SINGLE: {
                    if (front->playlist() && front->playlist()->id() == current_playlist->id())
                        return;
                    reason = "song's playlist is not the selected playlist";
                    break;
                }
                default:
                    assert (!"Switch/case unmatched.");
            }
        }
        flog (LOG_WHERE (Log::QUEUE),
              "Removing song ",
              front->id(),
              " ('",
              front->title(),
              "') from random queue: ",
              reason);
        random_queue.pop_front();
    }
}

/** Create a player, if the current state allows it.
    If there is nothing queued, and in random mode, get some random tracks.
    @return True on success (no player needed or player created),
    false on player creation error. */
bool AudioEngine::considerCreatingPlayer (void) {
    // Start a player if there are no reasons not to start one.
    assert (!cueing_player);
    assert (!cueing_song);

    // Check for pending shutdown
    if (quit_requested) {
        if (!quit_initiated && !player) {
            service->announceToRoom (Response (F_SHUTDOWN));
            service->close();
            quit_initiated = true;
        }
        return true;
    }

    // Don't play if we're not supposed to.
    if (queue_mode == QueueMode::STOPPED || playback_state == PlaybackState::PAUSED)
        return true;
    
    // Don't play if we're in lockout due to errors
    if (media_manager->isLockedOut()) {
        if (!lockout_announced) {
            service->announceToRoom (Response (I_INFO, "Playback temporarily delayed due to repeated failures"));
            lockout_announced = true;
        }
        return true;
    }
    lockout_announced = false;

    if (requests.empty() && current_playlist->playlistType() == PianodPlaylist::MIX) {
        if (mix.empty (current_playlist->source() == media_manager ? nullptr : current_playlist->source())) {
            if (!empty_warning_given && !player && !cueing_player) {
                service->announceToRoom (Response (I_INFO, "No playlists in mix"));
                empty_warning_given = true;
            }
            return true;
        }
    }

    // Get rid of any expiring or unwanted junk from the queue.
    PurgeUnselectedSongs();

    // Fill the queue if empty and applicable
    if (queue_mode == QueueMode::RANDOMPLAY && queueEmpty()) {
        acquireRandomTracks();
    }

    // Start something!
    if ((queue_mode == QueueMode::RANDOMPLAY && !queueEmpty())
        || (queue_mode == QueueMode::REQUESTS && !requests.empty())) {
        return startPlayer();
    }
    return true;
}

/** Periodically invoked function that monitors players,
    starts new songs, etc. */
float AudioEngine::periodic (void) {
    // If song finished playing, clean up things
    if (player && player->playbackComplete()) {
        ResponseGroup announcements;
        announcements (cleanupPlayer());
        announcements (promotePlayer());
        service->announceToRoom (announcements);
    }

    bool created_ok = true;
    // If player is fresh and ready to go, start it up
    if (player == nullptr) {
        created_ok = considerCreatingPlayer();
        if (cueing_player) {
            ResponseGroup announcements;
            announcements (promotePlayer());
            service->announceToRoom (announcements);
        }
    }

    // If the playback thread is valid, do various monitoring/handling on it
    float next_request = 3600;
    if (player && player->ready() && !player->playbackComplete()) {
        next_request = monitorPlayer();
    } else if (player && playback_state == PlaybackState::PLAYING) {
        next_request = 1;
    } else if (!player && !created_ok) {
        // Player creation failed above.  Retry again soon.
        next_request = 1;
    } else if (lockout_announced) {
        next_request = 10;
    }
    return next_request;
}

/** When a new source is ready, clear any track aquisition delays. */
void AudioEngine::sourceReady (const Media::Source * const ) {
    track_acquisition_time = 0;
}

/** Remove all songs from a given source.
    @param source The source to remove.
    @return True if successfully eradicated, false if not
    (busy playing media from the source). */
bool AudioEngine::sourceRemovalCheck (const Media::Source * const source) {
    bool changed = requests.purge (source);
    changed = random_queue.purge (source) || changed;
    if (changed) {
        service->announceToRoom (Response (V_QUEUE_CHANGED));
    }
    sourceOffline (source);

    return ((!current_song || current_song->source() != source) && (!cueing_song || cueing_song->source() != source));
}

/** Unregister a source by removing its playlists.  However, allow
 items to remain in the history and queue (in case it comes back
 online). */
void AudioEngine::sourceRemoved (const Media::Source * const source) {
    assert (requests.purge (source) == false);
    assert (random_queue.purge (source) == false);
    song_history.purge (source);
}

/** Unregister a source by removing its playlists.  However, allow
    items to remain in the history and queue (in case it comes back
    online). */
void AudioEngine::sourceOffline (const Media::Source * const source) {
    if (current_playlist->source() == source) {
        current_playlist = media_manager->getMixPlaylist();
        assert (current_playlist);
        service->announceToRoom (gatherSelectedPlaylist());
    }
}

/** Transmit any source status notifications to connected clients.
    @param status The numeric status code.
    @param detail Additional details about the status. */
void AudioEngine::sourceStatus (RESPONSE_CODE status, const char *detail) {
    service->announceToRoom (Response (status, detail));
}

/** When playlists have changed, reset track aquisition lockouts. */
void AudioEngine::playlistsChanged() {
    track_acquisition_time = 0;
    empty_warning_given = false;
}

/** When mix has changed, reset track aquisition lockouts. */
void AudioEngine::mixChanged (bool, const char *) {
    track_acquisition_time = 0;
    empty_warning_given = false;
}

void AudioEngine::usersChangedNotification() {
    mix.recalculatePlaylists();
};
