///
/// Wrapper for media players
/// Provides a way for sources to manage or monitor player actions.
/// @file       playerwrapper.h - pianod project
/// @author     Perette Barella
/// @date       2015-02-04
/// @copyright  Copyright 2020 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <memory>

#include "mediaplayer.h"

#include <ctime>

namespace Media {
    /** The Player class is abstract, and is implemented several different ways
        (ffmpeg, gstreamer, AVFoundation...).  Exactly what's in use is blackboxed
        from the sources, but that isolation also means we can't specialize them
        through inheritance.  Losing the encapsulation would create a maintenance nightmare.
        
        We could add some delegates, but that also means repeated implementations in
        each of the different player types.

        So, we have this stupid wrapper.  It's the least of the bad choices.  It accepts
        a player instance, and simply forwards every request onto the player.  A
        source that wants to customize, can wrap its player in a wrapper that's been
        specialized to its needs. */
    class PlayerWrapper : public Player {
        std::unique_ptr <Player> inherited;
    public:
        PlayerWrapper (Player *);
        PlayerWrapper (std::unique_ptr <Player> &&);
    
        // Regulate playback
        virtual void pause (void) override;
        virtual void abort (void) override;
        virtual void setVolume (float volume) override;
        virtual void cue (void) override;
        virtual void play (void) override;

        // Data retrieval functions
        virtual float trackDuration (void) const override;
        virtual float playPoint (void) const override;
        virtual float playRemaining (void) const override;
        virtual RESPONSE_CODE completionStatus (void) override;

        virtual State currentState (void) const override;

        virtual time_t getPauseTimeout (void) override;
    };
}

