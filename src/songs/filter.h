///
/// Track filter.
/// A filter is created from an string expression, from which it builds
/// an efficient expression tree.  The filter can then be used to assess
/// whether MusicThingies of any kind matches the expression.
/// If the filter compares on a field that isn't present in the thingie,
/// that comparison resolves to false.
///
/// @file       filter.h - pianod project
/// @author     Perette Barella
/// @date       Integrated: 2014-12-07.  Initial version: December 2005.
/// @copyright  Copyright 2005, 2014-2021 Devious Fish. All rights reserved.
/// @see        History comment in filter.cpp
///

#pragma once

#include <config.h>

#include <cstdlib>

#include <string>
#include <vector>
#include <memory>

#include "enumeratedarray.h"
#include "musictypes.h"

namespace Query {
    class Constraints;
    class Details;
    class List;
};

/** Track data filter.  Created from a logical statement describing match
    criteria, the track filter can then be used to test if a track/song,
    album, artist or playlist matches the criteria. */
class Filter {
    friend class Constraints;
    friend class Query::Details;
    friend class Query::List;
public:
    /// Fields on which comparisons can be done by the filter.
    enum class Field {
        Invalid = 0,
        Id,
        Artist,
        Album,
        Type,
        Genre,
        Playlist,
        Title,          ///< Song name.
        Search,
        Name,
        Year,
        Duration,
        Lastplay,
        Track,          ///< Track number
        Rating,
        Played,
        Heard,
        Rated,
        UserRating,
        False,
        Compilation,
        Count
    };
    static const unsigned FieldCount = static_cast <unsigned> (Field::Count);

    /// Filter operations; each node in the parse tree has one action
    /// indicating how to proceed.
    enum class Action {
        Noop,
        And,
        Or,
        LessThan,
        LessEqual,
        Equal,
        NotEqual,
        GreaterThan,
        GreaterEqual,
        Match
    };

    using DuplicationFlags = EnumeratedArray<Field, bool>;
    using FilterRating = float;

protected:
    /// Error for generic filter syntax/parse errors.
    class ParseError : public std::exception {
    public:
        std::string reason;
        const char *point;
        ParseError (const std::string &_reason, const char *where)
        : reason (_reason), point (where) {};
    };
    /// Error for filter expression containing an invalid value.
    class InvalidValue : public std::exception {
    public:
        std::string field;
        std::string value;
        InvalidValue (const std::string &_field, const std::string &_value)
        : field (_field), value (_value) {};
    };
    /// Error for filter expressions trying to use a relational operation incorrectly.
    class DisallowedOperator : public std::exception {
    public:
        std::string field;
        std::string op;
        DisallowedOperator (const std::string &_field, const std::string &_operator)
        : field (_field), op (_operator) {};
    };

    class Operation;
    /// Container for Operation, to ensure instances are cleaned up.
    using Op = std::unique_ptr<Operation>;
    inline const char *skipWhitespace (const char **const command) {
        while (isspace (**command)) {
            (*command)++;
        }
        return *command;
    }

    std::string gatherString (const char **const command, const char *error_reason);
    Op parseParenthesis (const char **const command);
    Op parseComparison (const char **const command);
    Op parseSearch (const char **const command);
    Op parseNegation (const char **const command);
    Op parseOr (const char **const command);
    Op parseAnd (const char **const command);

    /// Nodes for the filter parse tree.
    class Operation {
        friend class Query::List;
        friend Op Filter::parseNegation (const char **const command);
    private:
        Field field = Field::Invalid; ///< For comparison operations, the field to match.
        Action action = Action::Noop; ///< The operation: and/or, comparison type, etc.
        bool invert = false; ///< Apply not operator to result of action.
        union value_t {
            long numeric; ///< Numeric comparison value, when applicable.
            MusicThingie::Type type; ///< Type of results, when applicable
            /// For ratings and user-specific operations
            struct rating_t {
                float value; ///< Rating value, when applicable.
                const User *user; ///< A user, for applicable actions.
            } rating;
            /// For string operations
            struct string_t {
                char *value; ///< String comparison value, when applicable.
                size_t match_length; ///< Portion of string to match; selects exact vs wildcard match.
            } string;
            /// For and/or operations
            struct subnodes_t {
                Operation *l_eval; /// Left side of AND/OR
                Operation *r_eval; /// Right side of AND/OR
            } nodes;
        } value;
        void clear ();
    public:
        inline Operation () {
            memset (&value, '\0', sizeof (value));
        };
        inline ~Operation () {
            clear ();
        };

