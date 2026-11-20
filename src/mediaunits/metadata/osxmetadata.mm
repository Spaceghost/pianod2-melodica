///
/// Read media file metadata using AVFoundation.
///	@file		osxmetadata.mm - pianod2
///	@author		Perette Barella
///	@date		2015-11-30
///	@copyright	  Copyright (c) 2015-2016 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cstring>
#include <cmath>
#include <cctype>
#include <cassert>

#include "fundamentals.h"
#include "logging.h"

#include "osxmetadata.h"

#include <AVFoundation/AVFoundation.h>

using namespace std;

namespace Media {

// #define DEVELOP

    enum class Key {
        Unknown,
        Artist,
        Album,
        Title,
        Genre,
        Comment,
        Year,
        Track,
        Disc
    };

    constexpr int32_t KeyCode (int32_t a, int32_t b, int32_t c, int32_t d) {
        return (a << 24 | b << 16 | c << 8 | d << 0);
    }

    static Key saneKey (const NSString *keySpace,
                        const NSObject *key,
                        const NSObject *commonKey) {
        if (commonKey != nil) {
            // There are key constants for these, but they aren't defined
            // until OS X 10.10.
            if ([commonKey isEqual: @"artist"]) {
                return Key::Artist;
            } else if ([commonKey isEqual: @"albumName"]) {
                return Key::Album;
            } else if ([commonKey isEqual: @"title"]) {
                return Key::Title;
            } else if ([commonKey isEqual: @"type"]) {
                return Key::Genre;
            }
#ifdef DEVELOP
            flog (LOG_WHERE (Log::GENERAL),
                  "Unknown common key: ", [[commonKey description] UTF8String]);
#endif
        }

        if (![keySpace isEqual: AVMetadataKeySpaceID3])
            return Key::Unknown;

        uint32_t k = [(NSNumber *) key integerValue];
        switch (k) {
            case KeyCode (0, 'A', 'R', 'T'):
            case KeyCode ('a', 'A', 'R', 'T'):
                return Key::Artist;
            case KeyCode ('d', 'i', 's', 'k'):
            case KeyCode (0, 'T', 'P', 'A'):
                return Key::Disc;
            case KeyCode ('t', 'r', 'k', 'n'):
            case KeyCode (0, 'T', 'R', 'K'):
                return Key::Track;
            case KeyCode (0, 'n', 'a', 'm'):
                return Key::Title;
            case KeyCode (0, 'a', 'l', 'b'):
                return Key::Album;
            case KeyCode (0, 'g', 'e', 'n'):
                return Key::Genre;
            case KeyCode (0, 'T', 'Y', 'E'):
                return Key::Year;
            case KeyCode (0, 'C', 'O', 'M'):
                return Key::Comment;
            case KeyCode (0, 'T', 'E', 'N'): // Encoder
            case KeyCode (0, 'T', 'C', 'M'): // Creator
                return Key::Unknown;
        }
#ifdef DEVELOP
        flog (LOG_WHERE (Log::GENERAL),
              "Unknown ID3 key: ",
              (char) ((k >> 24) & 0xff),
              (char) ((k >> 16) & 0xff),
              (char) ((k >> 8) & 0xff),
              (char) ((k >> 0) & 0xff));
#endif
        return Key::Unknown;
    }


    OSXMetadataReader::OSXMetadataReader (const std::string &file_path) {
        /* We need to request AVURLAsset take its time and give correct data rather than
            than giving half-baked but fast answers. */
        static NSDictionary *options;
        if (!options) {
            NSNumber *val = [[NSNumber alloc] initWithBool: YES];
            options = [[NSDictionary alloc] initWithObjectsAndKeys:val, AVURLAssetPreferPreciseDurationAndTimingKey, nil];
        }
        @autoreleasepool {
            NSString *path = [[[NSString alloc] initWithCString: file_path.c_str()
                                                       encoding: NSUTF8StringEncoding] autorelease];
            if (!path)
                throw bad_alloc ();
            NSURL *url = [NSURL fileURLWithPath:path];
            if (!url)
                throw MediaException ("Invalid path");
            AVAsset *asset = [[[AVURLAsset alloc] initWithURL:url options:options] autorelease];
            if (!asset)
                throw MediaException ("File cannot be opened");
            NSArray *availableFormats = [asset availableMetadataFormats];
            if ([availableFormats count] == 0)
                throw MediaException ("No metadata found");

#ifdef DEVELOP
            NSLog(@"%@ metadata formats: %@", path, availableFormats);
#endif
            for (NSString *format in availableFormats) {
                NSArray *formatMetadata = [asset metadataForFormat:format];
                for (AVMetadataItem *item in formatMetadata)
                {
                    Key key = saneKey ([item keySpace], [item key], [item commonKey]);
                    if (key == Key::Unknown) {
#ifdef DEVELOP
                        NSLog(@"[%@, %@] %@ %@", format, item.keySpace,
                              item.commonKey ? item.commonKey : item.key, [item.value description]);
#endif
                        continue;
                    }

                    const char *value = [[item.value description] UTF8String];
                    if (!value)
                        throw bad_alloc();

                    switch (key) {
                        case Key::Artist:
                            artist = value;
                            break;
                        case Key::Album:
                            album = value;
                            break;
                        case Key::Title:
                            title = value;
                            break;
                        case Key::Genre:
                            genre = value;
                            break;
                        case Key::Year:
                            year = [[item numberValue] integerValue];
                            break;
                        case Key::Track:
                        {
                            int old_track = track_number;
                            splitOf (value, &track_number, &track_count);
                            if (old_track && old_track != track_number) {
                                flog (LOG_WHERE (Log::WARNING), path, ": Disagreement on track number ",
                                      track_number, " vs. ", old_track);
                            }
                            break;
                        }
                        case Key::Disc:
                            splitOf (value, &disc_number, &disc_count);
                            break;
                        case Key::Comment:
                        {
                            NSDictionary *dick = (NSDictionary *) [item value];
                            NSString *ident = [dick objectForKey:@"identifier"];
                            if ([ident isEqual: @"iTunNORM"]) {
                                gain = gainFromiTunesNormalization ([[dick objectForKey:@"text"] UTF8String]);
                            } else if ([ident isEqual: @"iTunes_CDDB_IDs"]) {
                                cddb_id = [[dick objectForKey:@"text"] UTF8String];
                            } else {
#ifdef DEVELOP
                                NSLog (@"%@", dick);
#endif
                            }
                        }
                        case Key::Unknown:
                            break;
                    }
                }
            }
        }
    }
}
