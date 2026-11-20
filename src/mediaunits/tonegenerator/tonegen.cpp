///
/// Tone sequence definitions.
/// Frequencies, durations and descriptions of commonly-known
/// tones for test tone purposes.
/// @file       tonegen.cpp - pianod
/// @author     Perette Barella
/// @date       2014-12-04
/// @copyright  Copyright 2014-2021 Devious Fish.  All rights reserved.
///

#include <config.h>

#include "fundamentals.h"
#include "utility.h"
#include "musiclibrary.h"
#include "tonegen.h"

// #define DOLBY_TEST

namespace ToneGenerator {
    static const TONE_ALBUM telephonys_best = {
        "AT&T/Bell Labs",
        "Telephony's Best Noises",
        "Telephony",
        NULL
    };
   static const TONE_SEQUENCE dial_tone[] = {
        { 1.0,      350,        440,        .5 },
        { 0 }
    };

   static const TONE_SEQUENCE ring_back[] = {
        { 2.0,      440,        480,        .5 },
        { 4.0,      0,          0,          .5 },
        { 0 }
    };

    static const TONE_SEQUENCE busy[] = {
        { 0.5,      480,        620,        .5 },
        { 0.5,      0,          0,          .5 },
        { 0 }
    };

    static const TONE_SEQUENCE fast_busy [] = {
        { 0.3,       480,        620,        .5 },
        { 0.2,       0,          0,          .5 },
        { 0 }
    };

    static const TONE_ALBUM sit_tones = {
        "AT&T/Bell Labs",
        "Special Information Tones",
        "Telephony",
        NULL
    };

    static const TONE_SEQUENCE no_circuit [] = {
        { 0.380,     985.2,        0,        1 },
        { 0.380,     1428.5,       0,        1 },
        { 0.380,     1776.7,       0,        1 },
        { 0 }
    };

    static const TONE_SEQUENCE vacant_circuit [] = {
        { 0.380,     985.2,        0,        1 },
        { 0.274,     1370.6,       0,        1 },
        { 0.380,     1776.7,       0,        1 },
        { 0 }
    };

    static const TONE_SEQUENCE op_intercept [] = {
        { 0.274,    913.8,          0,        1 },
        { 0.274,    1370.6,         0,        1 },
        { 0.380,    1776.7,         0,        1 },
        { 0 }
    };

    static const TONE_SEQUENCE reorder [] = {
        { 0.274,    913.8,         0,        1 },
        { 0.380,    1428.5,        0,        1 },
        { 0.380,    1776.7,        0,        1 },
        { 0 }
    };

    static const TONE_ALBUM common_tones = {
        "Traditional/Assorted Artists",
        "Test Tones of the 1980s",
        "Miscellaneous Tones",
        NULL
    };

    static const TONE_SEQUENCE a440 [] = {
        { 1.0,     440.0,        0,        1 },
        { 0 }
    };

    static const TONE_SEQUENCE hz1000 [] = {
        { 1.0,     1000.0,        0,        1 },
        { 0 }
    };

    static const TONE_SEQUENCE legacy_ebs [] = {
        { 1.0,      853.0,        960.0,    0.5 },
        { 0 }
    };



    static const TONE_ALBUM westminster = {
        "Westminster Chimes",
        "Big Ben",
        "Other Test Sounds",
        NULL
    };

#define G_SHARP (414.30)
#define F_SHARP (369.99)
#define E_PLAIN (329.63)
#define B_PLAIN (246.94)
    static const TONE_SEQUENCE quarter_hour [] = {
        { 0.6,      G_SHARP,        0,      1 }, // Sequence 1
        { 0.2,      0,              0,      1 },
        { 0.6,      F_SHARP,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      E_PLAIN,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      B_PLAIN,        0,      1 },
        { 0 }
    };
    
