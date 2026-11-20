///
/// Library/cache supporting Pandora player.
/// @file       mediaunits/pandora/pandoralibrary.cpp - pianod
/// @author     Perette Barella
/// @date       2020-04-03
/// @copyright  Copyright 2020-2021 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <cassert>

#include "pandoratypes.h"
#include "pandoracomm.h"
#include "pandoramessages.h"
#include "pandora.h"

namespace Pandora {
    MusicThingie *Library::reconstruct (MusicThingie::Type type, const Parsnip::Data &data) {
        switch (type) {
            case MusicThingie::Type::Song:
                return new Song (pandora(), type, data);
            case MusicThingie::Type::Artist:
                return new Artist (pandora(), type, data);
            case MusicThingie::Type::PlaylistSuggestion:
                return new GenreSuggestion (pandora(), type, data);
            default:
                return nullptr;
        }
        return nullptr;
    }
}  // namespace Pandora
