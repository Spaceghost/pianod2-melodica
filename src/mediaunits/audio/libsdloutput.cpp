///
/// SDL output.
/// Audio output module for Simple DirectMedia Layer (libsdl).
/// Note libsdl cannot open the same device multiple times, and therefore
/// is incapable of crossfading.
/// @file       libsdloutput.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-11-19
/// @copyright  Copyright (c) 2015-2021 Devious Fish. All rights reserved.
/// @see        https://www.libsdl.org
/// @see        https://www.libsdl.org/release/SDL-1.2.15/docs/html/guideaudioexamples.html
///

#include <config.h>

#include <stdexcept>
#include <mutex>
#include <condition_variable>

#include "audio/audiooutput.h"
#include "libsdloutput.h"

namespace Audio {
    /** The audio function callback takes the following parameters:
        buffer:  A pointer to the audio buffer to be filled
        length:  The length (in bytes) of the audio buffer */
    static void audio_fetch_callback (void *context, uint8_t *buffer, int length) {
        static_cast<LibsdlOutput *> (context)->fetch (buffer, length);
    }

    /// Mutex to restrict concurrent calls into libsdl API.
    std::mutex LibsdlOutput::sdl_mutex;

    /** Open audio output using libAO.
     @param settings Device/ driver/ host to use for output.
     @param format The format in which samples will arrive for output. */
    LibsdlOutput::LibsdlOutput (const AudioSettings &settings,
                                const AudioFormat &format) :
    cirque_size (cirque_samples * format.sampleGroupSize()) {
        if (settings.crossfade_time > 0)
            throw AudioException ("libsdl cannot crossfade");

        const uint16_t FORMAT_SIGNED_FLAG = 0x8000;
        const uint16_t FORMAT_UNSIGNED_FLAG = 0;
        const uint16_t FORMAT_BIG_ENDIAN = 0x1000;
        const uint16_t FORMAT_LITTLE_ENDIAN = 0;

        uint16_t sign = (format.signedness == SampleSignedness::Signed ?
                         FORMAT_SIGNED_FLAG : FORMAT_UNSIGNED_FLAG);
        uint16_t endo = (format.realArrangement() == SampleArrangement::Big ?
                         FORMAT_BIG_ENDIAN : FORMAT_LITTLE_ENDIAN);
        if ((format.bits & 0x7) || format.bits > 32)
            throw AudioException ("Invalid bits/sample " + to_string (format.bits));

        /* Construct the audio format */
        SDL_AudioSpec params = {};
        params.freq = format.rate;
        params.format = sign | endo | format.bits;
        params.channels = format.channels;
        params.samples = block_samples;
        params.callback = audio_fetch_callback;
        params.userdata = this;

        bytes_per_sample_set = format.sampleGroupSize();
        audio_cirque = new uint8_t [cirque_size];

        /* Open the audio device, forcing the desired format */
        std::lock_guard<std::mutex> lock (sdl_mutex);
#if SDL_VERSION >= 2
        device_id = SDL_OpenAudioDevice (settings.output_device.c_str(),
                                         false, // Open for output
                                         &params,
                                         nullptr, // Granted audio parameters
                                         0); // No freq etc changes
        if (!device_id) {
            delete audio_cirque;
            throw AudioException (SDL_GetError());
        }
        SDL_PauseAudioDevice (device_id, false);
#else
        (void)settings; // quiet compile warning when this isn't needed.
        int status = SDL_OpenAudio (&params, nullptr);
        if (status < 0) {
            delete audio_cirque;
            throw AudioException (SDL_GetError());
        }
        SDL_PauseAudio (false);
#endif
    }

    /** Fetch output as requested. */
    void LibsdlOutput::fetch (uint8_t *buffer, int length) {
        BufferIndex reader = cirque_read;
        do {
            BufferIndex writer = cirque_write;

            if (reader == writer)
                return;

            unsigned bytes_ready = (writer > reader ? writer - reader : cirque_size - reader);
            if (bytes_ready > length)
                bytes_ready = length;
            SDL_MixAudio (buffer, audio_cirque + reader, bytes_ready, SDL_MIX_MAXVOLUME);
            reader += bytes_ready;
            assert (reader <= cirque_size);
            if (reader == cirque_size)
                reader = 0;
            unique_lock<mutex> locker (data_mutex);
            cirque_read = reader;
            notifier.notify_one();

            buffer += bytes_ready;
            length -= bytes_ready;
        } while (length > 0);
    }

    /** Play output.
        @param buffer The samples, in packed (interleaved) format if multichannel.
        @param number_of_bytes Size of the buffer; number of samples is determined
        by the audio format set when opening the channel. */
    bool LibsdlOutput::play (void *buffer, unsigned number_of_bytes) {
        uint8_t *samples = static_cast<uint8_t *> (buffer);
        BufferIndex writer = cirque_write;
        while (true) {
            BufferIndex reader = cirque_read;
            unsigned writable = (writer < reader ? (reader - writer) - 1 :
                                 reader == 0 ? (cirque_size - writer) - 1 :
                                 cirque_size - writer);
            if (writable > number_of_bytes)
                writable = number_of_bytes;
            memcpy (audio_cirque + writer, samples, writable);
            samples += writable;
            number_of_bytes -= writable;

            writer += writable;
            assert (writer <= cirque_size);
            if (writer == cirque_size)
                writer = 0;
            cirque_write = writer;

            // Check for done
            if (number_of_bytes == 0)
                return true;

            // Go again if we are just circling to the start of the buffer
            reader = cirque_read;
            if (writer == 0 && reader > 1)
                continue;

            // Queue is full; wait for it to empty a bit.
            unique_lock<mutex> locker (data_mutex);
            reader = cirque_read;
            if (writer == reader - 1 || (reader == 0 && writer == cirque_size - 1)) {
                // Wait on a condition variable.
                // fetch() notifies when there's space in the buffer.
                notifier.wait (locker);
            }
        }
    };

    LibsdlOutput::~LibsdlOutput () {
        // Wait for the buffer to play out.
        {
            unique_lock<mutex> locker (data_mutex);
            while (cirque_read != cirque_write) {
                notifier.wait (locker);
            }
        }
        
        std::lock_guard<std::mutex> lock (sdl_mutex);
#if SDL_VERSION >= 2
        SDL_CloseAudioDevice (device_id);
#else
        SDL_CloseAudio();
#endif
    };
    
}
