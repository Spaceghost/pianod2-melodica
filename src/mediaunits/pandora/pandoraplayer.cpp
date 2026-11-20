///
/// Media player for Pandora.
/// This was once derived from pianobar, but that code has evolved away.
/// @file       mediaunits/pandora/pandoraplayer.cpp - pianod project
/// @author     Perette Barella
/// @date       2014-10-24
/// @copyright  Copyright 2012-2017 Devious Fish. All rights reserved.
///

#include <config.h>

#include "fundamentals.h"
#include "musictypes.h"
#include "retainer.h"

#include "pandoratypes.h"
#include "pandoramessages.h"
#include "pandora.h"

#include "mediaplayer.h"
#include "playerwrapper.h"

namespace Pandora {

    Media::Player *Source::getPlayer (const AudioSettings &audio, PianodSong *s) {
        PlayableSong *song = dynamic_cast<PlayableSong *> (s);
        if (song) {
            return Media::Player::getPlayer (audio, song->audio_url, song->audio_gain);
        }
        Advert *ad = dynamic_cast <Advert *> (s);
        assert (ad);
        if (ad) {
            std::unique_ptr <Media::Player> player { Media::Player::getPlayer (audio, ad->audio_url, ad->audio_gain) };
            AdvertPlayedNotification ad_played (ad->ad_station.get(), ad->ad_tokens);
            executeRequest (ad_played);
            return player.release();
        }
        return nullptr;
    };
}  // namespace Pandora
