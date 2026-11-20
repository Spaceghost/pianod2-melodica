///
/// Predicate handling.
/// Predicate parsing and interpretation.
/// @file       predicate.h - pianod2
/// @author     Perette Barella
/// @date       2015-10-12
/// @copyright  Copyright (c) 2015-2020 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include <memory>

#include "fundamentals.h"
#include "filter.h"


#define PREDKEY_MANNER "manner"
#define PREDKEY_TYPE "type"
#define PREDKEY_SEEDTYPE "seedtype"
#define PREDKEY_SOURCE "source"

#define PARSER_SIMPLE_LIST_PREDICATE "simpleListPredicate"
#define PARSER_LIST_PREDICATE "listPredicate"
#define PARSER_SEED_PREDICATE "seedPredicate"
#define PARSER_SINGLE_PREDICATE "singleItemPredicate"
#define PARSER_PLAYLIST_LIST "playlistPredicate"
#define PARSER_SINGLE_PLAYLIST "singlePlaylistPredicate"

#define KEY_PREDICATE "predicate"
#define KEY_PLAYLIST "playlist"

#define SIMPLE_LIST_PREDICATE " {" KEY_PREDICATE ":" PARSER_SIMPLE_LIST_PREDICATE "} ..."
#define LIST_PREDICATE " {" KEY_PREDICATE ":" PARSER_LIST_PREDICATE "} ..."
#define SEED_PREDICATE " {" KEY_PREDICATE ":" PARSER_SEED_PREDICATE "} ..."
#define SINGLE_PREDICATE " {" KEY_PREDICATE ":" PARSER_SINGLE_PREDICATE "}"
#define LIST_PLAYLIST " {" KEY_PLAYLIST ":" PARSER_PLAYLIST_LIST "} ..."
#define SINGLE_PLAYLIST " {" KEY_PLAYLIST ":" PARSER_SINGLE_PLAYLIST "}"

namespace Predicate {
    void construct_predicate_parsers (class PianodParser *parser);

    /// Check for presence of a predicate.
    inline bool havePredicate (const Parsnip::Data &options) {
        return options.contains (PREDKEY_MANNER);
    }

    std::unique_ptr <Filter> getPredicate (const PianodConnection &conn,
                                           const Parsnip::Data &predicate,
                                           Filter::Field field = Filter::Field::Search);

    // Generic Things
    ThingieList getSpecifiedThings (const PianodConnection &conn,
                                    const Parsnip::Data &predicate,
                                    SearchRange search_what = SearchRange::KNOWN);
    MusicThingie *getSpecifiedThing (const PianodConnection &conn,
                                     const Parsnip::Data &predicate);

    // Songs
    SongList getSpecifiedSongs (const PianodConnection &conn,
                                const Parsnip::Data &predicate,
                                SearchRange search_what = SearchRange::KNOWN);
    PianodSong *getSpecifiedSong (const PianodConnection &conn,
                                  const Parsnip::Data &predicate);

    // Playlists
    PlaylistList getSpecifiedPlaylists (const PianodConnection &conn,
                                        const Parsnip::Data &predicate);
    PianodPlaylist *getSpecifiedPlaylist (const PianodConnection &conn,
                                          const Parsnip::Data &predicate);

}


