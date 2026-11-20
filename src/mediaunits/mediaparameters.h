///
/// Class for source parameters and their parsing.
/// @file       mediaparameters.h - pianod2
/// @author     Perette Barella
/// @date       2015-01-28
/// @copyright  Copyright (c) 2015-2020 Devious Fish. All rights reserved.
///

#pragma once 

#include <config.h>

#include <string>

#include <parsnip/parsnip_command.h>

// #include "fundamentals.h"
#include "ownership.h"
#include "datastore.h"


namespace Media {
    /// How a source participates in track-source substitutions;
    enum class ProxyMode {
        None, ///< Source does not participate.
        Donor, ///< Source provides substitute tracks.
        Recipient ///< Source wants its tracks replaced when possible.
    };

    enum class PersistenceMode {
        Temporary, ///< Provided from command line, store neither settings nor indexes.
        Loaded, ///< Values loaded from stored data, do not update settings but do update indexes.
        Remember, ///< Store it, store indexes, but manually restore only.
        Restore, ///< Store it, store indexes, and automatically restore on startup.
    };

    /** Source parameter manager.  Implements common parameters, and can
        be extended for each source type to support custom parameters. */
    class SourceParameters {
        bool userSpecified = false; // For private use by parameter parser.
    public:
        ProxyMode song_proxy = ProxyMode::None;
        User *owner = nullptr;
        Ownership::Type permissions = Ownership::Type::PRIVATE;
        PersistenceMode persistence = PersistenceMode::Temporary; ///< Persistence state.
        std::string name;
        bool waitForReady = false; ///< Wait on initialization.  Flag never persisted.

        SourceParameters (void) { };
        SourceParameters (Ownership::Type perm, User *user = nullptr);
        SourceParameters (Ownership::Type perm,
                          const std::string &sourcename);
        SourceParameters (const UserData::JSONData &src);
        virtual ~SourceParameters ();
        virtual bool persist (UserData::JSONData &dest) const;
        
        static const Parsnip::OptionParser::Definitions &parser_definitions ();
        void extractOptions (const Parsnip::Data &options);
    private:
        void extract (const Parsnip::Data &options);
    };
}