    static const TONE_SEQUENCE half_hour [] = {
        { 0.6,      E_PLAIN,        0,      1 }, // Sequence 2
        { 0.2,      0,              0,      1 },
        { 0.6,      G_SHARP,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      F_SHARP,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      B_PLAIN,        0,      1 },
        { 1.0,      0,              0,      1 },
        { 0.6,      E_PLAIN,        0,      1 }, // Sequence 3
        { 0.2,      0,              0,      1 },
        { 0.6,      F_SHARP,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      G_SHARP,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      E_PLAIN,        0,      1 },
        { 0 }
    };

    static const TONE_SEQUENCE third_quarter_hour [] = {
        { 0.6,      G_SHARP,        0,      1 }, // Sequence 4
        { 0.2,      0,              0,      1 },
        { 0.6,      E_PLAIN,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      F_SHARP,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      B_PLAIN,        0,      1 },
        { 1.0,      0,              0,      1 },
        { 0.6,      B_PLAIN,        0,      1 }, // Sequence 5
        { 0.2,      0,              0,      1 },
        { 0.6,      F_SHARP,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      G_SHARP,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      E_PLAIN,        0,      1 },
        { 1.0,      0,              0,      1 },
        { 0.6,      G_SHARP,        0,      1 }, // Sequence 1
        { 0.2,      0,              0,      1 },
        { 0.6,      F_SHARP,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      E_PLAIN,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      B_PLAIN,        0,      1 },
        { 0 }
    };

    static const TONE_SEQUENCE on_the_hour [] = {
        { 0.6,      E_PLAIN,        0,      1 }, // Sequence 2
        { 0.2,      0,              0,      1 },
        { 0.6,      G_SHARP,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      F_SHARP,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      B_PLAIN,        0,      1 },
        { 1.0,      0,              0,      1 },
        { 0.6,      E_PLAIN,        0,      1 }, // Sequence 3
        { 0.2,      0,              0,      1 },
        { 0.6,      F_SHARP,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      G_SHARP,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      E_PLAIN,        0,      1 },
        { 1.0,      0,              0,      1 },
        { 0.6,      G_SHARP,        0,      1 }, // Sequence 4
        { 0.2,      0,              0,      1 },
        { 0.6,      E_PLAIN,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      F_SHARP,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      B_PLAIN,        0,      1 },
        { 1.0,      0,              0,      1 },
        { 0.6,      B_PLAIN,        0,      1 }, // Sequence 5
        { 0.2,      0,              0,      1 },
        { 0.6,      F_SHARP,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      G_SHARP,        0,      1 },
        { 0.2,      0,              0,      1 },
        { 0.6,      E_PLAIN,        0,      1 },
        { 1.0,      0,              0,      1 },
        {0}
    };

    static const TONE_SEQUENCE icds [] = {
         // Values for this have been approximated
        { 2.5,      2225,   0, 1 },
        { 1.25,     1270,   0, 1 },
        {0}
    };

#ifdef DOLBY_TEST
    static const TONE_ALBUM dolby = {
        "Thomas Dolby",
        "The Golden Age Of Wireless",
        "Test Songs for Cross-Source Seeding",
        NULL
    };
#endif

    static const TEST_SONG tone_list[] = {
        {
            "sdialtone",
            "Dial Tone",
            &telephonys_best,
            dial_tone, 18
        },
        {
            "sringtone",
            "Line Ringing",
            &telephonys_best,
            ring_back, 4
        },
        {
            "sbusytone",
            "Line in Use",
            &telephonys_best,
            busy, 10
        },
        {
            "sfastbusy",
            "Fast Busy",
            &telephonys_best,
            fast_busy, 10
        },
        {
            "snocircuit",
            "No Circuit Found",
            &sit_tones,
            no_circuit, 1
        },
        {
            "svacantcircuit",
            "Vacant Circuit",
            &sit_tones,
            vacant_circuit, 1
        },
        {
            "sopintercept",
            "Operator Intercept",
            &sit_tones,
            op_intercept, 1
        },
        {
            "sreorder",
            "Reorder (All Circuits Busy)",
            &sit_tones,
            reorder, 1
        },
        {
            "shour",
            "Hourly Westminster chimes",
            &westminster,
            on_the_hour, 1
        },
        {
            "squarter",
            "Quarter-past Westminster chimes",
            &westminster,
            quarter_hour, 1
        },
        {
            "shalf",
            "Half-hour Westminster chimes",
            &westminster,
            half_hour, 1
        },
        {
            "sthirdquarter",
            "Quarter-till Westminster chimes",
            &westminster,
            third_quarter_hour, 1
        },
        {
            "sa440",
            "A note (440 Hz)",
            &common_tones,
            a440, 60
        },
        {
            "shz1000",
            "1KHz tone",
            &common_tones,
            hz1000, 60
        },
        {
            "slegacyebs",
            "Legacy Emergency Broadcast System Activation Tone",
            &common_tones,
            legacy_ebs, 17
        },
        {
            "sicds",
            "Intergalactic Computer Distress Signal",
            &common_tones,
            icds, 10
        },
#ifdef DOLBY_TEST
        {   "sbogus",
            "She Blinded Me With Science",
            &dolby,
            icds, 10
        },
#endif
        { nullptr }
    };

