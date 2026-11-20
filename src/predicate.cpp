///
/// Predicate handling.
/// Predicate parsing and interpretation.
/// @file       predicate.cpp - pianod2
/// @author     Perette Barella
/// @date       2015-10-12
/// @copyright  Copyright (c) 2015-2021 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cstdlib>

#include <memory>
#include <unordered_set>
#include <exception>
#include <algorithm>

#include <football/football.h>
#include <parsnip/parsnip.h>
#include <parsnip/parsnip_command.h>

#include "fundamentals.h"
#include "lookup.h"
#include "utility.h"
#include "interpreter.h"
#include "predicate.h"
#include "connection.h"
#include "retainedlist.h"
#include "filter.h"
#include "querylist.h"
#include "mediaunit.h"
#include "mediamanager.h"

/// Functions and types for parsing and interpreting predicates.
namespace Predicate {
    /// Manners in which predicates may be specified.
    enum class MANNER { NAME, ID, WHERE, LIKE, SOURCE };

    /// Lookup table for manner names to enumeration.
    const LookupTable<MANNER> PredicateManners (
            { { "name", MANNER::NAME }, { "id", MANNER::ID }, { "where", MANNER::WHERE }, { "like", MANNER::LIKE } });

    /// Lookup table for optional type to filter field.
    const LookupTable<Filter::Field> SearchFields ({ { "any", Filter::Field::Search },
                                                     { "artist", Filter::Field::Artist },
                                                     { "album", Filter::Field::Album },
                                                     { "song", Filter::Field::Title },
                                                     { "playlist", Filter::Field::Playlist },
                                                     { "genre", Filter::Field::Genre } });

    struct Predicate {
        std::unique_ptr<Filter> filter;
        Media::Source *source = nullptr;
    };

#define PREDKEY_EXPRESSION "expression"
#define PREDKEY_ITEMLIST "items"
#define PREDKEY_FULFILLMENT "fulfill"
#define PREDKEY_SOURCENAME "sourcename"
#define PREDKEY_SEEDTYPE "seedtype"

#define SEEDPREP " <to|from|for>"

    void construct_predicate_parsers (PianodParser *parser) {
        // Single playlist-only predicate
        parser->addOptionParser (
                PARSER_SINGLE_PLAYLIST,
                Parsnip::OptionParser::Definitions{ "<" PREDKEY_MANNER ":id|name|like> {" PREDKEY_ITEMLIST "}" });

        // Multiple playlist-only predicate with source options
        parser->addOptionParser (
                PARSER_PLAYLIST_LIST,
                Parsnip::OptionParser::Definitions{ "<" PREDKEY_MANNER ":id|name|like> {" PREDKEY_ITEMLIST "} ...",
                                                    "<" PREDKEY_MANNER ":where> {" PREDKEY_EXPRESSION "...}",
                                                    "source {" PREDKEY_SOURCE ":" PARSER_SOURCEIDENTITY "}" });

        // Single-item general-purpose predicate: optional type and limited selection criteria
        parser->addOptionParser (PARSER_SINGLE_PREDICATE,
                                 Parsnip::OptionParser::Definitions{
                                         "[" PREDKEY_TYPE ":any|artist|album|song|playlist|genre] <" PREDKEY_MANNER
                                         ":id|name|like> {" PREDKEY_ITEMLIST "}" });

        // Simple general-purpose predicate parser: optional type and full selection criteria
        static Parsnip::OptionParser::Definitions simple_list_predicate
                = { "<" PREDKEY_TYPE ":any|artist|album|song|playlist|genre>",
                    "<" PREDKEY_MANNER ":id|name|like> {" PREDKEY_ITEMLIST "} ...",
                    "<" PREDKEY_MANNER ":where> {" PREDKEY_EXPRESSION "...}" };
        parser->addOptionParser (PARSER_SIMPLE_LIST_PREDICATE, simple_list_predicate);

        // Extended general-purpose predicate parser: above, plus source and fulfillment options
        static const Parsnip::OptionParser::Definitions extended_list_predicate
                = { "<" PREDKEY_FULFILLMENT ":authoritative|discretionary>",
                    "source {" PREDKEY_SOURCE ":" PARSER_SOURCEIDENTITY "}" };
        Parsnip::OptionParser::Definitions option_parser{ simple_list_predicate };
        std::copy (extended_list_predicate.begin(), extended_list_predicate.end(), std::back_inserter (option_parser));
        parser->addOptionParser (PARSER_LIST_PREDICATE, option_parser);

        // Seeding predicate parser:  Predicate parser with seeding options
        // Accepts _only one seed_ but multiple playlists
        option_parser = Parsnip::OptionParser::Definitions{ "<" PREDKEY_TYPE ":any|artist|album|song|playlist|genre>",
                                                            "<" PREDKEY_MANNER ":id|name|like> {" PREDKEY_ITEMLIST "}",
                                                            "type <" PREDKEY_SEEDTYPE ":artist|album|song|playlist>",
                                                            "<to|from|for> playlist " LIST_PLAYLIST };
        std::copy (extended_list_predicate.begin(), extended_list_predicate.end(), std::back_inserter (option_parser));
        parser->addOptionParser (PARSER_SEED_PREDICATE, option_parser);
    }

