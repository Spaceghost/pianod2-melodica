///
/// Manager for sources, which it itself a source that aggregates
/// the other sources.
/// @file       mediamanager.h - pianod project
/// @author     Perette Barella
/// @date       2014-10-23
/// @copyright  Copyright 2012–2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <cstdio>
#include <ctime>

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <functional>

#include "fundamentals.h"
#include "callback.h"
#include "logging.h"
#include "musictypes.h"
#include "filter.h"
#include "ownership.h"
#include "mediaunit.h"
#include "interpreter.h"

class PianodConnection;
namespace Tuner {
    class Tuner;
}

namespace Media {
    /** The Media::Manager contains a collection of all sources.  It manages
     their ownership and provides them to the engine when asked.  It also
     implements the Source interface, in which case it amalgamates
     data from all the sources it knows into one dataset. */
    class Manager : public Source, public PianodInterpreter, private std::unordered_map<Source::SerialNumber, Source * const > {
        friend class Tuner::Tuner;
    public:
        using base_container = std::unordered_map<Source::SerialNumber, Source * const >;
        using base_container::begin;
        using base_container::end;

        /// Notifications/Delegates provided by the media manager.
        class Callbacks {
        public:
            /// Notification sent when a source is ready (goes online).
            std::function<void (const Source * const)> sourceReady;
            /// Notification sent when a source goes offline or is about to be removed.
            std::function<void (const Source * const)> sourceOffline;
            /// Delegate that can veto source removal.
            std::function<bool (const Source * const)> canRemoveSource;
            /// Final notification that a source is being removed.
            std::function<void (const Source * const)> sourceRemoved;
            /// Subscription for alert messages from all sources.
            std::function<void (RESPONSE_CODE, const char *)> statusNotification = Source::defaultStatusHandler;
        };

        /// Callback manager provides API for registering and removing callbacks. 
        CallbackManager<Manager, Callbacks> callback;
        
    private:
        using SourceList = std::vector<Source *>;

        Retainer <PianodPlaylist *> mix_playlist;
        Retainer <PianodPlaylist *> everything_playlist;
        Retainer <PianodPlaylist *>transient_playlist;

        /// Criteria for selecting items into the transient playlist.
        Filter transient_criteria;

        void handleSourceStateChange (Source * const source);
        void redirectingStatusHandler (RESPONSE_CODE status, const char *detail);

        SourceList getRealSources () const;
        SourceList getRealSources (Source::State state) const;
        bool areSourcesInState (Source::State state) const;

        // Interpreter support
    public:
        static const Parsnip::Parser::Definitions &parser_definitions();
        static const Parsnip::OptionParser::Definitions &source_id_parser_definitions ();
        virtual const Parsnip::Parser::Definitions &getParserDefinitions () override;
        static const PianodSchema::CommandIds &json_request_names (PianodSchema &schema);
    private:
        virtual bool authorizedCommand (Parsnip::Parser::CommandId command, PianodConnection &conn) override;
        virtual ResponseCollector handleCommand (Parsnip::Parser::CommandId command, const Parsnip::Data &options, PianodConnection &conn) override;
        
        ResponseGroup reportStatistics (Media::Source * const source);
        bool add (Source * const src);

    public:
        Manager (void);
        ~Manager (void);

        // Manager routines
        Source * const get (const SerialNumber serial) const;
        Source * const get (const std::string &type, const std::string &ident) const;
        Source * const add (SourcePtr &&src);
        Media::Source * const getSource (const Parsnip::Data &options);
        ResponseCollector add (SourcePtr &&src, PianodConnection &conn);
        bool erase (Source * const source);
        void resetLockout ();

        inline bool areSourcesReady () const {
            return areSourcesInState (State::READY);
        }
        inline bool areSourcesPending () const {
            return (areSourcesInState (State::INITIALIZING) ||
                    areSourcesInState (State::VALID));
        }
        inline SourceList getReadySources () const {
            return getRealSources (State::READY);
        }

        // Capabilities routines
        virtual bool canExpandToAllSongs (void) const override;

        // Source API routines
        virtual bool flush (void) override;
        virtual float periodic (void) override;
        virtual const char *kind (void) const override;

        virtual PlaylistList getPlaylists (const Filter &filter = Filter::All) override;
        virtual MusicThingie *getAnythingById (const SplitId &id) override;
        MusicThingie *getAnythingById (const std::string &id);
        virtual PianodPlaylist *getMixPlaylist (void) override;
        virtual PianodPlaylist *getEverythingPlaylist (void) override;
        virtual PianodPlaylist *getTransientPlaylist (const Filter &criteria) override;


        virtual Player *getPlayer (const AudioSettings &audio, PianodSong *song) override;
        virtual SongList getRandomSongs (PianodPlaylist *playlist, const UserList &users,
                                         SelectionMethod selectionMethod) override;
        virtual ThingieList getSuggestions (const Filter &filter, SearchRange where) override;

        virtual PianodPlaylist *createPlaylist (const char *name,
                                              MusicThingie::Type type,
                                              MusicThingie *from) override;
        virtual PianodPlaylist *createPlaylist (const char *name, const Filter &filter) override;

        // Typecasts
        virtual MusicThingie *getSuggestion (MusicThingie *thing,
                                             MusicThingie::Type type,
                                             SearchRange where) override;
    };
}

extern Media::Manager *media_manager;