    SplitToneId::SplitToneId (const std::string &id) {
        std::string::size_type colon = id.find (':');
        if (colon != std::string::npos) {
            media_id = id.substr (0, colon);
            std::string channel_name = id.substr (colon + 1);
            if (strcasecmp (channel_name, "left") == 0)
                channel = Channel::LEFT;
            else if (strcasecmp (channel_name, "right") == 0)
                channel = Channel::RIGHT;
            else
                throw std::invalid_argument ("Bad channel specification in id");
        } else {
            media_id = id;
        }
    }


    static MusicLibrary::Allocator <Song, MusicLibrary::Album> song_allocate;

    Library::Library (Media::Source * const owner, const bool persistence)
    : MusicLibrary::Library (owner, persistence, MusicLibrary::Library::SongAllocator {song_allocate}) {
        bool existing = load();
        loadToneLibrary (existing);
    };

    static std::string ChannelName (const Channel channel) {
        switch (channel) {
            case Channel::CENTER:
                return "center";
            case Channel::LEFT:
                return "left";
            case Channel::RIGHT:
                return "right";
        }
        return "invalid_channel";
    }

    Song *Library::duplicate (const Song *song, const Channel channel) {
        assert (song);
        MusicLibrary::Album *alb = albums.getById (song->albumId());
        Song *newsong = static_cast <Song *> (songs.addOrGetItem (song->title() + " (" + ChannelName (channel) + ")",
                                              song->songId() + ":" + ChannelName (channel),
                                              alb));
        newsong->channel = channel;
        return newsong;
    }


    Song *Library::add (const TEST_SONG *item) {
        MusicLibrary::Artist *art = artists.addOrGetItem (item->album->artist, this);
        MusicLibrary::Album *alb = albums.addOrGetItem (item->album->album, art);
        Song *song = static_cast <Song *> (songs.addOrGetItem (item->title, std::string (item->id), alb));
        return song;
    }

    void Library::loadToneLibrary (bool restored) {
        bool channelized = static_cast <const Parameters *> (static_cast <Source * const > (source)->parameters)->discrete_channels;
        MusicAutoReleasePool pool;
        for (const TEST_SONG *test = tone_list; test->id; test++) {
            Song *song = add (test);
            if (channelized) {
                duplicate (song, Channel::LEFT);
                duplicate (song, Channel::RIGHT);
            }
            assert (restored || (((test - tone_list + 1) * (channelized ? 3 : 1)) == songs.size()));
        }
        if (!restored) {
            MusicLibrary::Playlist *play = playlists.addOrGetItem ("Soothing Sounds of Moog-like Clocks", this);
            play->selector = "artist = \"westminster*\"";
            play->enabled = false;
        }
        mixRecalculate();
    };

    const TEST_SONG *Song::media() const {
        try {
            const SplitToneId tone_id (songId());
            for (const TEST_SONG *song = tone_list; song->id; song++) {
                if (tone_id.media_id == song->id) {
                    const_cast <Song*> (this)->channel = tone_id.channel;
                    return song;
                }
            }
        } catch (const std::invalid_argument &e) {
            // Ignore it.
        }
        return nullptr;
    };
}