    /** Parse a command's predicate and return a matching filter.
        Predicate forms are:
        - `ID {id} ...` (exact match on 1 or more IDs)
        - `NAME {name} ...` (exact match on 1 or more names)
        - `[type] LIKE {text} ...` (permuted on `text`)
        If `type` was not specified, each word matches any text field;
        otherwise, the words must match on the specified field.
        - `WHERE {expression} ...` (logical filter expression)
        - `SOURCE TYPE {type} NAME {name} {manner} {expression} ...`
          Executes one of the other manners of predicates against a specific source.

        This function requires the command be defined with the following named fields:
        - `manner` - to match the predicate keyword
        - `expression` - the start of the predicate parameter
        - `type` (optional) - the optional type for LIKE.
        Tag names vary slightly for playlist predicates.
        (It is a syntax error if other predicates have this.)
        @param conn The connection for which the predicate is evaluated.
        @param predicate Details from the parsed predicate.
        @param field The field to use for name searches and the 'like' default. */
    static Predicate getFullPredicate (const PianodConnection &conn,
                                       const Parsnip::Data &predicate,
                                       Filter::Field field) {
        Predicate pred;
        MANNER manner = PredicateManners [predicate [PREDKEY_MANNER].asString()];
        pred.source = conn.source();

        if (manner == MANNER::ID) {
            // Disregard specified source if looking up by ID.
            pred.source = media_manager;
        } else if (predicate.contains (KEY_SOURCE)) {
            pred.source = media_manager->getSource (predicate [KEY_SOURCE]);
        }
        if (!pred.source) {
            throw CommandError (E_NOTFOUND, "Predicate source");
        }

        std::string data_type;
        if (predicate.contains (PREDKEY_TYPE)) {
            data_type = predicate [PREDKEY_TYPE].asString();
        }
        if (!data_type.empty() && manner == MANNER::ID)
            throw CommandError (E_TYPE_DISALLOWED);

        switch (manner) {
            case MANNER::WHERE:
                pred.filter.reset (new Filter (predicate [PREDKEY_EXPRESSION].asString()));
                break;
            case MANNER::ID:
                // Ideally, we should never get here.  But in case ID lookup
                // isn't implemented, or user enters a bizarre request involving
                // a specific predicate source and IDs, fall back on a filter.
                pred.filter.reset (
                        new ListFilter (predicate [PREDKEY_ITEMLIST].toList<std::string>(), Filter::Field::Id));
                break;
            case MANNER::NAME:
                pred.filter.reset (
                        new ListFilter (predicate [PREDKEY_ITEMLIST].toList<std::string>(), Filter::Field::Name));
                break;
            case MANNER::LIKE: {
                if (!data_type.empty())
                    field = SearchFields [data_type];
                pred.filter.reset (new PermutedFilter (predicate [PREDKEY_ITEMLIST].toList<std::string>(), field));
                break;
            }
            default:
                assert (!"Unhandled manner");
                throw CommandError (E_BUG);
        }
        return pred;
    }

    /** Return just the filter portion of a predicate, for external use.
        @see getFullPredicate. */
    std::unique_ptr<Filter> getPredicate (const PianodConnection &conn,
                                     const Parsnip::Data &predicate,
                                     Filter::Field field) {
        Predicate pred = getFullPredicate (conn, predicate, field);
        return std::move (pred.filter);
    }

