///
/// Playlist "tuner".
/// Manages the playlists included in a mix.
/// @file       tuner.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-12-11
/// @copyright  Copyright (c) 2016-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cassert>
#include <cstdio>

#include <string>
#include <vector>
#include <algorithm>

#include "callback.h"
#include "callbackimpl.h"
#include "interpreter.h"
#include "logging.h"
#include "connection.h"
#include "response.h"
#include "user.h"
#include "users.h"
#include "mediaunit.h"
#include "mediamanager.h"
#include "predicate.h"
#include "retainedlist.h"

#include "tuner.h"

// clang-format off
/// Text to enumeration translations for playlist mix actions.
const LookupTable <CartsVerb> CartsWord {
    { "Clear",      CartsVerb::CLEAR },
    { "Add",        CartsVerb::ADD },
    { "Remove",     CartsVerb::REMOVE },
    { "Delete",     CartsVerb::REMOVE },
    { "Toggle",     CartsVerb::TOGGLE },
    { "Set",        CartsVerb::SET }
};

/// Text to enumeration translations for randomization methods.
const LookupTable <Media::SelectionMethod> SelectionMethodWords {
    { "album",      Media::SelectionMethod::Album },
    { "artist",     Media::SelectionMethod::Artist },
    { "song",       Media::SelectionMethod::Song },
    { "playlist",   Media::SelectionMethod::Playlist },
    { "random",     Media::SelectionMethod::Random }
};
// clang-format on

// Force template instantiation.
template class CallbackManager<Tuner::Tuner, Tuner::Tuner::Callbacks>;

namespace Tuner {
    enum class PlaylistDetailLevel {
        NAME,
        DETAIL
    };
    /// Text to enumeration values for detail level in playlist lists.
    const LookupTable <PlaylistDetailLevel> PlaylistDetailLevels {
        { "names",      PlaylistDetailLevel::NAME },
        { "details",    PlaylistDetailLevel::DETAIL },
        { "list",       PlaylistDetailLevel::DETAIL }
    };




    /** Create a new tuner and load known playlists.
        @param svc The service with which the tuner will be associated. */
    Tuner::Tuner (PianodService *svc) : service (svc) {
        updatePlaylists();
        autotune.login = true;
        autotune.flag = true;

        Media::Manager::Callbacks callbacks;
        callbacks.sourceReady = std::bind (&Tuner::sourcesChanged, this, std::placeholders::_1);
        callbacks.sourceOffline = std::bind (&Tuner::sourcesChanged, this, std::placeholders::_1);
        media_manager->callback.subscribe (this, callbacks);
    }

    /** Destroy the tuner and release resources. */
    Tuner::~Tuner() {
        media_manager->callback.unsubscribe (this);
    }

    /** Register a new source by adding its playlists to those known */
    void Tuner::sourcesChanged (const Media::Source * const ) {
        updatePlaylists();
        recalculatePlaylists();
    }

    /** Update the list of playlists known by the tuner. */
    void Tuner::updatePlaylists() {
        PlaylistList allPlaylists = media_manager->getPlaylists();
        MixMap new_mix;
        new_mix.reserve (allPlaylists.size());

        for (auto playlist : allPlaylists) {
            MixItem &slot = new_mix [playlist->id()];
            assert (!slot.playlist);
            slot.playlist = playlist;

            // Retain the enabled status from the old list, if possible.
            auto old_one = mix.find (playlist->id());
            assert (old_one == mix.end() || old_one->second.playlist);
            slot.enabled = (old_one == mix.end() ? playlist->includedInMix() : old_one->second.enabled);
        }

        swap (mix, new_mix);

        callback.notify (&Callbacks::playlistsChanged);
    }

    /** Remove a source from the playlist list in preparation for its removal. */
    void Tuner::purge (const Media::Source * const source) {
        auto it = mix.begin();
        while (it != mix.end()) {
            if (it->second.playlist->source() == source) {
                it->second.playlist->release();
                it = mix.erase (it);
            } else {
                it++;
            }
        }
    }

