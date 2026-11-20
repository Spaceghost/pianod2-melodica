///
/// Source parameters for sources based on music library.
/// @file       musiclibraryparameters.h - pianod
/// @author     Perette Barella
/// @date       20150-03-16
/// @copyright  Copyright (c) 2015-2020 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <parsnip/parsnip_command.h>

#include "mediaparameters.h"
#include "musiclibrary.h"

#define KEY_RESCAN_BEHAVIOR "rescanInterval"

namespace MusicLibrary {
    enum class ScanFrequency {
        NEVER,
        NEXTSTARTUP,
        EVERYSTARTUP,
        DAILY
    };

    extern const LookupTable<ScanFrequency> RescanModes;

    class LibraryParameters : public Media::SourceParameters {
    public:
        int recent_bias = BIAS_NEUTRAL;  ///< Amount of bias to apply for time since last play.
        int rating_bias = BIAS_NEUTRAL;  ///< Amount of bias to apply for user ratings.
        ScanFrequency scan_frequency = ScanFrequency::NEXTSTARTUP;

        LibraryParameters (Ownership::Type perm, User *user);
        LibraryParameters (const UserData::JSONData &src);
        virtual bool persist (UserData::JSONData &dest) const override;
        void extractOptions (const Parsnip::Data &options);
        static Parsnip::OptionParser::Definitions parser_definitions ();
    private:
        void extract (const Parsnip::Data &options);
    };

}  // namespace MusicLibrary
