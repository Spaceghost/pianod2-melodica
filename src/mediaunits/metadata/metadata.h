///
/// Metadata support.
/// Extract metadata from media files and return in a standard format.
/// @file       metadata.h - pianod2
/// @author     Perette Barella
/// @date       2015-11-30
/// @copyright  Copyright (c) 2015-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <string>
#include <exception>


namespace Media {
    /** An exception thrown by metadata readers when metadata cannot be
        extracted. */
    class MediaException : public std::exception {
        const char *reason = nullptr;
    public:
        MediaException (const char *why) : reason (why) { };
        virtual const char *what() const noexcept override { return reason; };
    };
    
    class Metadata {
    protected:
        static void splitOf (const char *value, int *first, int *second);
        static void splitOf (const std::string &value, int *first, int *second) {
            splitOf (value.c_str(), first, second);
        };
    public:
        static float gainFromiTunesNormalization (const char *);
        static float gainFromiTunesNormalization (const std::string &value) {
            return gainFromiTunesNormalization (value.c_str());
        }
    public:
        std::string artist;
        std::string album;
        std::string title;
        std::string cddb_id;
        std::string genre;
        int year = 0;
        int track_number = 0;
        int track_count = 0;
        int disc_number = 0;
        int disc_count = 0;
        float duration = 0;
        float gain = 0;

        static float getFileGain (const std::string &filename);
        static const Metadata getMetadata (const std::string &filename);
    };
}

