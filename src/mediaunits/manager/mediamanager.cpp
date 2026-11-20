///
/// Manages the available media source instances.
/// Methods and actions outside the usual source actions.
/// @file       mediamanager.cpp - pianod
/// @author     Perette Barella
/// @date       2014-11-28
/// @copyright  Copyright 2014-2023 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <string>
#include <iterator>
#include <algorithm>

#include "musictypes.h"
#include "encapmusic.h"
#include "mediaunit.h"
#include "mediaparameters.h"
#include "mediamanager.h"

#include "callback.h"
#include "callbackimpl.h"
#include "fundamentals.h"
#include "utility.h"
#include "response.h"
#include "connection.h"

// Force template instantiation.
template class CallbackManager<Media::Manager, Media::Manager::Callbacks>;

namespace Media {
    Manager::Manager (void) : Source::Source (new SourceParameters (Ownership::Type::PUBLISHED, "Pianod")) {
        MusicAutoReleasePool pool;
        add (this);
        state = State::READY;

        // Create a reusable mix playlist
        // If this fails, let bad_alloc throw because we need the manager's mix playlist.
        mix_playlist = new MetaPlaylist (this, PianodPlaylist::MIX);
        mix_playlist->retain();

        // Create a reusable everything playlist
        everything_playlist = new (std::nothrow) MetaPlaylist (this, PianodPlaylist::EVERYTHING);
        if (everything_playlist) {
            everything_playlist->retain();
        }

        // Create a reusable transient playlist
        transient_playlist = new (std::nothrow) MetaPlaylist (this, PianodPlaylist::TRANSIENT);
        if (transient_playlist) {
            transient_playlist->retain();
        }
    }

    Manager::~Manager () {
        iterator it = begin();
        while (it != end()) {
            iterator remove = it++;
            if (remove->second != this) {
                kept_assert (erase (remove->second));
            }
        }
    }


    /** Retrieve a source by serial number.
        @param serial The source serial number.
        @return The source, or nullptr if not found. */
    Source * const Manager::get (const SerialNumber serial) const {
        const_iterator item = find (serial);
        if (item == end())
            return nullptr;
        return item->second;
    }

    /** Retrieve a source by type and ID.  ID meaning varies by source.
        @param type The source type name.
        @param ident The source ID.
        @return The source, or nullptr if not found. */
    Source * const Manager::get (const std::string &type, const std::string &ident) const {
        for (auto const &item : *this) {
            if (strcasecmp (item.second->kind(), type) == 0 && strcasecmp (item.second->name(), ident) == 0) {
                return item.second;
            }
        }
        return nullptr;
    }

    /** Add a source.  No validation or messaging is done.
        @param source The new source.
        Caller is responsible for destruction on failure.
        @return True on success, false on error (duplicate). */
    bool Manager::add (Source * const source) {
        assert (source);
        if (!get (source->kind(), source->name())) {
            source->statusHandler = std::bind (&Manager::redirectingStatusHandler, this,
                                               std::placeholders::_1, std::placeholders::_2);
            static SerialNumber unitNumber = 0;
            source->serialNum = ++unitNumber;
            value_type item (source->serialNum, source);
            std::pair<iterator, bool> result = insert (item);
            if (result.second) {
                return true;
            }
            flog (LOG_WHERE (Log::ERROR), "Failed to insert new source");
        };
        return false;
    };

    /** Add a source.
        @param source A managed pointer to the new source,
        ensuring that on failure, it is freed.
        @return Non-managed pointer to the new source. */
    Source * const Manager::add (SourcePtr &&source) {
        if (!add (source.get())) {
            throw CommandError (E_DUPLICATE);
        }
        return source.release();
    }

    /** Interactively add a source.  Checks usability, handles events,
        announces change.
        @param source The source to add.  If there is a problem, this is freed.
        @param conn The connection attempting to add the source.
        @return false on failure, true on success. */
    ResponseCollector Manager::add (SourcePtr &&source, PianodConnection &conn) {
        // Disowned sources have no owner, therefore fail the usual usable test.
        if (source->isOwned() && !source->isUsableBy (conn.user)) {
            throw CommandError (E_UNAUTHORIZED);
        }
        Source * const sp = add (std::move (source));
        assert (sp->parameters);
        if (sp->parameters->waitForReady) {
            conn.waitForEvent (WaitEvent::Type::SourceReady, sp);
        }
        if (sp->parameters->persistence != PersistenceMode::Temporary &&
            sp->parameters->persistence != PersistenceMode::Loaded) {
            sp->persist();
        }
        CommandReply response (sp->parameters->waitForReady ? NO_REPLY : S_OK);
        response.broadcast_events (sp->isOwnedBy (conn.user) ? A_SOURCE_ADD : A_SOURCE_BORROW);
        response.broadcast_events (V_SOURCES_CHANGED);
        return std::move (response);
    }

