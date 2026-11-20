///
/// OS X audio player.
/// Play media files on OS X using AVFoundation framework.
/// @file       osxplayer.h - pianod2
/// @author     Perette Barella
/// @date       2015-11-18
/// @copyright  Copyright (c) 2015-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>

#include "mediaplayer.h"

namespace Audio {
    extern Media::Player *getAVFoundationPlayer(const AudioSettings &,
                                                const std::string &media_url,
                                                float initial_audio_gain);
}
