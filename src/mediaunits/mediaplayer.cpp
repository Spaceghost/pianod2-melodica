///
/// Public interface for media players.
/// Media players are responsible for playing audio.
/// @file       mediaplayer.cpp - pianod project
/// @author     Perette Barella
/// @date       2014-10-23
/// @copyright  Copyright 2012-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <assert.h>

#include <iostream>
#include <thread>

#include "utility.h"
#include "fundamentals.h"
#include "logging.h"
#include "audio/audiooutput.h"
#include "mediaplayer.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"
#pragma GCC diagnostic ignored "-Wshadow"

#ifdef WITH_GSTREAMER

#include <gst/gst.h>

#include "audio/gstreamplayer.h"

#elif defined(WITH_FFMPEG)

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavfilter/avfilter.h>
}

#include "audio/ffmpegplayer.h"

#ifdef WITH_LIBAVDEVICE
extern "C" {
#include <libavdevice/avdevice.h>
}
#endif
#elif defined(WITH_AVFOUNDATION)
#include "audio/osxplayer.h"
#endif

#pragma GCC diagnostic pop


namespace Media {
    Player::~Player (void) {
       // Nothing in this base class
    };

    
    time_t Player::getPauseTimeout (void) {
        return FAR_FUTURE;
    };

    
    
    /** Report media libraries in use and their versions.
        @param verbose If >0, include additional details. */
    void reportLibrariesAndVersions(int verbose) {
        std::cerr << "  Media libraries:\n";
#ifdef WITH_GSTREAMER
        (void) verbose;
        guint major, minor, micro, nano;
        gst_version(&major, &minor, &micro, &nano);
        std::cerr << "    gstreamer\n";
        {
            std::cerr << "      gstreamer library\n"
            << "        runtime version " << major << '.' << minor << '.' << micro << '.' << nano << '\n';
        }

#elif WITH_FFMPEG
        std::cerr << "    ffmpeg\n";

        {
            unsigned linked = avformat_version();
            std::cerr << "      avformat\n"
            << "        compile version " << LIBAVFORMAT_VERSION_MAJOR << '.' << LIBAVFORMAT_VERSION_MINOR << '.' << LIBAVFORMAT_VERSION_MICRO << '\n'
            << "        runtime version " << AV_VERSION_MAJOR (linked) << '.' << AV_VERSION_MINOR (linked) << '.' << AV_VERSION_MICRO (linked) << '\n';
            if (verbose) {
                std::cerr << "        configuration " << avformat_configuration() << '\n';
            }
        }
        {
            unsigned linked = avcodec_version();
            std::cerr << "      avcodec\n"
            << "        compile version " << LIBAVCODEC_VERSION_MAJOR << '.' << LIBAVCODEC_VERSION_MINOR << '.' << LIBAVCODEC_VERSION_MICRO << '\n'
            << "        runtime version " << AV_VERSION_MAJOR (linked) << '.' << AV_VERSION_MINOR (linked) << '.' << AV_VERSION_MICRO (linked) << '\n';
            if (verbose) {
                std::cerr << "        configuration " << avcodec_configuration() << '\n';
            }
        }
        {
            unsigned linked = avfilter_version();
            std::cerr << "      avfilter\n"
            << "        compile version " << LIBAVFILTER_VERSION_MAJOR << '.' << LIBAVFILTER_VERSION_MINOR << '.' << LIBAVFILTER_VERSION_MICRO << '\n'
            << "        runtime version " << AV_VERSION_MAJOR (linked) << '.' << AV_VERSION_MINOR (linked) << '.' << AV_VERSION_MICRO (linked) << '\n';
            if (verbose) {
                std::cerr << "        configuration " << avfilter_configuration() << '\n';
            }
        }
        {
            unsigned linked = avutil_version();
            std::cerr << "      avutil\n"
            << "        compile version " << LIBAVUTIL_VERSION_MAJOR << '.' << LIBAVUTIL_VERSION_MINOR << '.' << LIBAVUTIL_VERSION_MICRO << '\n'
            << "        runtime version " << AV_VERSION_MAJOR (linked) << '.' << AV_VERSION_MINOR (linked) << '.' << AV_VERSION_MICRO (linked) << '\n';
            if (verbose) {
                std::cerr << "        configuration " << avutil_configuration() << '\n';
            }
        }
#elif defined(WITH_AVFOUNDATION)
        std::cerr << "    avfoundation\n";
        std::cerr << "    avfoundation\n";
#endif
    };

    /** Determine the length of track remaining to play.
        @return The length in seconds, floating point, or a negative number
        if the value could not be ascertained.
        @note Although it "shouldn't" happen, return value may also be negative
        if actual playpoint exceeds expected duration, which can happen if
        ID3 data is wrong, encoding is wrong, etc. */
    float Player::playRemaining() const {
        float length = trackDuration();
        float point = playPoint();
        return (length < 0.0 || point < 0.0) ? -1 : length - point;
    }


    /** Get the a media player currently compiled in the code.
        @param settings Describe the output device.
        @param media_url A URL indicating the media to play.
        @param initial_gain A suggested gain for the media to play.
        This may be superceded if the media stream contains its own gain value.
        @return A media player.
        @throw AudioException or a derived exception in the event of media
        problems, output device problems, etc. */
    Media::Player *Player::getPlayer (const AudioSettings &settings,
                                      const std::string &media_url,
                                      float initial_gain) {
#ifdef WITH_FFMPEG
        return new Audio::LavPlayer (settings, media_url, initial_gain);
#elif defined(WITH_AVFOUNDATION)
        return Audio::getAVFoundationPlayer (settings, media_url, initial_gain);
#elif defined(WITH_GSTREAMER)
        return new Audio::GstreamerPlayer (settings, media_url, initial_gain);
#else
#error No media engine selected for inclusion.  Check configure settings and config.log.
#endif

    };

#ifdef WITH_GSTREAMER
    static GMainLoop *glib_main_loop = nullptr;     ///< Needed to dispatch events from bus.
    static std::thread *glib_run_loop_thread = nullptr; ///< Thread for the event dispatcher run loop.
#endif



    /** Initialize the media engine on startup. */
    Initializer::Initializer() {
#ifdef WITH_FFMPEG
#if LIBAVFORMAT_VERSION_MAJOR < 58
        av_register_all();
        avfilter_register_all();
#endif
        avformat_network_init();
#ifdef WITH_LIBAVDEVICE
        avdevice_register_all();
#endif
#endif

#ifdef WITH_GSTREAMER
        assert (glib_run_loop_thread == nullptr);
        gst_init (0, nullptr);

        glib_main_loop = g_main_loop_new (nullptr, false);
        glib_run_loop_thread = new std::thread {[]() {
            g_main_loop_run (glib_main_loop);
        }};
#endif
    }

    /** Uninitialize the media engine on prior to shutdown. */
    Initializer::~Initializer() {
#ifdef WITH_FFMPEG
        avformat_network_deinit();
#endif
#ifdef WITH_GSTREAMER
        g_main_loop_quit (glib_main_loop);
        g_main_context_wakeup (g_main_loop_get_context (glib_main_loop));
        glib_run_loop_thread->join();
        g_main_loop_unref (glib_main_loop);
        delete glib_run_loop_thread;
        glib_run_loop_thread = nullptr;
        gst_deinit();
#endif 
    }
}