        std::string toString ();
        /** Construct a check or comparision.
            @param _action The manner of comparison: equal, not equal, etc.
            @param _field The field on which to compare or test. */
        inline Operation (const Action _action, const Field _field)
        : Operation () {
            action = _action;
            field = _field;
        };
        Operation (const Action action, std::unique_ptr<Filter::Operation> &&left, std::unique_ptr<Filter::Operation> &&right);
        Operation (const Action action, const Field field, const std::string &_value, const bool wildcard = false);
        Operation (const Action action, const Field field, const long value);
        Operation (const Action action, const Field field, const FilterRating value, const User *user);
        Operation (const Action action, const Field field, const MusicThingie::Type type);

        // Copy and move operations.
        Operation (const Operation &) = delete;
        Operation &operator =(const Operation &) = delete;
        Operation (Operation &&from);
        Operation &operator =(Operation &&from);


        // Operation execution methods
        template<typename DiffType>
        bool applyOperation (DiffType diff) const;
        
        /** Perform a numeric comparision.
            @param compare_to Value being compared.  Zero is null, so always false.
            @return True if values compare according to action, false otherwise. */
        inline bool numericCheck (long compare_to) const {
            return compare_to == 0 ? false : applyOperation (compare_to - value.numeric);
        };
        bool stringCheck (const std::string &compare_to) const;

        bool genreCheck (const std::string &list) const;

        bool evaluate (const PianodPlaylist *playlist) const;
        bool evaluate (const PianodArtist *artist) const;
        bool evaluate (const PianodAlbum *album) const;
        bool evaluate (const PianodSong *track) const;

        // Static functions
        static bool isNumericOperator (Action action);
        static bool isStringOperator (Action action);
        static bool isStringField (Field field);
        static MusicThingie::Type getTypeFromField (const Filter::Field field);
    };

    std::shared_ptr<Operation> parsetree;
    bool persistable = true; ///< Not true if user referenced or other problematic expressions.

public:
    const static Filter All; ///< Selects all tracks
    const static Filter None; ///< Selects nothing

    Filter (void) {};
    Filter (const Filter &) = default;
    Filter (Filter &&from_filter) = default;
    Filter (const std::string &expression);
    Filter (const MusicThingie *thing,
            const MusicThingie::Type type = MusicThingie::Type (0),
            const DuplicationFlags &manner = DuplicationFlags (true));

    Filter &operator=(const std::string &expression);
    Filter &operator=(const Filter &filter) = default;
    Filter &operator=(Filter &&from_filter) = default;


    virtual ~Filter ();

    /** Check if a filter matches a song, album, artist or playlist.
        @param thing The item to compare.
        @return True if it matches, false otherwise. */
    template<typename MusicThing> inline bool matches (const MusicThing *thing) const {
        if (!parsetree) return true;
        return (parsetree->evaluate (thing));
    };

    inline bool canPersist (void) const  {
        return persistable;
    }
    std::string toString () const;
};

template<> bool Filter::matches<MusicThingie> (const MusicThingie *thing) const;



/** Permuted filter, like Filter but constructed from a string.
    The filter matches track data containing all words in the string. */
class PermutedFilter: public Filter {
    friend class Query::List;
private:
    std::vector<std::string> phrases;
    const Field target_field;
public:
    PermutedFilter (const std::string &phrase, Field search_target = Field::Search);
    PermutedFilter (const std::vector<std::string> &phrases, Field search_target = Field::Search);
};



/** Permuted filter, like Filter but constructed from a list of strings.
    The filter matches track data that contains any of the strings. */
class ListFilter: public Filter {
public:
    ListFilter (const std::vector<std::string> &items, Field search_target = Field::Search);
};

