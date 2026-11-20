///
/// Wrapper for media players
/// Provides a way for sources to manage or monitor player actions.
/// @file       playerwrapper.h - pianod project
/// @author     Perette Barella
/// @date       2015-02-04
/// @copyright  Copyright 2020 Devious Fish. All rights reserved.
///

#include <config.h>

#include <utility>
#include <memory>

#include <ctime>

#include "mediaplayer.h"
#include "playerwrapper.h"

namespace Media {
    PlayerWrapper::PlayerWrapper (Player *player): inherited (player) {
        
    }

    PlayerWrapper::PlayerWrapper (std::unique_ptr <Player> &&player): inherited (std::move (player)) {

    }

    void PlayerWrapper::pause (void) {
        inherited->pause();
    }
    
    void PlayerWrapper::abort (void) {
        inherited->abort();
    }
    
    void PlayerWrapper::setVolume (float volume) {
        inherited->setVolume (volume);
    }
    
    void PlayerWrapper::cue (void) {
        inherited->cue();
    }
    
    void PlayerWrapper::play (void) {
        inherited->play();
    }

    // Data retrieval functions
    float PlayerWrapper::trackDuration (void) const {
        return inherited->trackDuration ();
    }
    
    float PlayerWrapper::playPoint (void) const {
        return inherited->playPoint();
    }
    
    float PlayerWrapper::playRemaining (void) const {
        return inherited->playRemaining();
    }
    
    RESPONSE_CODE PlayerWrapper::completionStatus (void) {
        return inherited->completionStatus();
    }

    Player::State PlayerWrapper::currentState (void) const {
        return inherited->currentState();
    }

    time_t PlayerWrapper::getPauseTimeout (void) {
        return inherited->getPauseTimeout();
    }

}  // namespace Media