    /** Specialization for getting things by ID.
        @param predicate The predicate options.
        @throw CommandError if the ID does not match any item.
        @note Specifying the same ID multiple items will produce duplicates
        in the returned list return. */
    static ThingieList getPredicateIdItems (const Parsnip::Data &predicate) {
        assert (PianodInterpreter::optionIs (predicate, PREDKEY_MANNER, "id"));
        std::vector<std::string> ids{ predicate [PREDKEY_ITEMLIST].toList<std::string>() };

        ThingieList results;
        for (const auto &id : ids) {
            MusicThingie *thing = media_manager->getAnythingById (id);
            if (!thing)
                throw CommandError (E_NOTFOUND, id);
            results.push_back (thing);
        }
        return results;
    }

    /*
     *                  Generic things
     */

    /** Gather a list of assorted things specified by a predicate, which must be
        present.  This differs from the standard call in that when querying
        the media manager, it does not consider a single source's inability to
        complete a query as a failure.
        @param pred The predicate, including source and filter.
        @param search_what Specifies manner of search.
        @return The things specified.
        @throw CommandError if no matching things are found.
        @throw Query::impossible If no sources could handle the query. */
    static ThingieList getPartialSpecifiedThings (Predicate &pred, SearchRange search_what) {
        assert (pred.source == media_manager);

        ThingieList matching_things;
        bool any_possible = false;
        bool any_impossible = false;
        for (Media::Source *source : media_manager->getReadySources()) {
            try {
                matching_things.join (source->getSuggestions (*pred.filter.get(), search_what));
                any_possible = true;
            } catch (const Query::impossible &e) {
                any_impossible = true;
            } catch (const CommandError &e) {
                if (e.reason() != E_MEDIA_ACTION)
                    throw;
                any_impossible = true;
            }
            matching_things.join (source->getPlaylists (*pred.filter.get()));
        }
        if (any_impossible && !any_possible) {
            throw Query::impossible();
        }

        if (matching_things.empty())
            throw CommandError (E_NOTFOUND);
        return matching_things;
    }

    /** Gather a list of assorted things specified by a predicate.
        The predicate must be present.
        @param conn The connection for which the predicate is being interpreted.
        @param predicate The predicate options.
        @param search_what Nature of items to search for (requestable, etc).
        @return The things specified.
        @throw CommandError if no matching things are found. */
    ThingieList getSpecifiedThings (const PianodConnection &conn,
                                    const Parsnip::Data &predicate,
                                    SearchRange search_what) {
        assert (havePredicate (predicate));
        if (PianodInterpreter::optionIs (predicate, PREDKEY_MANNER, "id")) {
            return getPredicateIdItems (predicate);
        }

        Predicate pred{ getFullPredicate (conn, predicate, Filter::Field::Search) };
        if (pred.source == media_manager && predicate.contains (PREDKEY_FULFILLMENT)
            && PianodInterpreter::optionIs (predicate, PREDKEY_FULFILLMENT, "discretionary")) {
            return getPartialSpecifiedThings (pred, search_what);
        }

        ThingieList matching_things{ pred.source->getSuggestions (*pred.filter.get(), search_what) };
        PlaylistList matching_playlists{ pred.source->getPlaylists (*pred.filter.get()) };
        bool for_request = forRequest (search_what);
        for (auto playlist : matching_playlists) {
            if (!for_request || playlist->canQueue()) {
                matching_things.push_back (playlist);
            }
        }

        if (matching_things.empty() && !matching_playlists.empty()) {
            throw CommandError (E_MEDIA_ACTION, "Cannot request");
        }
        if (matching_things.empty())
            throw CommandError (E_NOTFOUND);
        return matching_things;
    }

    /** Interpret a request for an unspecified type of thing and return it.
        Predicate must be present.
        @return The requested thing.
        @throw CommandError if no things or multiple things are found. */
    MusicThingie *getSpecifiedThing (const PianodConnection &conn, const Parsnip::Data &predicate) {
        assert (havePredicate (predicate));
        ThingieList matches{ getSpecifiedThings (conn, predicate) };
        if (matches.size() > 1)
            throw CommandError (E_AMBIGUOUS);
        return matches.front();
    }