    /** Push the current playlist selections to the playlists/sources in
        preparation for getting random songs.
        @param source If null, pushes all playlists.  Otherwise, only
        pushes those belonging to the source. */
    void Tuner::pushPlaylistSelections (Media::Source * const source) {
        for (auto &item : mix) {
            if (!source || source == item.second.playlist->source()) {
                item.second.playlist->includedInMix (item.second.enabled);
            }
        }
    }

    /** Check if a playlist is included in the mix.
        @param playlist The playlist whose status to check.
        @return True if the playlist is in the mix, false otherwise. */
    bool Tuner::includedInMix (const PianodPlaylist *playlist) {
        MixMap::iterator it = mix.find (playlist->id());
        if (it != mix.end())
            return it->second.enabled;

        flog (LOG_WHERE (Log::WARNING|Log::TUNING), "Playlist ", playlist->playlistName(), " is not a known playlist.");
        return true;
    }

    /** Check for enabled playlists in the mix.
        @param source If not-null, consider only playlists from specified source.
        @return True if no matching enabled playlists are found, false otherwise. */
    bool Tuner::empty (const Media::Source * const source) const {
        if (automatic_mode && !anyone_listening)
            return true;
        for (const auto &item : mix) {
            if (item.second.enabled && (!source || source == item.second.playlist->source())) {
                return false;
            }
        }
        return true;
    }

    /** Set or clear automatic playlist selection.
        @param automatic_playlists True to enable, false otherwise. */
    void Tuner::automatic (bool automatic_playlists) {
        automatic_mode = automatic_playlists;
        recalculatePlaylists();
    }

    /** Get a list of users applicable to autotuning.
        @param settings The autotuning mode settings.
        @param service The football service to search for user logins.
        @return A list of users matching the autotune settings. */
    UserList Tuner::getApplicableUsers (const AutotuneSettings &settings, const PianodService *service) {
        return user_manager->getUsers ([service, settings] (const User *user) -> bool {
            if (!user->havePrivilege (Privilege::Influence))
                return false;
            if (settings.login && user->online (*service))
                return true;
            if (settings.flag && user->havePrivilege (Privilege::Present))
                return true;
            return false;
        });
    }

    /** Get a list of users to consider for autotuning with the current
        service and autotuning mode. */
    UserList Tuner::getAutotuneUsers() const {
        return getApplicableUsers (autotune, service);
    };

    /** A structure used for autotuning calculations. */
    struct PlaylistSummary {
        PianodPlaylist *playlist;  ///< The playlist
        bool vetoed = false;       ///< Set if a user hates this playlist
        float average_rating = ratingAsFloat (Rating::NEUTRAL);
        PlaylistSummary (PianodPlaylist *p) : playlist (p){};
        PlaylistSummary(){};
        bool operator< (const PlaylistSummary &other) const {
            return ((vetoed ? 0 : average_rating) < (other.vetoed ? 0 : other.average_rating));
        }
    };

    /** Choose a random playlist (for mixing via playlist).
        @param criteria Specifies which playlists to choose from:
        which source, and single, mix, everything.
        @return The single playlist chosen. */
    PianodPlaylist *Tuner::getRandomPlaylist (const PianodPlaylist *criteria) {
        assert (criteria->playlistType() != PianodPlaylist::SINGLE);
        bool everything = criteria->playlistType() == PianodPlaylist::EVERYTHING;
        bool manager = criteria->source() == media_manager;
        PlaylistList choices;
        choices.reserve (mix.size());
        for (const auto &playlist : mix) {
            if ((everything || playlist.second.enabled)
                && (manager || playlist.second.playlist->source() == criteria->source())) {
                choices.push_back (playlist.second.playlist);
            }
        }
        assert (!choices.empty());
        return (choices [random() % choices.size()]);
    }

