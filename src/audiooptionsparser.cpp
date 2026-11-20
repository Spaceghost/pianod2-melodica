///
/// Parser for audio options.
/// Parse audio options for creating or reconfiguring a room.
/// @file       audiooptionsparser.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-11-23
/// @copyright  Copyright (c) 2015-2020 Devious Fish. All rights reserved.
///

#include <config.h>

#include <parsnip/parsnip_command.h>

#include "interpreter.h"
#include "audiooptionsparser.h"
#include "audio/audiooutput.h"

#define KEY_DRIVER "driver"
#define KEY_DEVICE "device"
#define KEY_ID "id"
#define KEY_SERVER "server"
#define KEY_LIBRARY "library"
#define KEY_VOLUME_LEVEL "volume"
#define KEY_CROSSFADE_TIME "crossfadeSeconds"
#define KEY_CROSSFADE_LEVEL "crossfadeLevel"
#define KEY_PREROLL_TIME "prerollSeconds"

namespace AudioOptions {

    /** Validate audio settings.
        @param settings The audio settings to validate.
        @throw An appropriate command error if the settings are invalid. */
    void validate (const AudioSettings settings) {
        if (settings.preroll_time == 0.0f && settings.crossfade_time > 0.0f) {
            throw CommandError (E_INVALID, "Cannot crossfade without preroll.");
        }
        try {
            if (!Audio::Output::isValidOutput (settings)) {
                throw CommandError (E_NOTFOUND, settings.output_library);
            }
        } catch (const Audio::AudioException &e) {
            throw CommandError (E_INVALID, e.what());
        }
        if (settings.preroll_time > 0.0f) {
            try {
                if (!Audio::Output::outputCanCrossfade (settings)) {
                    throw CommandError (E_INVALID, "Cannot crossfade.");
                }
            } catch (const Audio::AudioException &e) {
                throw CommandError (E_INVALID, e.what());
            }
        }
    }

    const Parsnip::OptionParser::Definitions &parser_definitions () {
        static const Parsnip::OptionParser::Definitions audio_option_statements {
            "library {" KEY_LIBRARY "}",  // Choose output library
            "driver {" KEY_DRIVER "}",    // output setting
            "device {" KEY_DEVICE "}",    // output setting
            "id {" KEY_ID "}",            // output setting
            "options {" KEY_OPTIONS "}",  // output setting
            "server {" KEY_SERVER "}",    // output setting
            "volume {#" KEY_VOLUME_LEVEL ":-100-100} ", // room volume
            "crossfade duration {#" KEY_CROSSFADE_TIME ":0.0-15}",  // How much volume adjustment when crossfading
            "crossfade level {#" KEY_CROSSFADE_LEVEL ":0.0-50}",    // How long to overlap songs
            "preroll duration {#" KEY_PREROLL_TIME ":0.0-30}",      // Advance cueing time to allow buffering
        };
        return audio_option_statements;
    }

    /** Extract audio options and put them in an AudioSettings structure. */
    void extract_options (const Parsnip::Data &options, AudioSettings &settings) {
        if (options.empty()) {
            return;
        }
        if (options.contains (KEY_DRIVER)) {
            settings.output_driver = options [KEY_DRIVER].asString ();
        }
        if (options.contains (KEY_DEVICE)) {
            settings.output_device = options [KEY_DEVICE].asString ();
        }
        if (options.contains (KEY_ID)) {
            settings.output_id = options [KEY_ID].asString ();
        }
        if (options.contains (KEY_OPTIONS)) {
            settings.output_options = options [KEY_OPTIONS].asString();
        }
        if (options.contains (KEY_SERVER)) {
            settings.output_server = options [KEY_SERVER].asString ();
        }
        if (options.contains (KEY_LIBRARY)) {
            settings.output_library = options [KEY_LIBRARY].asString ();
        }
        if (options.contains (KEY_VOLUME_LEVEL)) {
            settings.volume = options [KEY_VOLUME_LEVEL].asInteger ();
        }
        if (options.contains (KEY_CROSSFADE_TIME)) {
            settings.crossfade_time = options [KEY_CROSSFADE_TIME].asDouble();
        }
        if (options.contains (KEY_CROSSFADE_LEVEL)) {
            settings.crossfade_level = options [KEY_CROSSFADE_LEVEL].asDouble ();
        }
        if (options.contains (KEY_PREROLL_TIME)) {
            settings.preroll_time = options [KEY_PREROLL_TIME].asDouble ();
        }
        validate (settings);
    }

}  // namespace AudioOptions
