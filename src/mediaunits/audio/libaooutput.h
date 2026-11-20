///
/// LibAO output.
/// Audio output support for playing via libao.
/// @file       libaooutput.h - pianod2
/// @author     Perette Barella
/// @date       2015-11-23
/// @copyright  Copyright (c) 2015-2016 Devious Fish. All rights reserved.
///

#pragma once

#ifndef pianod2_libaooutput_h
#define pianod2_libaooutput_h

#include <mutex>

#include <ao/ao.h>

#include "audio/audiooutput.h"

namespace Audio {
    /// Audio output class utilizing libao for output.
    class LibaoOutput : public Output {
    private:
        static std::mutex ao_mutex;
        ao_device *device = nullptr;
    public:
        LibaoOutput (const AudioSettings &settings,
                     const AudioFormat &format);
        virtual ~LibaoOutput () override;
        virtual bool play (void *buffer, unsigned numberOfBytes) override;
    };
}

#endif