    /** Acquire some random tracks utilizing the currently-selected randomization mode.
        @param from A playlist or a metaplaylist from which to choose music.
        @param users The users present, for biasing selections.
        @return Some random tracks for play. */
    SongList Tuner::getRandomTracks (PianodPlaylist *from, const UserList &users) {
        Media::SelectionMethod selection_method = random_selection_method;
        if (selection_method == Media::SelectionMethod::Random) {
            // Choose a concrete method.
            selection_method = static_cast<Media::SelectionMethod> (random() % (int) Media::SelectionMethod::Random);
        }

        // Update room mix selections in sources, if required.
        if (from->playlistType() == PianodPlaylist::MIX) {
            Media::Source * const source = (from->source() == media_manager ? nullptr : from->source());
            if (empty (source)) {
                // Nothing selected or current users don't agree on music, so be quiet.
                return SongList{};
            }

            pushPlaylistSelections (source);
        }

        // If we're mixing via playlist, choose a single playlist to pick from.
        if (selection_method == Media::SelectionMethod::Playlist) {
            if (from->playlistType() != PianodPlaylist::SINGLE) {
                from = getRandomPlaylist (from);
            }
            selection_method = Media::SelectionMethod::Song;
        }

        // Shuffle some stuff into the queue.
        SongList songs = from->getRandomSongs (users, selection_method);
        if (songs.empty()) {
            flog (LOG_WHERE (Log::WARNING), "Playlist ", from->playlistName(), " did not provide random tracks");
            service->announceToRoom (Response (F_PLAYER_EMPTY));
            throw std::runtime_error ("Could not retrieve random tracks");
        }
        return songs;
    }

    /** Recalculate the playlists based on current users and autotune settings.
        @return true if playlists changed, false otherwise. */
    bool Tuner::recalculatePlaylists() {
        if (mix.empty() || !automatic_mode) {
            flog (LOG_WHERE (Log::TUNING), "No playlists or autotuning disabled.");
            return false;
        }

        std::vector<PlaylistSummary> playlist_data;

        // Get the list of users that will influence calculations.
        UserList considered_users = getAutotuneUsers();
        anyone_listening = !considered_users.empty();
        if (!anyone_listening) {
            flog (LOG_WHERE (Log::TUNING), "No users to consider for autotuning.");
            return false;
        }

        flog (LOG_FUNCTION (Log::TUNING), "Computed station biases follow:");

        // For each source...
        for (const auto &source : media_manager->getReadySources()) {
            // Get user ratings for this source.
            std::vector<UserData::Ratings *> source_ratings;
            for (const auto user : considered_users) {
                UserData::Ratings *ratings
                        = UserData::Ratings::retrieve (user, UserData::Key::PlaylistRatings, source->key());
                if (ratings) {
                    source_ratings.push_back (ratings);
                }
            }
            // Calculate average ratings for each playlist from this source.
            PlaylistList playlists = source->getPlaylists();
            for (auto playlist : playlists) {
                std::string id{ playlist->playlistId() };
                PlaylistSummary item (playlist);
                RatingAverager average;
                for (auto user_ratings : source_ratings) {
                    auto rating = user_ratings->find (id);
                    if (rating != user_ratings->end()) {
                        if (ratingAsFloat (rating->second) <= autotune.veto_threshold)
                            item.vetoed = true;
                        average.add (rating->second);
                    }
                }
                item.average_rating = average (Rating::NEUTRAL);
                playlist_data.push_back (item);

                if (logging_enabled (Log::TUNING)) {
                    fprintf (stderr,
                             "%-40.40s rate %5f = %u/%u vetoed %s\n",
                             playlist->name().c_str(),
                             item.average_rating,
                             average.sum(),
                             average.items(),
                             item.vetoed ? "yes" : "no ");
                }
            }
        }

        // We've now rated every playlist from every source.  Sort the results ascending.
        sort (playlist_data.begin(), playlist_data.end());

        // Choose a quality goal: the best average rating, adjusted with some margin.
        float quantity_goal_min_quality = playlist_data.back().average_rating - autotune.quality_margin;
        bool changed = false;

        int included_count = 0;    // # of included playlists.
        int good_count = 0;        // # of playlists that meet inclusion threshold.
        int acceptable_count = 0;  // # of playlists included to meet variety target.
        int equal_count = 0;       // # of playlists included because rated same as prior inclusions.
        float equal_rating = 11;
        for (auto it = playlist_data.rbegin(); it != playlist_data.rend(); it++) {
            const auto &playlist = *it;
            const char *reason = nullptr;
            bool included = false;
            if (playlist.vetoed) {
                reason = "vetoed";
            } else if (playlist.average_rating < autotune.rejection_threshold) {
                reason = "average below rejection threshold";
            } else if (playlist.average_rating >= autotune.inclusion_threshold) {
                reason = "average above inclusion threshold";
                included = true;
                good_count++;
            } else if (included_count < autotune.quantity_goal
                       && playlist.average_rating >= quantity_goal_min_quality) {
                reason = "trying to meet quantity goal";
                included = true;
                acceptable_count++;
            } else if (playlist.average_rating >= equal_rating) {
                reason = "average equal to others included";
                included = true;
                equal_count++;
            } else {
                reason = (included_count < autotune.quantity_goal ? "average below quality margin"
                                                                  : "quantity goal already met.");
            }
            flog (LOG_WHERE (Log::TUNING),
                  playlist.playlist->name(),
                  included ? " is included: " : " is excluded: ",
                  reason);
            if (included) {
                included_count++;
                equal_rating = playlist.average_rating;
            }
            MixMap::iterator mixit = mix.find (playlist.playlist->id());
            assert (mixit != mix.end());
            if (mixit != mix.end() && mixit->second.enabled != included) {
                changed = true;
                mixit->second.enabled = included;
            }
        }

        if (changed) {
            int message_id = ((good_count ? 4 : 0) | (acceptable_count ? 2 : 0) | (equal_count ? 1 : 0));
            assert (message_id >= 0 && message_id <= 7);
            static const char *const messages [8]
                    = { /* 0 */ "current listener playlists preferences are incompatible",
                        /* 1 */ nullptr,
                        /* 2 */ "autotuner picked the acceptable playlists",
                        /* 3 */ "autotuner picked acceptable playlists with variety",
                        /* 4 */ "autotuner picked only good playlists",
                        /* 5 */ nullptr,
                        /* 6 */ "autotuner picked good and acceptable playlists",
                        /* 7 */ "autotuner picked good and acceptable playlists, with variety" };
            const char *message = messages [message_id];
            assert (message);
            ResponseGroup announcements;
            announcements (V_MIX_CHANGED);
            announcements (V_SERVER_STATUS, message);
            service->announceToRoom (announcements);
            callback.notify (&Callbacks::mixChanged, true, message);
        }
        return changed;
    }

