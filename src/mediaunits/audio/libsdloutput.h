///
/// SDL output.
/// Audio output module for Simple DirectMedia Layer (libsdl).
/// Note libsdl cannot open the same device multiple times, and therefore
/// is incapable of crossfading.
/// @file       libsdloutput.h - pianod2
/// @author     Perette Barella
/// @date       2015-11-23
/// @copyright  Copyright (c) 2015-2016 Devious Fish. All rights reserved.
///

#pragma once

#ifndef pianod2_libsdloutput_h
#define pianod2_libsdloutput_h

#include <mutex>
#include <condition_variable>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"
#include <SDL/SDL.h>
#include <SDL/SDL_version.h>
#include <SDL/SDL_audio.h>
#pragma GCC diagnostic pop

#include "audio/audiooutput.h"

namespace Audio {
    /// Audio output class utilizing SDL for output.
    class LibsdlOutput : public Output {
    private:
#if SDL_VERSION >= 2
        SDL_AudioDeviceID device_id;
#endif
        static std::mutex sdl_mutex;
        std::mutex data_mutex;
        std::condition_variable notifier;

        using BufferIndex = uint32_t;

        uint8_t *audio_cirque = nullptr;
        const BufferIndex block_samples = 0x400;
        const BufferIndex block_count = 0x10;
        const BufferIndex cirque_samples = block_samples * block_count;
        const BufferIndex cirque_size;
        volatile BufferIndex cirque_write = 0; // Only written by play().
        volatile BufferIndex cirque_read = 0; // Only writte by fetch().
    public:
        LibsdlOutput (const AudioSettings &settings,
                      const AudioFormat &format);
        virtual ~LibsdlOutput () override;
        void fetch (Uint8 *buffer, int length);
        virtual bool play (void *buffer, unsigned numberOfBytes) override;
    };
}

#endif
