///
/// Tone generator.
/// @file       tonegenplayer.cpp - pianod
/// @author     Perette Barella
/// @date       2014-12-04
/// @copyright  Copyright 2014-2021 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <cmath>

#include <mutex>

#include "fundamentals.h"
#include "logging.h"
#include "mediaplayer.h"
#include "audio/audiooutput.h"
#include "tonegen.h"

#define TONE_SAMPLE_FREQ (44100)
#define RG_SCALE_FACTOR (100)


namespace ToneGenerator {
    Player::Player (const AudioSettings &a, const PianodSong *s):
    audio (a), song (static_cast<const Song *> (s)), media (song->media()) {
        // Don't crash if a nonexistent song was restored from persistence file
        if (!media)
            throw Audio::AudioException ("Song has been removed");
        
        // Calculate the duration of tone and length of the buffer needed
        for (const TONE_SEQUENCE *ts = media->tones; ts->duration; ts++) {
            duration += ts->duration;
            samples += ts->duration * TONE_SAMPLE_FREQ;
        }
    }

    void Player::createTone (int volume) {
        static std::mutex m;
        {
            // Mutex to prevent volume thread fighting with initial tone creation.
            std::lock_guard<std::mutex> lock (m);

            if (tone == nullptr) {
                tone.reset (new AUDIO_SAMPLE [samples]);
            }
        }
        float left_volume = (song->channel != Channel::RIGHT ? 1.0f : 0.0f);
        float right_volume = (song->channel != Channel::LEFT ? 1.0f : 0.0f);

        float multiplier = pow (10.0, volume / 20.0) * 0x7fff;
        // Calculate the duration of tone and length of the buffer needed
        long sample_point = 0;
        long last_crossover = 0;
        bool last_was_positive = true;
        for (const TONE_SEQUENCE *ts = media->tones; ts->duration; ts++) {
            long ts_samples = ts->duration * TONE_SAMPLE_FREQ;
            assert (sample_point + ts_samples <= samples);

            // Determine the tone segement details
            int f1, f2; // Frequencies 
            float a1, a2; // Amplitudes 
            if ((f1 = ts->firstFreq)) {
                assert (ts->relativeVolume > 0 && ts->relativeVolume <= 1.0);
                a1 = ts->relativeVolume;
                if ((f2 = ts->secondFreq)) {
                    a2 = (1.0 - a1);
                } else {
                    f2 = 1000;
                    a2 = 0;
                }
            } else {
                assert (ts->secondFreq == 0);
                f1 = 1000;
                f2 = 1000;
                a1 = 0;
                a2 = 0;
            }

            a1 *= multiplier;
            a2 *= multiplier;

            // Add the tone segment to the tone buffer
            for (long s = 0; s < ts_samples; s++) {
                float sample = ((a1 * sin (2 * M_PI * f1 * ((float) s / TONE_SAMPLE_FREQ))) +
                                (a2 * sin (2 * M_PI * f2 * ((float) s / TONE_SAMPLE_FREQ))));
                tone.get() [sample_point].left = sample * left_volume;
                tone.get() [sample_point].right = sample * right_volume;
                bool is_positive = sample >= 0.0;
                if (sample == 0 || is_positive != last_was_positive) {
                    last_crossover = sample_point;
                    last_was_positive = is_positive;
                }
                sample_point++;
            }
            // Truncate from the last zero-crossing to the current sample,
            // to eliminate "clicking" at end of tones.
            for (auto s = last_crossover; s < sample_point; s++) {
                tone.get() [s].left = 0;
                tone.get() [s].right = 0;
            }
        }
    }

    void Player::setVolume (float volume) {
        createTone (volume);
    }

    // Data retrieval functions 
    float Player::trackDuration (void) const {
        return media->toneRepetitions * duration;
    };
    float Player::playPoint (void) const {
        return duration * current_rep + seconds_into_rep;
    }

    Media::Player::State Player::currentState (void) const  {
        if (repetitions == 0) {
            return Initializing;
        }
        if (current_rep < repetitions) {
            return Playing;
        }
        return Done;
    };

    RESPONSE_CODE Player::playerThread (void) {
        Audio::AudioFormat format;
        try {
            output = Audio::Output::getOutput (audio, format);
            if (!output) {
                throw Audio::AudioException ("No output device");
            }

            // Create the test tone.
            createTone(audio.volume);

            repetitions = media->toneRepetitions;
            const int frame_size = 1024;
            while (current_rep < repetitions && !checkForPauseOrQuit()) {
                for (int offset = 0; offset < samples; offset += frame_size) {
                    seconds_into_rep = offset / float (TONE_SAMPLE_FREQ);
                    int count = samples - offset;
                    if (count > frame_size)
                        count = frame_size;
                    output->play (tone.get() + offset, count * sizeof (*tone));
                    if (checkForPauseOrQuit())
                        break;
                }
                current_rep++;
            }
            current_rep = repetitions; // In case of quit 
            seconds_into_rep = 0;

            delete output;
        } catch (...) {
            repetitions = 1;
            current_rep = repetitions;
            seconds_into_rep = 0;
            throw;
        }
        return S_OK;
    }

}