    // clang-format off
    enum class AutotuneConsiders {
        LOGIN,
        FLAG,
        ALL
    };

    static const LookupTable <AutotuneConsiders> AutotuneConsiderLookup ( {
        { "login",      AutotuneConsiders::LOGIN },
        { "flag",       AutotuneConsiders::FLAG },
        { "all",        AutotuneConsiders::ALL }
    });
    // clang-format on

#define KEY_CONSIDER "consider"
#define KEY_VETO "vetoRating"
#define KEY_REJECT "rejectionRating"
#define KEY_INCLUDE "inclusionRating"
#define KEY_QUANTITY_GOAL "quantityGoal"
#define KEY_QUALITY_MARGIN "qualityMargin"

    const Parsnip::OptionParser::Definitions &autotuning_option_parser_definitions() {
        /** Parse definitions for the parameter parser */
        // clang-format off
        static const Parsnip::OptionParser::Definitions mode_statements = {
            "<" KEY_CONSIDER ":login|flag|all>",
            "consider <" KEY_CONSIDER ":login|flag|all>",
            "veto {" KEY_VETO "}",
            "reject {" KEY_REJECT "}",
            "include {" KEY_INCLUDE "}",
            "quantity goal {#" KEY_QUANTITY_GOAL ":0-999}",
            "quality margin {#" KEY_QUALITY_MARGIN ":0.0-5.0}",
        };
        // clang-format on
        return mode_statements;
    }

