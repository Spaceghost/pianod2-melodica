///
/// Playlist "tuner".
/// Manages the playlists included in a mix.
/// - Automatic playlist selection (autotuning) is performed here.
/// - Per-room playlist inclusions are held here, and set in sources only when
/// necessary before refilling the queue.
/// @file       tuner.h - pianod2
/// @author     Perette Barella
/// @date       2015-12-11
/// @copyright  Copyright (c) 2016-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <unordered_map>
#include <functional>

#include "callback.h"
#include "fundamentals.h"
#include "interpreter.h"
#include "musictypes.h"
#include "retainer.h"


/** Verb possibilities for inclusion/exclusion (CARTS) actions. */
enum class CartsVerb {
    CLEAR,
    ADD,
    REMOVE,
    TOGGLE,
    SET
};

/** Lookup table for inclusion/exclusion (CARTS) actions. */
extern const LookupTable <CartsVerb> CartsWord;


/// Store and maintain mixes and perform autotuning.
namespace Tuner {

    enum TUNERCOMMAND {
        MIXINCLUDED = CMD_RANGE_TUNING, ///< List playlists included in the mix.
        MIXEXCLUDED,     ///< List playlists excluded from the mix.
        MIXADJUST,       ///< Set a specific mix of playlists.
        PLAYLISTLIST,    ///< List playlists matching a predicate.
        AUTOTUNESETMODE, ///< Set autotuning mode and configure parameters.
        AUTOTUNEGETMODE, ///< Retrieve autotuning mode and parameters.
        SELECTIONMETHOD, ///< Set manner in which queue is populated.
    };

    /** Autotuning mode and parameters, defining which users to consider and
        how to decide whether a playlist is included or excluded from the mix. */
    struct AutotuneSettings {
        bool login = false; ///< If true, consider users in room when autotuning.
        bool flag = false; ///< If true, consider users' presence attribute when autotuning.
        float veto_threshold = ratingAsFloat (Rating::POOR); ///< Below this, an individual rating omits playlist.
        float rejection_threshold = ratingAsFloat (Rating::NEUTRAL); ///< Below this, an average rating omits playlist.
        float inclusion_threshold = ratingAsFloat (Rating::GOOD); ///< At or above this, an average rating includes playlist.
        int quantity_goal = 999; ///< Number of playlists to include.
        float quality_margin = 10; ///< How close average rating must be to best to include other playlists.
    };

    const Parsnip::Parser::Definitions &parser_definitions ();
    const PianodSchema::CommandIds &json_request_names ();
    const Parsnip::OptionParser::Definitions &autotuning_option_parser_definitions ();

    /** Tuner maintains a set of playlists and their inclusion or exclusion from
        the current mix, and can push it to the sources on demand (before getting
        random songs); this allows multiple mixes to exist.
     
        The tuner also performs autotuning (automatic playlist selection). */
    class Tuner : public PianodInterpreter {
        PianodService *service;
        struct MixItem {
            Retainer <PianodPlaylist *>playlist;
            bool enabled = false;
        };
        using MixMap = std::unordered_map<std::string, MixItem>;
        MixMap mix;
        bool automatic_mode = false;
        bool anyone_listening = false;
        Media::SelectionMethod random_selection_method = Media::SelectionMethod::Song;

        AutotuneSettings autotune;

    public:
        struct Callbacks {
            /// Callback sent when list of playlists has changed.
            std::function<void ()> playlistsChanged;
            /// Callback sent when mix has changed.
            std::function<void (bool automatically, const char *explanation)> mixChanged;
        };
        CallbackManager<Tuner, Callbacks> callback;

        virtual const Parsnip::Parser::Definitions &getParserDefinitions () override;
    private:
        virtual bool authorizedCommand (Parsnip::Parser::CommandId command, PianodConnection &conn) override;
        virtual ResponseCollector handleCommand (Parsnip::Parser::CommandId command, const Parsnip::Data &options, PianodConnection &conn) override;

        void sourcesChanged (const Media::Source * const );
    protected:
        PianodPlaylist *getRandomPlaylist (const PianodPlaylist *criteria);
    public:
        Tuner (PianodService *service);
        ~Tuner();
        void updatePlaylists ();
        bool includedInMix (const PianodPlaylist *playlist);
        SongList getRandomTracks (PianodPlaylist *from, const UserList &users);
        void purge (const Media::Source * const source);
        void pushPlaylistSelections (Media::Source * const source = nullptr);
        bool recalculatePlaylists ();
        bool empty(const Media::Source * const source) const;
        bool automatic () const { return automatic_mode; };
        void automatic (bool automatic_playlists);
        static UserList getApplicableUsers (const AutotuneSettings &settings,
                                            const PianodService *service);
        UserList getAutotuneUsers () const;
        Response assembleStatus () const;
    };
}

