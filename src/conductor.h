///
/// Coordinate initialization and operation of pianod daemon.
/// @file       conductor.h - pianod2
/// @author     Perette Barella
/// @date       2015-12-09
/// @copyright  Copyright (c) 2015-2020 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>

#include <football/football.h>

#include "fundamentals.h"
#include "connection.h"
#include "audio/audiooutput.h"
#include "mediaplayer.h"

/// Daemon initialization, cleanup and main run loop.
namespace Orchestra {
    extern void reportLibrariesAndVersions (int verbose);

    /// Manage crypto library initialization and deinitialization.
    class GCryptInitializer {
    public:
        GCryptInitializer ();
    };

    /// Manage transport layer security initialization and deinitialization.
    class cURLInitializer {
    public:
        cURLInitializer ();
        ~cURLInitializer();
    };

    /** Manage daemon initialization and operation.
        Although unenforced, this is a singleton. */
    class Conductor {
    private:
        volatile bool shutdown_requested = false;
        PianodService *service = nullptr;
        GCryptInitializer initter_gcrypt;
        cURLInitializer init_curl;
        Audio::Initializer initter_audio;
        Media::Initializer initter_media;

    public:
        Conductor (std::string config_dir, FB_SERVICE_OPTIONS service_options,
                   AudioSettings audio);
        ~Conductor ();
        void conduct ();
        void cleanUp ();
        /// Request the daemon exit its run loop and shut down.
        inline void shutdown () { shutdown_requested = true; };
    };
}

