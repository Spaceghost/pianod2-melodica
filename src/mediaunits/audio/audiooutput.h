///
/// Class to abstract away details of audio output system is in use.
/// @file       audiooutput.h - pianod2
/// @author     Perette Barella
/// @date       2015-02-26
/// @copyright  Copyright (c) 2015-2017 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include "fundamentals.h"

/// Classes dealing with audio output drivers and decoding/playing audio files.
namespace Audio {
    class LavGenericAdapter;

    enum SampleArrangement {
        Native,
        Big,
        Little
    };
    enum SampleSignedness {
        Signed,
        Unsigned
    };
    /** Audio format, somewhat ripped off from libao */
    struct AudioFormat {
        int bits = 16; ///< Bits per sample *in a single channel*
        int rate = 44100; ///< Samples per second *in a single channel*
        int channels = 2;
        SampleArrangement arrangement = SampleArrangement::Native;
        SampleSignedness signedness = Signed;
        SampleArrangement realArrangement () const;
        bool isNativeArrangement () const;
        /** Get the size of sample data.
            @return Number of channels * sizeof (individual channel's sample). */
        int sampleGroupSize () const {
            assert ((bits & 0x07) == 0);
            return channels * ((bits + 7) / 8);
        };
        static SampleArrangement nativeArrangement (int bits);
    };

    /// Exception for audio output problems.
    class AudioException : public std::exception {
    protected:
        std::string reason;
    public:
        AudioException (const char *why = "") : reason (why) { };
        AudioException (std::string why) : reason (why) { };
        virtual const char *what() const noexcept override  { return reason.c_str(); };
    };


    /// Initialize the audio output libraries; uninitialize on destruction.
    class Initializer {
    public:
        Initializer ();
        ~Initializer ();
    };

    /// Base audio output class (abstract).
    class Output {
    protected:
        char bytes_per_sample_set; // Bytes/channel * number of channels
    public:
        virtual ~Output () { };
        /** Play output.
            @param buffer The samples, in packed (interleaved) format if multichannel.
            @param number_of_bytes Size of the buffer; number of samples is determined
            by the audio format set when opening the channel. */
        virtual bool play (void *buffer, unsigned number_of_bytes) = 0;
        inline char bytesPerSample() { return bytes_per_sample_set; };

        static bool isValidOutput (const AudioSettings &settings);
        static bool outputCanCrossfade (const AudioSettings &settings);

        static Output *getOutput (const AudioSettings &settings,
                                  const AudioFormat &format);
        static void reportLibrariesAndVersions (int verbose);
    };
}