    /** Interpret options into the AutotuneSettings structure. */
    void interpret_autotune_options (const Parsnip::Data &options, AutotuneSettings *dest) {
        if (options.contains (KEY_CONSIDER)) {
            switch (AutotuneConsiderLookup [options [KEY_CONSIDER].asString()]) {
                case AutotuneConsiders::LOGIN:
                    dest->login = true;
                    dest->flag = false;
                    break;
                case AutotuneConsiders::FLAG:
                    dest->login = false;
                    dest->flag = true;
                    break;
                case AutotuneConsiders::ALL:
                    dest->login = true;
                    dest->flag = true;
                    break;
            }
        }
        if (options.contains (KEY_VETO)) {
            dest->veto_threshold = RATINGS.getPrecise (options [KEY_VETO].asString());
        }
        if (options.contains (KEY_REJECT)) {
            dest->rejection_threshold = RATINGS.getPrecise (options [KEY_REJECT].asString());
        }
        if (options.contains (KEY_INCLUDE)) {
            dest->inclusion_threshold = RATINGS.getPrecise (options [KEY_INCLUDE].asString());
        }
        if (options.contains (KEY_QUANTITY_GOAL)) {
            dest->quantity_goal = options [KEY_QUANTITY_GOAL].asInteger();
        }
        if (options.contains (KEY_QUALITY_MARGIN)) {
            dest->quality_margin = options [KEY_QUALITY_MARGIN].asDouble();
        }
    }

#define KEY_RANDOMIZE_METHOD "randomizeBy"
#define KEY_ADJUST_ACTION "action"
#define KEY_DETAIL "detail"
#define DETAIL_OPTIONS "[" KEY_DETAIL ":names|details|list]"

    const Parsnip::Parser::Definitions &parser_definitions() {
        // clang-format off
        static const Parsnip::Parser::Definitions tuning_statements = {
            { PLAYLISTLIST,     "playlist " DETAIL_OPTIONS }, // List the playlists
            { PLAYLISTLIST,     "playlist " DETAIL_OPTIONS LIST_PLAYLIST },	// List specific playlists
            // Mix-related commands
            { MIXINCLUDED,      "mix" },                            // Unofficial
            { MIXINCLUDED,      "mix " DETAIL_OPTIONS " included" },            // Short forms are not official protocol
            { MIXEXCLUDED,      "mix " DETAIL_OPTIONS " excluded" },            // Show songs not included in mix
            { MIXADJUST,        "mix <" KEY_ADJUST_ACTION ":add|remove|set|toggle>" LIST_PLAYLIST  }, // Change mix composition
            { AUTOTUNEGETMODE,  "autotune mode" },                  // Query autotune users mode
            { AUTOTUNESETMODE,  "autotune mode {" KEY_OPTIONS ":" KEY_TUNEROPTIONS "} ..." }, // Change the autotune users mode
            { SELECTIONMETHOD,  "queue randomize by <" KEY_RANDOMIZE_METHOD ":song|artist|album|playlist|random>" }
        };
        // clang-format on
        return tuning_statements;
    }

    const Parsnip::Parser::Definitions &Tuner::getParserDefinitions() {
        return parser_definitions();
    }

    /** Retrieve names for our JSON requests.
    @return Request name to command ID mappings. */
    const PianodSchema::CommandIds &json_request_names() {
        static const PianodSchema::CommandIds mappings{
            { "getPlaylists", PLAYLISTLIST },         { "getMix", MIXINCLUDED },
            { "getMixExclusions", MIXEXCLUDED },      { "adjustMix", MIXADJUST },
            { "getAutotuneMode", AUTOTUNEGETMODE },   { "setAutotuneMode", AUTOTUNESETMODE },
            { "setRandomizeMethod", SELECTIONMETHOD }
        };
        return mappings;
    };

    /** Send a list of playlists.
        @param list The list to send.
        @param long_format If true, send playlist details.  If false, send names only. */
    static ResponseCollector sendPlaylistList (PlaylistList &list, PlaylistDetailLevel format) {
        DataResponse response;
        if (format == PlaylistDetailLevel::DETAIL) {
            response.data (list);
        } else {
            for (auto item : list) {
                response.data (Response (I_PLAYLIST, item->playlistName()));
            }
        }
        return std::move (response);
    }

