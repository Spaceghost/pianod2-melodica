///
/// Parser for audio options.
/// Parse audio options for creating or reconfiguring a room.
/// @file       audiooptionsparser.h - pianod2
/// @author     Perette Barella
/// @date       2015-11-23
/// @copyright  Copyright (c) 2015-2020 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <parsnip/parsnip_command.h>

#include "fundamentals.h"

/// Support for parsing audio options into the AudioSettings structure.
namespace AudioOptions {
    extern void validate (const AudioSettings);
    extern const Parsnip::OptionParser::Definitions &parser_definitions ();
    extern void extract_options (const Parsnip::Data &options, AudioSettings &settings);
}

