///
/// Source parameters for sources based on music library.
/// @file       musiclibraryparameters.cpp - pianod
/// @author     Perette Barella
/// @date       2020-12-01
/// @copyright  Copyright (c) 2015-2020 Devious Fish. All rights reserved.
///

#include <config.h>

#include <algorithm>

#include <parsnip/parsnip_command.h>

#include "mediaparameters.h"
#include "musiclibraryparameters.h"

/*
     *                  Library Parameters
     */

#define KEY_RECENT_BIAS "recentBias"
#define KEY_RATING_BIAS "ratingBias"

namespace MusicLibrary {

    // clang-format off
    const LookupTable<MusicLibrary::ScanFrequency> RescanModes ({
        {"never",        ScanFrequency::NEXTSTARTUP},
        {"once",         ScanFrequency::NEXTSTARTUP},
        {"always",       ScanFrequency::EVERYSTARTUP},
        {"periodically", ScanFrequency::DAILY}
    });
    // clang-format on

    Parsnip::OptionParser::Definitions LibraryParameters::parser_definitions() {
        /** Parse definitions for the parameter parser */
        Parsnip::OptionParser::Definitions parser { Media::SourceParameters::parser_definitions() };
        static const Parsnip::OptionParser::Definitions library_options = {
                "recent bias {#" KEY_RECENT_BIAS ":1-100}",
                "rating bias {#" KEY_RATING_BIAS ":1-100}",
                "rescan <" KEY_RESCAN_BEHAVIOR ":never|once|always|periodically>",
        };
        std::copy (library_options.begin(), library_options.end(), std::back_inserter (parser));
        return parser;
    }

    LibraryParameters::LibraryParameters (Ownership::Type perm, User *user) : Media::SourceParameters (perm, user) {
    }
    LibraryParameters::LibraryParameters (const UserData::JSONData &src) : Media::SourceParameters (src) {
        extract (src);
        if (rating_bias < BIAS_MINIMUM || rating_bias > BIAS_MAXIMUM)
            throw std::invalid_argument ("Invalid ratingBias");
        if (recent_bias < BIAS_MINIMUM || recent_bias > BIAS_MAXIMUM)
            throw std::invalid_argument ("Invalid recentBias");
    };

    bool LibraryParameters::persist (UserData::JSONData &dest) const {
        dest [KEY_RATING_BIAS] = rating_bias;
        dest [KEY_RECENT_BIAS] = recent_bias;
        dest ["rescan"] = RescanModes[scan_frequency];
        return Media::SourceParameters::persist (dest);
    };

    void LibraryParameters::extractOptions (const Parsnip::Data &options) {
        Media::SourceParameters::extractOptions (options);
        extract (options);
    }
    
    void LibraryParameters::extract (const Parsnip::Data &options) {
        if (options.contains (KEY_RECENT_BIAS)) {
            recent_bias = options[KEY_RECENT_BIAS].asInteger();
        }
        if (options.contains (KEY_RATING_BIAS)) {
            rating_bias = options[KEY_RATING_BIAS].asInteger();
        }
        if (options.contains (KEY_RESCAN_BEHAVIOR)) {
            scan_frequency = RescanModes[options[KEY_RESCAN_BEHAVIOR].asString()];
        }
    }
}  // namespace MusicLibrary
