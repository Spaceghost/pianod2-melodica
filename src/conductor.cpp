///
/// Coordinate initialization and operation of pianod daemon.
/// @file       conductor.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-12-09
/// @copyright  Copyright (c) 2015-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include "conductor.h"
#include "logging.h"

#include <config.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstdint>
#include <cassert>

#include <signal.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>

#include <exception>

#ifdef HAVE_LIBGCRYPT
#include <gcrypt.h>
#endif

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

#include <football/fb_public.h>

#include "fundamentals.h"
#include "sources.h"
#include "response.h"
#include "logging.h"
#include "servicemanager.h"
#include "engine.h"
#include "user.h"
#include "users.h"
#include "audio/audiooutput.h"
#include "mediaplayer.h"
#include "mediamanager.h"


namespace Orchestra {
    /** Report libraries in use and their versions.
        @param verbose Verbosity level.  The higher, the more detail. */
    void reportLibrariesAndVersions (int verbose) {
        Audio::Output::reportLibrariesAndVersions (verbose);
    }

    /// Initialize libgcrypt
    GCryptInitializer::GCryptInitializer (void) {
#ifdef HAVE_LIBGCRYPT
        gcry_check_version (NULL);
        gcry_control (GCRYCTL_DISABLE_SECMEM, 0);
        gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);
#endif
    }

    /// Initialize cURL
    cURLInitializer::cURLInitializer (void) {
#ifdef HAVE_LIBCURL
        curl_global_init (CURL_GLOBAL_DEFAULT);
#endif
    }

    cURLInitializer::~cURLInitializer (void) {
#ifdef HAVE_LIBCURL
        curl_global_cleanup();
#endif
    }

    /// Initialize the daemon.
    // Globals constructed and initialized in order: user manager, media manager, service manager.
    Conductor::Conductor (std::string config_dir, FB_SERVICE_OPTIONS options, AudioSettings audio) {
        // Initialize TLS stuff for the service
        if (!fb_init_tls_support (config_dir.c_str())) {
            options.https_port = 0;
        }
        assert (!media_manager);
        assert (!service_manager);
        assert (!user_manager);
        try {
            // Create user manager and restore users
            try {
                user_manager = new UserManager();
            } catch (const std::exception &e) {
                flog (LOG_WHERE (Log::ERROR), "Could not create user manager: ", e.what());
                throw InitializationException ("Cannot construct user manager");;
            }

            try {
                user_manager->restore();
            } catch (const std::exception &e) {
                flog (LOG_WHERE (Log::ERROR), "Error restoring users: ", e.what());
                throw InitializationException ("Cannot restore user list");;
            }

            // Create media manager and restore sources
            try {
                media_manager = new Media::Manager();
            } catch (const std::exception &e) {
                flog (LOG_WHERE (Log::ERROR), "Could not create media manager: ", e.what());
                throw InitializationException ("Cannot construct media manager");;
            }
            try {
                auto sources = user_manager->getStoredSources (UserManager::WhichSources::Restorable);
                for (const auto &source : sources) {
                    try {
                        Sources::restore (source.second, source.first);
                    } catch (const std::exception &e) {
                        flog (LOG_WHERE (Log::ERROR), "Could not restore source: User ", source.first->username(),
                              ", source ", source.second->origin(), ", name ", source.second->identity(),
                              ": ", e.what());
                    }
                }
            } catch (const std::exception &e) {
                flog (LOG_WHERE (Log::ERROR), "Error restoring sources: ", e.what());
                throw InitializationException ("Cannot restore sources");;
            }

            // Create the service manager and an initial service
            try {
                service_manager = new ServiceManager();
            } catch (const std::exception &e) {
                flog (LOG_WHERE (Log::ERROR), "Could not create service manager: ", e.what());
                throw InitializationException ("Cannot construct service manager");;
            }
            service = service_manager->createRoom ("pianod", audio, options);
            if (!service) {
                flog (LOG_WHERE (Log::ERROR), "Unable to create service, giving up.");
                throw InitializationException ("Cannot create pianod network service");
            }

        } catch (const std::exception &e) {
            cleanUp();
            throw;
        }

        try {
            if (!Audio::Output::outputCanCrossfade (audio)) {
                audio.preroll_time = 0.0f;
                audio.crossfade_time = 0.0f;
            }
        } catch (const Audio::AudioException &e) {
            audio.preroll_time = 0.0f;
            audio.crossfade_time = 0.0f;
        }

        // Create an artificial session that reads the configuration file
        PianodConnection *config = service->newConnectionFromFile (config_dir + "startscript");
        if (config) {
            config->user = User::getStartscriptUser();
        }
    }

    Conductor::~Conductor() {
        cleanUp();
    }

    /// Run the daemon.
    void Conductor::conduct() {
        // Main run loop
        float pollTime = 1.0;
        while (Football::Arena::ready()) {
            MusicAutoReleasePool releasePool;
            // See if sockets need attention
            if (!Football::Arena::pollWithTimeout (pollTime)) {
                // Check the signal handler's flag for shutdown requests.
                if (shutdown_requested) {
                    shutdown_requested = false;
                    service_manager->shutdown (true);
                }
                /* We will only periodic() when idle (or signal).
                 A flurry of activity or a long request could delay
                 periodic() and cause problems; perhaps in the future
                 some of these non-interactive things could move to
                 another thread... but that requires adding some mutexes
                 and care and thought, so not right now. */
                pollTime = service_manager->periodic();
            } else {
                pollTime = 0;
            }
        }
        service_manager->flush();
    }

    // Globals destructed in reverse order of construction.
    void Conductor::cleanUp() {
        if (service_manager) {
            delete service_manager;
            service_manager = nullptr;
        }
        if (media_manager) {
            delete media_manager;
            media_manager = nullptr;
        }
        if (user_manager) {
            delete user_manager;
            user_manager = nullptr;
        }
        fb_cleanup_tls_support();
    }

}  // namespace Orchestra