    /*
     *                  Playlists
     */

    /** Get playlist items by id.
        @param predicate The predicate options. */
    static PlaylistList getSpecifiedPlaylistsById (const Parsnip::Data &predicate) {
        ThingieList things{ getPredicateIdItems (predicate) };
        PlaylistList playlists;
        for (auto thing : things) {
            auto playlist = thing->asPlaylist();
            if (!playlist) {
                auto song = thing->asSong();
                if (song) {
                    playlist = song->playlist();
                    if (!playlist) {
                        throw CommandError (E_NO_ASSOCIATION, thing->id());
                    }
                }
            }
            if (!playlist) {
                throw CommandError (E_WRONGTYPE, thing->id());
            }
            playlists.push_back (playlist);
        }
        return playlists;
    }

    /** Interpret a list of playlists specified by a predicate.
        @param conn The connection for which the predicate is being interpreted.
        @param predicate The predicate options.
        @return The playlists specified.  If the predicate is missing,
        returns all playlists for the current source.
        @throw CommandError if no matching playlists are found. */
    PlaylistList getSpecifiedPlaylists (const PianodConnection &conn, const Parsnip::Data &predicate) {
        if (!havePredicate (predicate))
            return conn.source()->getPlaylists();
        if (PianodInterpreter::optionIs (predicate, PREDKEY_MANNER, "id"))
            return getSpecifiedPlaylistsById (predicate);

        Predicate pred{ getFullPredicate (conn, predicate, Filter::Field::Playlist) };
        PlaylistList matching_playlists = pred.source->getPlaylists (*pred.filter.get());
        if (matching_playlists.empty())
            throw CommandError (E_NOTFOUND);

        return matching_playlists;
    }

    /** Gather a request for a single playlist and return it.
        Predicate must be present.
        @return The requested playlist, or nullptr.
        @throw CommandError if no playlists or multiple playlists are found.*/
    PianodPlaylist *getSpecifiedPlaylist (const PianodConnection &conn, const Parsnip::Data &predicate) {
        assert (havePredicate (predicate));
        PlaylistList matches{ getSpecifiedPlaylists (conn, predicate) };
        if (matches.size() > 1)
            throw CommandError (E_AMBIGUOUS);
        return matches.front();
    }

    /*
     *                  Songs
     */

    /** Interpret a list of songs specified by a predicate.
        @param conn The connection for which the predicate is being interpreted.
        @param predicate The predicate options.
        @param search_what Specifies manner of search.
        @return The songs specified or belonging to the artists, albums, or
        playlists specified.
        @throw CommandError if no matching songs are found. */
    SongList getSpecifiedSongs (const PianodConnection &conn, const Parsnip::Data &predicate, SearchRange search_what) {
        assert (search_what == SearchRange::KNOWN || search_what == SearchRange::REQUESTS);

        ThingieList things = getSpecifiedThings (conn, predicate, search_what);
        if (things.empty())
            throw CommandError (E_NOTFOUND);

        /* Songs may be returned from searches or from membership in playlists,
           causing duplication.  Deduplicate the results, retaining ordering. */
        SongList songs;
        songs.reserve (things.size());
        std::unordered_set<std::string> present;
        for (auto thing : things) {
            if (!thing->canQueue()) {
                throw CommandError (E_MEDIA_ACTION, "Requests not supported");
            }
            SongList expanded = thing->songs();
            for (auto song : expanded) {
                const std::string &id = song->id();
                if (present.find (id) == present.end()) {
                    present.insert (id);
                    songs.push_back (song);
                }
            }
        }
        return songs;
    }

    /** Gather a request for a single song and return it.
        Predicate must be present.
        @return The requested song, or nullptr.
        @throw CommandError if no songs or multiple songs are found.*/
    PianodSong *getSpecifiedSong (const PianodConnection &conn, const Parsnip::Data &predicate) {
        assert (havePredicate (predicate));
        SongList matches{ getSpecifiedSongs (conn, predicate) };
        if (matches.size() > 1)
            throw CommandError (E_AMBIGUOUS);
        return matches.front();
    }

}  // namespace Predicate