    bool Tuner::authorizedCommand (Parsnip::Parser::CommandId command, PianodConnection &conn) {
        switch (command) {
            case MIXINCLUDED:
            case MIXEXCLUDED:
            case PLAYLISTLIST:
            case AUTOTUNEGETMODE:
                return conn.haveRank (Rank::Listener);
            case AUTOTUNESETMODE:
                return conn.haveRank (Rank::Administrator);
            default:
                return conn.haveRank (Rank::Standard);
        }
    }

    ResponseCollector Tuner::handleCommand (Parsnip::Parser::CommandId command,
                                            const Parsnip::Data &options,
                                            PianodConnection &conn) {
        assert (conn.source());

        switch (command) {
            case PLAYLISTLIST: {
                PlaylistList playlists{
                    Predicate::getSpecifiedPlaylists (conn, options.getOr (KEY_PLAYLIST, EmptyDictionary))
                };
                PlaylistDetailLevel format = optionalValue (options, "detail", PlaylistDetailLevels, PlaylistDetailLevel::NAME);
                return sendPlaylistList (playlists, format);
            }
            case MIXINCLUDED:
            case MIXEXCLUDED: {
                PlaylistList list = conn.source()->getPlaylists();
                PlaylistList matches;
                for (auto item : list) {
                    if ((command == MIXINCLUDED) == (mix [item->id()].enabled)) {
                        matches.push_back (item);
                    }
                }
                PlaylistDetailLevel format = optionalValue (options, "detail", PlaylistDetailLevels, PlaylistDetailLevel::NAME);
                return sendPlaylistList (matches, format);
            }
            case MIXADJUST: {
                CartsVerb action = CartsWord [options [KEY_ADJUST_ACTION].asString()];
                PlaylistList update = Predicate::getSpecifiedPlaylists (conn, options [KEY_PLAYLIST]);
                bool change = false;
                if (action == CartsVerb::SET) {
                    for (auto &item : mix) {
                        item.second.enabled = false;
                    }
                    change = true;
                }
                for (auto item : update) {
                    assert (mix.find (item->id()) != mix.end());
                    bool before = mix [item->id()].enabled;
                    bool after = (action == CartsVerb::ADD || action == CartsVerb::SET ? true
                                  : action == CartsVerb::REMOVE                        ? false
                                                                                       : !before);
                    if (before != after) {
                        mix [item->id()].enabled = after;
                        change = true;
                    }
                }
                CommandReply response;
                response.succeed (Response (S_MATCH, update.size()));
                if (change) {
                    response.room_events (V_MIX_CHANGED);
                    response.room_events (A_CHANGED_MIX);
                    callback.notify (&Callbacks::mixChanged, true, "user action");
                }
                return std::move (response);
            }
            case AUTOTUNEGETMODE: {
                Response::List mode;
                if (autotune.login)
                    mode.push_back ("login");
                if (autotune.flag)
                    mode.push_back ("flag");
                assert (!mode.empty());
                return Response (
                        I_AUTOTUNE_MODE,
                        std::move (mode),
                        Parsnip::Data::make_dictionary ({ { "login", autotune.login }, { "flag", autotune.flag } }));
            }
            case AUTOTUNESETMODE: {
                AutotuneSettings new_settings = autotune;
                interpret_autotune_options (options [KEY_OPTIONS], &new_settings);
                autotune = new_settings;
                recalculatePlaylists();
                return S_OK;
            }
            case SELECTIONMETHOD: {
                random_selection_method = SelectionMethodWords [options [KEY_RANDOMIZE_METHOD].asString()];
                CommandReply response (S_OK);
                response.room_events (Response (V_SELECTIONMETHOD, SelectionMethodWords [random_selection_method]));
                response.room_events (Response (A_ADJUSTAUDIO, "randomization method"));
                return std::move (response);
            }
        }
        throw CommandError (E_NOT_IMPLEMENTED, std::to_string (command));
    }

    /** Send current state information. */
    Response Tuner::assembleStatus() const {
        return Response (V_SELECTIONMETHOD, SelectionMethodWords [random_selection_method]);
    }
}  // namespace Tuner
