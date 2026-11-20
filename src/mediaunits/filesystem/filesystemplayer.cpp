///
/// Play audio files from a local filesystem.
/// @file       filesystemplayer.cpp - pianod
/// @author     Perette Barella
/// @date       2015-02-25
/// @copyright  Copyright 2015-2021 Devious Fish.  All rights reserved.
///

#include <config.h>

#include "metadata/metadata.h"

#include "fundamentals.h"
#include "filesystem.h"


// Miscellaneous
namespace Filesystem {
    Media::Player *Source::getPlayer (const AudioSettings &audio, PianodSong *s) {
        auto song = static_cast <Song *> (s);
        float gain = Media::Metadata::getFileGain (song->path());

        Media::Player *player = Media::Player::getPlayer (audio, song->path(), gain);
        if (player)
            library.markDirty(); // last played time will have changed.
        return player;
    };

}