    /** Remove a source.  If a source is busy, its is moved out of "ready" state
        and removal deferred.  The periodic() method subsequently checks if it
        is ready to be removed, and does so when possible.
        @param source The source to remove.
        @return true if successful, false if source is busy (removal is deferred). */
    bool Manager::erase (Source * const source) {
        source->state = State::DEAD;
        if (!callback.queryUnanimousApproval (true, &Callbacks::canRemoveSource, source)) {
            return false;
        }
        callback.notify (&Callbacks::sourceOffline, source);
        callback.notify (&Callbacks::sourceRemoved, source);
        unordered_map::erase (source->serialNum);
        source->alert (V_SOURCES_CHANGED, "removal complete");
        delete source;
        return true;
    }

    /** Reset a temporary lockout so playback can be attempted immediately. */
    void Manager::resetLockout () {
        lockout_until = 0;
        for (auto const &src : *this) {
            src.second->lockout_until = 0;
        }
    }


    /** Check if there are any sources in a given state.
        @return state The state to search for.
        @return true if any sources are in the state, false otherwise. */
    bool Manager::areSourcesInState (Source::State state) const {
        for (auto const &src : *this) {
            if (src.second->state == state)
                return true;
        }
        return false;
    }

    /** Get all sources (excluding the manager itself). */
    Manager::SourceList Manager::getRealSources() const {
        SourceList sources;
        sources.reserve (size());
        for (const auto &src : *this) {
            if (src.second != this) {
                sources.push_back (src.second);
            }
        }
        return sources;
    };

    /** Get sources (excluding the manager itself) in a given state. */
    Manager::SourceList Manager::getRealSources (Source::State state) const {
        SourceList sources;
        sources.reserve (size());
        for (const auto &src : *this) {
            if (src.second != this && state == src.second->state) {
                sources.push_back (src.second);
            }
        }
        return sources;
    };

    /** Ask all the sources to persist any data.
        @return True if all succeeded, false otherwise. */
    bool Manager::flush (void) {
        bool status = true;
        for (auto src : getRealSources()) {
            status = src->flush() && status;
        }
        return status;
    }

    /** Respond to a source's state change.
        - Send any appropriate messages.
        - Invoke appropriate callbacks.
        @param source The source whose state has changed. */
    void Manager::handleSourceStateChange (Source * const source) {
        if (source->state == State::READY) {
            source->alert (V_SOURCES_CHANGED, "ready");
            callback.notify (&Callbacks::sourceReady, source);
        } else if (source->state == State::VALID && source->announced_state == State::READY) {
            source->alert (V_SOURCES_CHANGED, "offline");
            callback.notify (&Callbacks::sourceOffline, source);
        } else if (source->state == State::DEAD) {
            source->reportStatus ("removal initiated");
        }
    }

    /** Perform periodic activities and remove sources that deferred removal.
        This method dispatches to each registered source.
        Each source indicates when they want their next time slice.
        @return Interval until next invokation (shortest interval requested
        by any children sources). */
    float Manager::periodic (void) {
        float nextRequest = A_LONG_TIME;

        // Purge dead sources.  Range-for can't be used because of erasures.
        for (iterator iter = begin(); iter != end();) {
            iterator remove = iter++;
            if (remove->second->state == State::DEAD) {
                assert (remove->second != this);
                remove->second->flush();
                erase (remove->second);
            }
        }

        for (auto &it : *this) {
            if (it.second != this) {
                float nextReq = it.second->periodic();
                if (nextReq < nextRequest)
                    nextRequest = nextReq;

                // If the state has changed, dispatch the state change callbacks.
                if (it.second->announced_state != it.second->state) {
                    handleSourceStateChange (it.second);
                    it.second->announced_state = it.second->state;
                }
            }
        }
        return nextRequest;
    }

    /** Handle an alert by passing it to callbacks.
        @param status The numeric status or alert.
        @param detail Description of alert details. */
    void Manager::redirectingStatusHandler (RESPONSE_CODE status, const char *detail) {
        callback.notify (&Callbacks::statusNotification, status, detail);
    }

    /** Retrieve a playlist, song, album or artist by ID.
        @param id The item's ID.
        @return The item, or nullptr if not found or source not ready.
        @throw CommandError if the ID is invalid. */
    MusicThingie *Manager::getAnythingById (const std::string &id) {
        SplitId origin (id);
        return origin.source->getAnythingById (origin);
    }

    /** Retrieve a playlist, song, album or artist by ID.
        @param id The item's ID.
        @return The item, or nullptr if not found or source not ready. */
    MusicThingie *Manager::getAnythingById (const SplitId &id) {
        assert (0);
        flog (LOG_FUNCTION (Log::WARNING), " called with split ID");
        return id.source->getAnythingById (id);
    }
}  // namespace Media

/* Global */ Media::Manager *media_manager{nullptr};
