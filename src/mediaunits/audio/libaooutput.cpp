///
/// LibAO output.
/// Audio output support for playing via libao.
/// @file       libaooutput.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-11-18
/// @copyright  Copyright (c) 2015-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <stdexcept>

#include <errno.h>

#include <ao/ao.h>

#include "logging.h"
#include "audio/audiooutput.h"
#include "libaooutput.h"

namespace Audio {
    /// Mutex to restrict concurrent calls into libao API.
    std::mutex LibaoOutput::ao_mutex;

    /** Open audio output using libAO.
        @param settings Device/ driver/ host to use for output.
        @param data_format The format in which samples will arrive for output. */
    LibaoOutput::LibaoOutput (const AudioSettings &settings,
                              const AudioFormat &data_format) {

        ao_sample_format format = {};
        format.bits = data_format.bits;
        format.rate = data_format.rate;
        format.channels = data_format.channels;
        format.byte_format = (data_format.arrangement == SampleArrangement::Big ? AO_FMT_BIG :
                              data_format.arrangement == SampleArrangement::Little ? AO_FMT_LITTLE :
                              AO_FMT_NATIVE);

        // Find driver, or use default if unspecified.
        int audio_out_driver = (settings.output_driver.empty()
                                ? ao_default_driver_id()
                                : ao_driver_id (settings.output_driver.c_str()));
        if (audio_out_driver < 0) {
            flog (LOG_WHERE (Log::ERROR), "Audio driver ",
                  settings.output_driver.empty() ? "(default)" : settings.output_driver.c_str(),
                  " not found");
            throw std::invalid_argument ("Invalid audio driver");
        }

        // Create a list of ao_options
        ao_option *options = nullptr;
        ao_append_option(&options, "client_name", PACKAGE);
        if (!settings.output_device.empty()) {
            ao_append_option (&options, "dev", settings.output_device.c_str());
        }
        if (!settings.output_id.empty()) {
            ao_append_option (&options, "id", settings.output_id.c_str());
        }
        if (!settings.output_server.empty()) {
            ao_append_option (&options, "server", settings.output_server.c_str());
        }
        {
            std::lock_guard<std::mutex> lock (ao_mutex);
            device = ao_open_live (audio_out_driver,
                                   & const_cast <ao_sample_format &> (format),
                                   options);
        }
        ao_free_options (options);
        if (device == NULL) {
            const char *reason = (errno == AO_ENODRIVER ? "No driver" :
                                  errno == AO_ENOTLIVE ? "Not a live output device" :
                                  errno == AO_EBADOPTION ? "Bad option" :
                                  errno == AO_EOPENDEVICE ? "Cannot open device" : "Other failure");
            flog (LOG_WHERE (Log::ERROR), "Cannot open audio device ",
                  settings.output_device.empty() ? "default" : settings.output_device.c_str(),
                  "/",
                  settings.output_id.empty() ? "default" : settings.output_id.c_str(),
                  "/",
                  settings.output_server.empty() ? "default" : settings.output_server.c_str(),
                  ": ", reason);
            throw AudioException (std::string (__func__) + ": " + reason);
        }

        bytes_per_sample_set = data_format.sampleGroupSize();
    }

    /** Play output.
        @param buffer The samples, in packed (interleaved) format if multichannel.
        @param number_of_bytes Size of the buffer; number of samples is determined
        by the audio format set when opening the channel. */
    bool LibaoOutput::play (void *buffer, unsigned number_of_bytes) {
        std::lock_guard<std::mutex> lock (ao_mutex);
        return ao_play(device, (char *) buffer, number_of_bytes);
    };

    LibaoOutput::~LibaoOutput () {
        std::lock_guard<std::mutex> lock (ao_mutex);
        ao_close (device);
    };

}
