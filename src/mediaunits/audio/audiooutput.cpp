///
/// Class to abstract away details of audio output system is in use.
/// @file       audiooutput.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-02-26
/// @copyright  Copyright (c) 2015-2017 Devious Fish. All rights reserved.
///

#include <config.h>

#include <iostream>
#include <memory>

#include <climits>
#include <cstdint>

#include "fundamentals.h"
#include "utility.h"
#include "audio/audiooutput.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"

#ifdef WITH_LIBSDL
#include "libsdloutput.h"
#endif
#ifdef WITH_LIBAO
#include "libaooutput.h"
#endif
#ifdef WITH_GSTREAMER
#include "gstreamplayer.h"
#endif
#ifdef WITH_LIBAVDEVICE
#include "avdeviceoutput.h"
extern "C" {
#include <libavdevice/avdevice.h>
}
// Libav doesn't provide these macros.  Yet another source of irritation it provides.
#ifndef AV_VERSION_MAJOR
#define AV_VERSION_MAJOR(x) ((x) >> 16)
#define AV_VERSION_MINOR(x) (((x) >> 8) & 0xff)
#define AV_VERSION_MICRO(x) ((x) & 0xff)
#endif
#endif

#pragma GCC diagnostic pop

namespace Audio {

#if CHAR_BIT == 8
    enum
    {
        B4_LITTLE_ENDIAN = 0x03020100ul,
        B4_BIG_ENDIAN = 0x00010203ul,
        B4_PDP_ENDIAN = 0x01000302ul,
        B2_LITTLE_ENDIAN = 0x0100,
        B2_BIG_ENDIAN = 0x0001
    };
#else
#error "unsupported char size"
#endif

    /// Constants to determine byte ordering on-the-fly.
    /// Credit to Cristoph
    /// @see http://stackoverflow.com/questions/2100331/c-macro-definition-to-determine-big-endian-or-little-endian-machine
    static const union {
        unsigned char bytes[4];
        uint32_t value;
    } B4_host_order = { { 0, 1, 2, 3 } };
    /// Constants to determine byte ordering of 2-byte values.
    static const union {
        unsigned char bytes[2];
        uint16_t value;
    } B2_host_order = { { 0, 1 } };

    /** Return the byte ordering scheme, translating
        "native" ordering into big or little or an exception.
        @return Big or Little endian. */
    SampleArrangement AudioFormat::realArrangement () const {
        if (arrangement != SampleArrangement::Native) {
            return arrangement;
        }
        return nativeArrangement (bits);
    };

    /** Determine whether the byte ordering matches the native ordering. */
    bool AudioFormat::isNativeArrangement () const {
        if (arrangement == SampleArrangement::Native || bits == 8)
            return true;
        try {
            return (arrangement == nativeArrangement (bits));
        } catch (...) {
            return false;
        }
    }

    /** Determine the byte ordering of the machine. */
    SampleArrangement AudioFormat::nativeArrangement (int bits) {
        if (bits == 32) {
            if (B4_host_order.value == B4_LITTLE_ENDIAN)
                return SampleArrangement::Little;
            if (B4_host_order.value == B4_BIG_ENDIAN)
                return SampleArrangement::Big;
        } else {
            if (B2_host_order.value == B2_LITTLE_ENDIAN)
                return SampleArrangement::Little;
            if (B2_host_order.value == B2_BIG_ENDIAN)
                return SampleArrangement::Big;
        }
        throw AudioException ("Weird host byte ordering");
    }



    /** Check if audio settings are valid.
        @param settings Where to send the audio output.
        @return True if valid, false otherwise.
        @throw AudioException or a derivative if opening/setting up
        the device failed. */
    bool Output::isValidOutput(const AudioSettings &settings) {
#if (defined(WITH_FFMPEG) || defined(WITH_GSTREAMER)) && defined(WITH_AVFOUNDATION)
        if (strcasecmp (settings.output_library, "avfoundation") == 0)
            return true;
#endif
#ifndef WITH_AVFOUNDATION
        std::unique_ptr <Output> output { getOutput(settings, AudioFormat {}) };
        return (output && output);
#endif
        return true;
    }


