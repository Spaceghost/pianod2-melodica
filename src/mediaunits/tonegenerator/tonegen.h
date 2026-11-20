///
/// Tone generator audio source.
/// Provides test tones for audio testing, and a testbed for
/// MediaUnit design.
/// @file       tonegen.h - pianod
/// @author     Perette Barella
/// @date       2014-12-04
/// @copyright  Copyright (c) 2014-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <cstdio>
#include <ctime>

#include <map>
#include <memory>

#include "mediaunit.h"
#include "mediaplayer.h"
#include "audio/audiooutput.h"
#include "fundamentals.h"
#include "musiclibrary.h"
#include "musiclibraryparameters.h"

namespace Media {
    class Output;
}


/** Classes for playing test tones.
    Available tones are organized like albums/tracks, so
    this also functions as a test mechanism for requests, etc.,
    without having to use a network-based audio service. */
namespace ToneGenerator {
    typedef struct tone_sequence_t {
        float duration;
        float firstFreq;
        float secondFreq;
        float relativeVolume;
    } TONE_SEQUENCE;
    typedef struct tone_albumt_t {
        const char *artist;
        const char *album;
        const char *playlist;
        const char *coverart;
    } TONE_ALBUM;
    typedef struct tone_song_t {
        const char *id;
        const char *title;
        const TONE_ALBUM *album;
        const TONE_SEQUENCE *tones;
        int toneRepetitions;
    } TEST_SONG;

    enum class Channel {
        LEFT,
        CENTER,
        RIGHT
    };

    class Source;
    class Player;
    class Library;
    class Song : public MusicLibrary::Song {
        friend Source;
        friend Player;
        friend Library;
    private:
        const TEST_SONG *media() const;
        Channel channel = Channel::CENTER;
    public:
        using MusicLibrary::Song::Song;
    };

    class Player : public Media::ThreadedPlayer {
        using channel_sample = int16_t;
        typedef struct audio_sample {
            channel_sample left;
            channel_sample right;
        } AUDIO_SAMPLE;
    private:
        Audio::Output *output = nullptr;
    public:
        const AudioSettings audio;
        const Song *song;
        const TEST_SONG *media;
        volatile int repetitions = 0;
        volatile int current_rep = 0;
        int seconds_into_rep = 0;
        std::unique_ptr<AUDIO_SAMPLE> tone;
        float duration = 0;
        long samples = 0;

    public:
        Player (const AudioSettings &audio, const PianodSong *song);
        void createTone (int volume);

        // Regulate playback 
        virtual void setVolume (float volume) override;

        // Data retrieval functions 
        State currentState (void) const override;
        virtual float trackDuration (void) const override;
        virtual float playPoint (void) const override;
        virtual RESPONSE_CODE playerThread (void) override;
    };

    struct SplitToneId {
        std::string media_id;
        Channel channel = Channel::CENTER;
        SplitToneId (const std::string &id);
    };

    class Library : public MusicLibrary::Library {
        friend Source;
    private:

    public:
        Library (Media::Source * const owner, const bool persistence);
        void loadToneLibrary (bool restored);
        Song *add (const TEST_SONG *item);
        Song *duplicate (const Song *song, const Channel channel);
    };

    /// Source parameters for filesystem source.
    struct Parameters : public MusicLibrary::LibraryParameters {
        bool discrete_channels = false;
        using MusicLibrary::LibraryParameters::LibraryParameters;
    };


    class Source : public Media::Source {
        friend class Playlist;
        friend class Library;
    private: 
        Library library;
        PianodPlaylist *mix_playlist = nullptr;
        PianodPlaylist *everything_playlist = nullptr;
    public:
        Source (const Parameters &params);
        ~Source (void);

        // Identity
        virtual const char *kind (void) const override;

        // Capability check methods
        virtual bool canExpandToAllSongs (void) const override;

        // General source stuff
        virtual bool flush (void) override;
        virtual float periodic (void) override;

        // Playlist methods
        virtual PlaylistList getPlaylists (const Filter &filter) override;
        virtual MusicThingie *getAnythingById (const Media::SplitId &id) override;
        virtual PianodPlaylist *getMixPlaylist (void) override;
        virtual PianodPlaylist *getEverythingPlaylist (void) override;

        // Creating playlists
        virtual PianodPlaylist *createPlaylist (const char *name,
                                              MusicThingie::Type type,
                                              MusicThingie *from) override;
        virtual PianodPlaylist *createPlaylist (const char *name, const Filter &filter) override;
        virtual PianodPlaylist *getTransientPlaylist (const Filter &criteria) override;

        // Miscellaneous
        virtual Media::Player *getPlayer (const AudioSettings &audio, PianodSong *song) override;
        virtual SongList getRandomSongs (PianodPlaylist *playlist,
                                         const UserList &users,
                                         Media::SelectionMethod) override;
        virtual ThingieList getSuggestions (const Filter &filter, SearchRange where) override;

        // Typecasts
        virtual MusicThingie *getSuggestion (MusicThingie *thing,
                                             MusicThingie::Type type,
                                             SearchRange where) override;
    };
}