    /** Check if audio settings are valid.
     @param settings Where to send the audio output.
     @return True if valid, false otherwise.
     @throw AudioException or a derivative if opening/setting up
     the device failed. */
    bool Output::outputCanCrossfade (const AudioSettings &settings) {
#if (defined(WITH_FFMPEG) || defined(WITH_GSTREAMER)) && defined(WITH_AVFOUNDATION)
        if (strcasecmp (settings.output_library, "avfoundation") == 0)
            return true;
#endif
#ifndef WITH_AVFOUNDATION
        std::unique_ptr <Output> output { getOutput(settings, AudioFormat {}) };
        std::unique_ptr <Output> output_2 { getOutput (settings, AudioFormat {}) };
        return output && output_2;
#endif
        return true;
    }


    /** Factory gets whatever kind of output is best, or
        requested by the audio settings.
        @param settings Where to send the audio output.
        @param format The form in which the audio data will arrive.
        @return A ready-to-go output device, or a nullptr if there is no
        matching device.
        @throw AudioException or a derivative if opening/setting up
        the device failed. */
    Output *Output::getOutput (const AudioSettings &settings,
                               const AudioFormat &format) {
#ifdef WITH_LIBSDL
        if (settings.output_library.empty() ||
            strcasecmp (settings.output_library, "libsdl") == 0) {
            return new LibsdlOutput (settings, format);
        }
#endif
#ifdef WITH_LIBAO
        if (settings.output_library.empty() ||
            strcasecmp (settings.output_library, "libao") == 0) {
            return new LibaoOutput (settings, format);
        }
#endif
#ifdef WITH_GSTREAMER
        if (settings.output_library.empty() ||
            strcasecmp (settings.output_library, "gstreamer") == 0) {
            return new GstreamerOutput (settings, format);
        }
#endif
#ifdef WITH_LIBAVDEVICE
        if (settings.output_library.empty() ||
            strcasecmp (settings.output_library, "libavdevice") == 0) {
            return new AvDeviceOutput (settings, format);
        }
#endif
        return nullptr;
    };

    /** Report audio libraries in use, and their versions.
        @param verbose If > 0, include more detail. */
    void Output::reportLibrariesAndVersions (int verbose) {
        std::cerr << "  Audio outputs:\n";
#ifdef WITH_LIBSDL
        {
            SDL_version compiled;

            SDL_VERSION(&compiled);
            std::cerr << "    libsdl\n"
            << "      compile version " << (unsigned) compiled.major << '.' << (unsigned) compiled.minor << '.' << (unsigned) compiled.patch << '\n';
#ifdef HAVE_SDL_GETVERSION
            SDL_version linked;
            SDL_GetVersion(&linked);
            std::cerr << "      runtime version " << (unsigned) linked.major << '.' << (unsigned) linked.minor << '.' << (unsigned) linked.patch << '\n';
#endif
        }
#endif
#ifdef WITH_LIBAO
        std::cerr << "    libao\n";
#endif
#ifdef WITH_LIBAVDEVICE
        {
            unsigned compiled = LIBAVDEVICE_BUILD;
            unsigned linked = avdevice_version();

            std::cerr << "    libavdevice\n"
            << "      compile version " << AV_VERSION_MAJOR (compiled) << '.' << AV_VERSION_MINOR (compiled) << '.' << AV_VERSION_MICRO (compiled) << '\n'
            << "      runtime version " << AV_VERSION_MAJOR (linked) << '.' << AV_VERSION_MINOR (linked) << '.' << AV_VERSION_MICRO (linked) << '\n';
            if (verbose) {
                std::cerr << "      configuration " << avdevice_configuration() << '\n';
            }
        }
#endif
#ifdef WITH_GSTREAMER
        (void) verbose;
        std::cerr << "    gstreamer\n";
#endif
    };





    /** Initialize the audio libraries on startup, if required. */
    Initializer::Initializer () {
#ifdef WITH_LIBSDL
        if (SDL_Init (SDL_INIT_AUDIO) != 0) {
            throw InitializationException ("libsdl: SDL_Init", SDL_GetError());
        }
#endif
#ifdef WITH_LIBAO
        ao_initialize ();
#endif
    };

    /** Uninitialize the audio libraries prior to shutdown. */
    Initializer::~Initializer () {
#ifdef WITH_LIBAO
        ao_shutdown ();
#endif
#ifdef WITH_LIBSDL
        SDL_Quit();
#endif
    };

}

