///
/// Track filter.
/// Parsers expression to build a parse tree representing the expression.
/// Filter may then be applied to songs, etc., to determine if they match.
/// @file       filter.cpp - pianod project
/// @author     Perette Barella
/// @date       Incorporated: 2014-12-07.  Initial version: December 2005.
/// @copyright  Copyright 2005, 2014-2021 Devious Fish. All rights reserved.
///
/*  This filter is derived from an original C one written by Perette Barella
    as a drop-in replacement for the filter provided with mserv (mserv.org).
    The new filter built a parse tree to improve performance, and added
    several new search capabilities. */

#define _BSD_SOURCE // strdup 
#define _DEFAULT_SOURCE

#include <config.h>

#include <cctype>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cassert>

#include "filter.h"
#include "utility.h"
#include "user.h"
#include "users.h"

const LookupTable<Filter::Field> FilterFields {
    { "id",             Filter::Field::Id },
    { "artist",         Filter::Field::Artist }, // our preferred word
    { "author",         Filter::Field::Artist }, // mserv compatibility
    { "albumname",      Filter::Field::Album }, // mserv equivalent
    { "album",          Filter::Field::Album }, // in mserv album was album number
    { "type",           Filter::Field::Type }, // Artist, album, song, playlist
    { "genre",          Filter::Field::Genre },
    { "playlist",       Filter::Field::Playlist },
    { "title",          Filter::Field::Title }, // mserv term for song name
    { "song",           Filter::Field::Title }, // our alternative
    { "search",         Filter::Field::Search },
    { "name",           Filter::Field::Name },
    { "year",           Filter::Field::Year },
    { "duration",       Filter::Field::Duration },
    { "lastplay",       Filter::Field::Lastplay },
    { "track",          Filter::Field::Track },
    { "played",         Filter::Field::Played },
    { "heard",          Filter::Field::Heard },
    { "rated",          Filter::Field::Rated },
    { "rating",         Filter::Field::Rating },
    { "false",          Filter::Field::False },
    { "compilation",    Filter::Field::Compilation }
};

std::string Filter::Operation::toString () {
    const char *operation;
    switch (action) {
        case Action::Noop:
            return (std::string (invert ? "!" : "") + FilterFields [field]);
        case Action::And:
        case Action::Or:
            return (std::string (invert ? "!(" : "(")
                    + value.nodes.l_eval->toString()
                    + (action == Action::And ? ") && (" : ") || (")
                    + value.nodes.r_eval->toString()
                    + ")");
            break;
        case Action::LessThan:
            operation = "<";
            break;
        case Action::LessEqual:
            operation = "<=";
            break;
        case Action::Equal:
            operation = "==";
            break;
        case Action::NotEqual:
            operation = "!=";
            break;
        case Action::GreaterThan:
            operation = ">";
            break;
        case Action::GreaterEqual:
            operation = ">=";
            break;
        case Action::Match:
            operation = "=~";
            break;
    }
    std::string field_name = (field == Field::UserRating ? value.rating.user->username()
                         : FilterFields [field]);
    std::string operand;
    switch (field) {
        case Field::Invalid:
        case Field::Count:
        case Field::False:
        case Field::Compilation:
            assert (!"Invalid operation/no operand");
            throw std::invalid_argument ("Comparison operator on boolean field");
        case Field::Id:
        case Field::Artist:
        case Field::Album:
        case Field::Genre:
        case Field::Playlist:
        case Field::Title:
        case Field::Search:
        case Field::Name:
        {
            operand = value.string.value;
            // Quotify it
            std::string::size_type f = 0;
            while ((f = operand.find ('"', f)) != std::string::npos) {
                operand.insert (f, 1, '"');
                f += 2;
            }
            operand.insert (0, 1, '"');
            operand.append (1, '"');
            // Add wildcard if appropriate.
            if (value.string.match_length == strlen (value.string.value)) {
                operand += "*";
            }
            break;
        }
        case Field::Played:
        case Field::Heard:
        case Field::Rated:
            return (std::string (invert ? "!" : "") + field_name);
        case Field::Type:
            operand = THINGIETYPES [value.type];
            break;
        case Field::Year:
        case Field::Duration:
        case Field::Lastplay:
        case Field::Track:
            operand = std::to_string (value.numeric);
            break;
        case Field::UserRating:
        case Field::Rating:
        {
            Rating rounded = floatToRating (value.rating.value);
            if (value.rating.value == 0.0f)
                operand = "unrated";
            else if (value.rating.value == ratingAsFloat (rounded))
                operand = RATINGS [rounded];
            else
                operand = std::to_string (value.rating.value);
            break;
        }
    }
    assert (!field_name.empty());
    return (std::string (invert ? "!" : "") + field_name + operation + operand);
}


/** Check if a field contains string values.
    @param field The field to check.
    @return True if the field is string, false otherwise. */
bool Filter::Operation::isStringField (Field field) {
    return (field == Field::Search ||
            field == Field::Name ||
            field == Field::Artist ||
            field == Field::Album ||
            field == Field::Title ||
            field == Field::Genre ||
            field == Field::Playlist ||
            field == Field::Id);
}

/** Check if an operation applies to numeric values.
    @param action The operation to check.
    @return True if the operation can be used on numbers, false otherwise. */
bool Filter::Operation::isNumericOperator (Action action) {
    return (action == Action::LessThan ||
            action == Action::LessEqual ||
            action == Action::Equal ||
            action == Action::NotEqual ||
            action == Action::GreaterEqual ||
            action == Action::GreaterThan);
}

/** Check if an operation applies to strings.
    @param action The operation to check.
    @return True if the operation can be used on strings, false otherwise. */
bool Filter::Operation::isStringOperator (Action action) {
    return (action == Action::Match || isNumericOperator (action));
}


/** Get the music thingie type corresponding to a field name.
    @param field The field to get the type of.
    @return A MusicThingie Type for the object type.
    @throw InvalidValue if the field has no associated MusicThingie type. */
MusicThingie::Type Filter::Operation::getTypeFromField (const Filter::Field field) {
    switch (field) {
        case Filter::Field::Title:
            return MusicThingie::Type::Song;
        case Filter::Field::Album:
            return MusicThingie::Type::Album;
        case Filter::Field::Artist:
            return MusicThingie::Type::Artist;
        case Filter::Field::Playlist:
            return MusicThingie::Type::Playlist;
        default:
            throw Filter::InvalidValue ("type", FilterFields.get (field)->name);
    }
}


/** Release value in an operation node.  Does not reset the node;
    invoking this twice will double-release unless the values have
    been replaced.  */
void Filter::Operation::clear() {
    if (action == Action::And || action == Action::Or) {
        delete value.nodes.l_eval;
        delete value.nodes.r_eval;
    } else if (isStringField (field)) {
        free (value.string.value);
    }
}

/** Move-construct an operation */
Filter::Operation::Operation (Operation &&from) {
    if (&from == this) return;
    action = from.action;
    field = from.field;
    memcpy (&value, &from.value, sizeof (value));
    from.action = Action::Noop;
    from.field = Field::Invalid;
}
/** Move-assign an operation. */
Filter::Operation &Filter::Operation::operator =(Operation &&from) {
    if (&from != this) {
        clear();
        action = from.action;
        field = from.field;
        memcpy (&value, &from.value, sizeof (value));
        from.action = Action::Noop;
        from.field = Field::Invalid;
    }
    return *this;
}

/** Construct an AND or OR operation joining two other operations.
    One of the two joined operations may be null, in which case the
    constructed node simply inherits the non-null operation.
    @param _action Either AND or OR.
    @param left The left operation.
    @param right The right operation. */
Filter::Operation::Operation (const Action _action, Op &&left, Op &&right)
: Filter::Operation::Operation () {
    assert (_action == Action::Or || _action == Action::And);
    if (!left) {
        assert (right);
        *this = std::move (*right);
    } else if (!right) {
        *this = std::move (*left);
    } else {
        action = _action;
        value.nodes.l_eval = left.release ();
        value.nodes.r_eval = right.release ();
    }
}

/** Construct a numeric comparison operation.
    @param _action The manner of comparison.
    @param _field The field to compare.
    @param _value The value to compare to. */
Filter::Operation::Operation (const Action _action, const Field _field, const long _value)
: Filter::Operation::Operation (_action, _field) {
    assert (isNumericOperator(_action));
    value.numeric = _value;
}

/** Construct a string comparison.
    @param _action The manner of comparison.
    @param _field The field to compare.
    @param _value The value to compare to.
    @param wildcard If true, _value need only match the start of string. */
Filter::Operation::Operation (const Action _action, const Field _field,
                              const std::string &_value, const bool wildcard)
: Filter::Operation::Operation (_action, _field) {
    assert (isStringOperator (_action));
    assert (isStringField (_field));
    assert (!wildcard || _action != Action::Match);

    if (!(value.string.value = strdup (_value.c_str()))) {
        throw std::bad_alloc ();
    }
    value.string.match_length = _value.length() + (wildcard ? 0 : 1);
}

/** Construct a rating comparison.
    @param _action The manner of comparison.
    @param _field The field to compare to.
    @param rating The value to compare to.
    @param user The user to apply the comparison to, if applicable. */
Filter::Operation::Operation (const Action _action, const Field _field, const FilterRating rating, const User *user)
: Filter::Operation::Operation (_action, _field) {
    assert (isNumericOperator (action));
    assert (field != Field::UserRating || user);
    assert (field == Field::Rating || field == Field::UserRating);
    value.rating.value = rating;
    value.rating.user = user;
};

/** Construct a type comparision.
    @param _action The manner of comparison.
    @param _field The field to compare to: always Field::Type.
    @param _type The type to compare to. */
Filter::Operation::Operation (const Action _action, const Field _field, const MusicThingie::Type _type)
: Filter::Operation::Operation (_action, _field) {
    assert (action == Action::Equal || action == Action::NotEqual);
    assert (field == Field::Type);
    value.type = _type;
};



/** Apply operation to difference, yielding truth.
    @param diff Difference between two items, or 0 for equal.
    @return True if A op B. */
template<typename DiffType>
bool Filter::Operation::applyOperation (DiffType diff) const {
    switch (action) {
        case (Action::LessThan):
            return (diff < 0);
        case (Action::LessEqual):
            return (diff <= 0);
        case (Action::GreaterThan):
            return (diff > 0);
        case (Action::GreaterEqual):
            return (diff >= 0);
        case (Action::Equal):
            return (diff == 0);
        case (Action::NotEqual):
            return (diff != 0);
        default:
            assert (!"Switch case not matched");
            return false;
    }
}


/** Gather a string, either quoted or not, processing quote characters.
    @param command The point in the command being parsed at which to gather.
    @param error_reason A message to include in a ParseError should there
    be no string to gather.
    @return String found, with quoted characters adapted. */
std::string Filter::gatherString (const char **const command,
                             const char *error_reason) {
    char quote = *skipWhitespace (command);
    const char *before = *command;
    if (quote != '"' && quote != '\'') {
        assert (error_reason);
        // We want to collect letters, digits, unicode characters and '.'.
        // I.e., everything except whitespace or punctuation (but allow . for use in numbers).
        while (**command && ((!ispunct (**command) && !isspace (**command)) || **command == '.')) {
            (*command)++;
        }
        if (before == *command) {
            throw ParseError (error_reason, before);
        }
        std::string value (before, *command);
        skipWhitespace (command);
        return value;
    }

    before = ++(*command);
    while (**command) {
        if (**command == quote) {
            if (*((*command) + 1) != quote) {
                break;
            }
            (*command)++;
        }
        (*command)++;
    }
    std::string value (before, *command);
    if (**command)
        (*command)++;
    skipWhitespace (command);

    char quoted [3] = { quote, quote, '\0' };
    std::string::size_type f;
    while ((f = value.find (quoted)) != std::string::npos) {
        value.replace (f, 2, &quoted [1]);
    }
    return value;
}



/** Handle (parenthesis in an expression).
    @param command The point in the command being parsed.
    On return, updated to reflect expression portions consumed.
    @return Operation tree representing expression parsed. */
Filter::Op Filter::parseParenthesis (const char **const command) {
    if (**command == '(') {
        (*command)++;
        Op parse = parseAnd (command);
        if (**command == ')') {
            (*command)++;
            return parse;
        }
    }
    throw ParseError ("Syntax error", *command);
}


/** Handle bare search term in expression.  This is identified
    by a quote (either ' or ") where we expect a comparison term in the expression.
    @param command The point in the command being parsed.
    On return, updated to reflect expression portions consumed.
    @return Operation representing search term. */
Filter::Op Filter::parseSearch (const char **const command) {
    // Extract string between matching delimiters
    // On missing close delimiter, extract the whole remainder.
    assert (**command == '\'' || **command == '"');
    std::string search_value = gatherString (command, nullptr);
    return Filter::Op {new Operation (Action::Match, Field::Search, search_value)};
}


/** Handle comparison or other base operator in expression.
    @param command The point in the command being parsed.
    On return, updated to reflect expression portions consumed.
    @return Operation representing comparison or operator parsed. */
Filter::Op Filter::parseComparison (const char **const command) {
    const char *before;
    User *user = nullptr;

    before = *command;
    while (isalnum (**command)) {
        (*command)++;
    }
    std::string field_name (before, *command - before);
    Field field;
    if (!FilterFields.tryGetValue(field_name, &field)) {
        // user=<heard|rating> - check for username
        user = user_manager->tryget (field_name);
        if (!user) {
            throw InvalidValue ("field name", field_name);
        }
        field = Field::UserRating;
        persistable = false;
    }
    skipWhitespace (command);

    // Determine comparison
    Action action = Action::Noop;
    before = *command;
    if (**command == '<') {
        (*command)++;
        if (**command == '=') {
            action = Action::LessEqual;
            (*command)++;
        } else {
            action = Action::LessThan;
        }
    } else if (**command == '>') {
        (*command)++;
        if (**command == '=') {
            action = Action::GreaterEqual;
            (*command)++;
        } else {
            action = Action::GreaterThan;
        }
    } else if (**command == '=') {
        (*command)++;
        if (**command == '~') {
            action = Action::Match;
            (*command)++;
        } else {
            if (**command == '=')
                (*command)++; // Allow ==
            action = Action::Equal;
        }
    } else if (**command == '!' &&
               *(*command + 1) == '=') {
        (*command)+= 2;
        action = Action::NotEqual;
    }
    std::string operation (before, *command);

    // Handle all the boolean cases
    if (action == Action::Noop) {
        switch (field) {
            case Field::Artist:
            case Field::Album:
            case Field::Playlist:
            case Field::Title:
                // Short form of type checks.
                return Op (new Operation (Action::Equal, Field::Type, Operation::getTypeFromField (field)));
            case Field::Track:
                // Keyword without comparsion specifies type.
                return Op (new Operation (action, Field::Type, MusicThingie::Type::Song));
            case Field::Played:
            case Field::Heard:
                return Op (new Operation (Action::GreaterEqual, field));
            case Field::Rated:
                return Op (new Operation (Action::GreaterThan, field));
            case Field::Compilation:
            case Field::False:
                return Op (new Operation (action, field));
            default:
                throw ParseError ("Missing operation", *command);
        }
    }

    // Get the right-hand operand
    std::string operand = gatherString (command, "Missing operand");

    switch (field) {
        case Field::Name:
        case Field::Search:
            if (action != Action::Match && action != Action::Equal && action != Action::NotEqual) {
                throw DisallowedOperator (field_name, operation);
                break;
            }
            // FALLTHRU
        case Field::Artist:
        case Field::Album:
        case Field::Playlist:
        case Field::Title:
        {
            // value = string cases
            bool wildcard = false;
            if (action != Action::Match) {
                if (!operand.empty() && *(operand.rbegin()) == '*') {
                    // Wildcard inside the quoted string.
                    // Shorten match to avoid comparing the wildcard indicator
                    wildcard = true;
                    operand.erase (operand.length() - 1);
                } else if (**command == '*') {
                    // Wildcard indicator outside of quotes/after string
                    // (shell-style).  Skip over it.
                    wildcard = true;
                    (*command)++;
                }
            }
            return Op (new Operation (action, field, operand, wildcard));
        }
        case Field::Genre:
            if (action != Action::Equal && action != Action::Match)
                throw DisallowedOperator (field_name, operation);
            return Op (new Operation (action, Field::Genre, operand));
        case Field::Id:
            if (action != Action::Equal && action != Action::NotEqual)
                throw DisallowedOperator (field_name, operation);
            return Op (new Operation (action, Field::Id, operand));
        case Field::Type:
        {
            if (action != Action::Equal && action != Action::NotEqual)
                throw DisallowedOperator (field_name, operation);
            Field match_type;
            if (!FilterFields.tryGetValue (operand, &match_type))
                throw InvalidValue ("type", operand);
            return Op (new Operation (action, Field::Type, Operation::getTypeFromField (match_type)));
        }
        case Field::Track:
        case Field::Year:
        case Field::Duration:
        case Field::Lastplay:
        {
            // value <op> number cases
            if (!Operation::isNumericOperator(action))
                throw DisallowedOperator (field_name, operation);
            long value;
            char *error;
            value = strtol (operand.c_str(), &error, 10);
            if (*error)
                throw InvalidValue ("number", operand);
            return Op (new Operation (action, field, value));
        }
        case Field::Rating:
        case Field::UserRating:
        {
            if (!Operation::isNumericOperator(action))
                throw DisallowedOperator (field_name, operation);
            FilterRating rating;
            if (RATINGS.tryGetPrecise (operand, &rating)) {
                // nothing
            } else if (user && strcasecmp (operand, "rated") == 0) {
                rating = ratingAsFloat (Rating::UNRATED);
                // Only allowed to do == or != for rated.
                if (action == Action::Equal)
                    action = Action::GreaterThan;
                else if (action == Action::NotEqual)
                    action = Action::Equal;
                else
                    throw DisallowedOperator (field_name + ": rated", operation);
            } else {
                throw InvalidValue ("rating", operand);
            }
            return Op (new Operation (action, field, rating, user));
        }
        default:
            // Attempt to use boolean operator as relational.
            throw DisallowedOperator (field_name, operation);
    }
    throw CommandError (E_BUG, *command);
}



/** Handle negation operator, then choose parenthesis or a comparison.
    @param command The point in the command being parsed.
    On return, updated to reflect expression portions consumed.
    @return parse tree representing expression parsed. */
Filter::Op Filter::parseNegation (const char **const command) {
    bool invert = false;

    if (*skipWhitespace (command) == '!') {
        (*command)++;
        invert=true;
    }
    Op parse;
    skipWhitespace (command);
    if (isalpha (**command)) {
        parse = parseComparison (command);
    } else if (**command == '\'' || **command == '"') {
        parse = parseSearch (command);
    } else {
        parse = parseParenthesis (command);
    }
    while (isspace (**command))
        (*command)++;
    parse->invert = (parse->invert != invert);
    return (parse);
}


/** Handle binary OR, either as | or ||.
    @param command The point in the command being parsed.
    On return, updated to reflect expression portions consumed.
    @return parse tree representing expression parsed. */
Filter::Op Filter::parseOr (const char **const command) {
    Op left = parseNegation (command);
    if (**command != '|')
        return left;
    (*command)++;
    if (**command == '|')
        (*command)++; // Allow ||
    Op right = parseOr (command);
    return Op { new Operation (Action::Or, std::move (left), std::move (right)) };
}


/** Handle binary AND, either as & or &&.
    @param command The point in the command being parsed.
    On return, updated to reflect expression portions consumed.
    @return parse tree representing expression parsed. */
Filter::Op Filter::parseAnd (const char **const command) {
    Op left = parseOr (command);
    if (**command != '&')
        return left;
    (*command)++;
    if (**command == '&')
        (*command)++; // Allow &&
    Op right = parseAnd (command);
    return Op { new Operation (Action::And, std::move (left), std::move (right)) };
}


/** Check if a genre/genre list matches the filter comparison.
    @param list A genre or list separated by commas, slashes and plus. */
bool Filter::Operation::genreCheck(const std::string &list) const {
    if (action == Action::Match) {
        // Look for substring of requested field.
        return (strcasestr (list, value.string.value) != nullptr);
    }
    const char *cmp = list.c_str();
    while (*cmp) {
        std::string::size_type len = strcspn (cmp, "/,+");
        if (len == value.string.match_length - 1 &&
            strncasecmp (cmp, value.string.value, len) == 0) {
            return true;
        }
        cmp += len;
        if (*cmp) cmp++;
        while (*cmp && isspace (*cmp)) {
            cmp++;
        }
    }
    return false;
}


/** Check if string value matches filter comparison.
    @param compare_to The string to check against. */
bool Filter::Operation::stringCheck (const std::string &compare_to) const {
    if (action == Action::Match) {
        // Look for substring of requested field.
        return (strcasestr (compare_to, value.string.value) != nullptr);
    }
    /* match_length determines if this is exact or
     partial match. */
    long diff;
    if (strncasecmp (compare_to, "the ", 4) == 0 &&
        strncasecmp (value.string.value, "the ", 4) != 0) {
        diff = strncasecmp (compare_to.c_str() + 4, value.string.value, value.string.match_length);
    } else if (strncasecmp (compare_to, "an ", 3) == 0 &&
               strncasecmp (value.string.value, "an ", 3) != 0) {
        diff = strncasecmp (compare_to.c_str() + 3, value.string.value, value.string.match_length);
    } else if (strncasecmp (compare_to, "a ", 2) == 0 &&
               strncasecmp (value.string.value, "a ", 2) != 0) {
        diff = strncasecmp (compare_to.c_str() + 2, value.string.value, value.string.match_length);
    } else {
        diff = strncasecmp (compare_to, value.string.value, value.string.match_length);
    }
    return applyOperation (diff);
}


/** Determine if a playlist matches a filter. */
bool Filter::Operation::evaluate (const PianodPlaylist *playlist) const {
    bool result = false;

    switch (action) {
        case Action::And:
            result = (value.nodes.l_eval->evaluate (playlist) &&
                      value.nodes.r_eval->evaluate (playlist));
            break;
        case Action::Or:
            result = (value.nodes.l_eval->evaluate (playlist) ||
                      value.nodes.r_eval->evaluate (playlist));
            break;
        default:
        {
            switch (field) {
                case Field::False:
                    result = false;
                    break;
                case Field::Type:
                    result = ((value.type == MusicThingie::Type::Playlist) ^
                              (action == Action::NotEqual));
                    break;
                case Field::Id:
                    result = applyOperation (strcasecmp (playlist->id(), value.string.value));
                    break;
                case Field::Genre:
                    result = genreCheck (playlist->genre());
                    break;
                case Field::Playlist:
                case Field::Name:
                case Field::Search:
                    result = stringCheck (playlist->playlistName());
                    break;
                case Field::Rating:
                case Field::Rated:
                {
                    float rated = playlist->averageRating();
                    if (rated > 0.0f) {
                        result = applyOperation (rated - value.rating.value);
                    }
                    break;
                }
                case Field::UserRating:
                {
                    Rating rated = playlist->rating (value.rating.user);
                    if (rated != Rating::UNRATED) {
                        result = applyOperation (ratingAsFloat (rated) - value.rating.value);
                    }
                    break;
                }
                default:
                    return false;
            }
        }
    }
    if (invert)
        result = !result;
    return (result);
}


/** Determine if an artist (or artist components of an album or song)
    match a filter. */
bool Filter::Operation::evaluate (const PianodArtist *artist) const {
    bool result = false;
    switch (action) {
        case Action::And:
            result = (value.nodes.l_eval->evaluate (artist) &&
                      value.nodes.r_eval->evaluate (artist));
            break;
        case Action::Or:
            result = (value.nodes.l_eval->evaluate (artist) ||
                      value.nodes.r_eval->evaluate (artist));
            break;
        default:
        {
            switch (field) {
                case Field::False:
                    result = false;
                    break;
                case Field::Type:
                    result = ((value.type == MusicThingie::Type::Artist) ^
                              (action == Action::NotEqual));
                    break;
                case Field::Id:
                    result = applyOperation (strcasecmp (artist->id(), value.string.value));
                    break;
                case Field::Name:
                case Field::Search:
                    // Same as author for artists.
                case Field::Artist:
                    result = stringCheck (artist->artist());
                    break;
                default:
                    return false;
            }
        }
    }
    if (invert)
        result = !result;
    return (result);
}


/** Determine if an album (or album components of a song) match a filter. */
bool Filter::Operation::evaluate (const PianodAlbum *album) const {
    bool result = false;

    switch (action) {
        case Action::And:
            result = (value.nodes.l_eval->evaluate (album) &&
                      value.nodes.r_eval->evaluate (album));
            break;
        case Action::Or:
            result = (value.nodes.l_eval->evaluate (album) ||
                      value.nodes.r_eval->evaluate (album));
            break;
        default:
            switch (field) {
                case Field::Type:
                    result = ((value.type == MusicThingie::Type::Album) ^
                              (action == Action::NotEqual));
                    break;
                case Field::Name:
                case Field::Album:
                    result = stringCheck (album->albumTitle());
                    break;
                case Field::Search:
                    result = (stringCheck (album->artist()) ||
                              stringCheck (album->albumTitle()));
                    break;
                case Field::Compilation:
                    result = album->compilation();
                    break;
                default:
                    return evaluate (static_cast<const PianodArtist *> (album));
                    break;
            }
    }
    if (invert)
        result = !result;
    return (result);
}


/** Determine if a song matches a filter. */
bool Filter::Operation::evaluate (const PianodSong *track) const {
    bool result = false;

    switch (action) {
        case Action::And:
            result = (value.nodes.l_eval->evaluate (track) &&
                      value.nodes.r_eval->evaluate (track));
            break;
        case Action::Or:
            result = (value.nodes.l_eval->evaluate (track) ||
                      value.nodes.r_eval->evaluate (track));

            break;
        default:
            switch (field) {
                case Field::Type:
                    result = ((value.type == MusicThingie::Type::Song) ^
                              (action == Action::NotEqual));
                    break;
                case Field::Name:
                case Field::Title:
                    result = stringCheck (track->title());
                    break;
                case Field::Genre:
                    result = genreCheck (track->genre());
                    break;
                case Field::Playlist:
                {
                    PianodPlaylist *playlist = track->playlist();
                    result = stringCheck (playlist ? playlist->playlistName() : "");
                    break;
                }
                case Field::Search:
                    result = (stringCheck (track->artist()) ||
                              stringCheck (track->albumTitle()) ||
                              stringCheck (track->title()));
                    break;
                case Field::Year:
                    result = numericCheck (track->year());
                    break;
                case Field::Duration:
                    result = numericCheck (track->duration());
                    break;
                case Field::Played:
                case Field::Lastplay:
                {
                    // time since last played in hours 
                    time_t last = track->lastPlayed();
                    if (last != 0) {
                        result = numericCheck ((time(NULL) - last) / 3600);
                    }
                    break;
                }
                case Field::Track:
                    result = numericCheck (track->trackNumber());
                    break;
                case Field::Rating:
                case Field::Rated:
                {
                    float rated = track->averageRating();
                    if (rated > 0.0) {
                        result = applyOperation (rated - value.rating.value);
                    }
                    break;
                }
                case Field::UserRating:
                {
                    Rating rated = track->rating (value.rating.user);
                    if (rated != Rating::UNRATED || value.rating.value == 0.0f) {
                        result = applyOperation (ratingAsFloat (rated) - value.rating.value);
                    }
                    break;
                }
                default:
                    return evaluate (static_cast <const PianodAlbum *> (track));
                    break;
            }
    }
    if (invert)
        result = !result;
    return (result);
}


/** Specialization for matching to determine the type of thing being
    tested by the filter expression at run-time.
    @param thing A thing to compare.
    @return true if the thing matches, false if not. */
template<> bool Filter::matches<MusicThingie> (const MusicThingie *thing) const {
    return thing->matches (*this);
}


/** Get a filter's expression. */
std::string Filter::toString () const {
    return (parsetree ? parsetree->toString() : "!false");
}


/** Create a new filter from an expression.
    @param expr The expression for the filter.
    @throw CommandError indicating nature of syntax or semantic errors. 
    bad_alloc may also be thrown. */
Filter::Filter (const std::string &expr) {
    persistable = true;
    const char *parse = expr.c_str();
    try {
        if ((parsetree = parseAnd (&parse)) && !*parse) {
            // Success 
            return;
        }
    } catch (const ParseError &error) {
        if (*error.point)
            throw CommandError (E_EXPRESSION, error.reason + " before " + error.point);
        throw CommandError (E_EXPRESSION, error.reason + " at end of expression");
    } catch (const InvalidValue &invalid) {
        throw CommandError (E_EXPRESSION, "Invalid " + invalid.field + ": " + invalid.value);
    } catch (const DisallowedOperator &disallowed) {
        throw CommandError (E_EXPRESSION, "Can not use " + disallowed.op + " on " + disallowed.field);
    }
    throw CommandError (E_EXPRESSION, std::string { "Syntax error at " } + parse);
}


/** Create a new filter.
    @param thing Criteria for the new filter.
    @param type The type of filter to create.
    Songs can create song, album, or artist filters;
    albums can create album or artist filters.
    @param manner Flags indicating what criteria to include in the filter. */
Filter::Filter (const MusicThingie *thing,
                const MusicThingie::Type type,
                const DuplicationFlags &manner) {
    auto *artist = thing->asArtist();
    if (!artist)
        throw std::invalid_argument ("Require an artist, album or song");
    bool empty = false;

    // Artist name
    Op expression;
    if (manner [Field::Artist]) {
        empty = empty || artist->artist().empty();
        expression = Op {new Operation (Action::Equal, Field::Artist, artist->artist())};
    }

    auto *album = artist->asAlbum();
    if (type != MusicThingie::Type::Artist && album) {
        // Album name
        if (manner [Field::Album]) {
            empty = empty || album->albumTitle().empty();
            Op comparison = Op { new Operation (Action::Equal, Field::Album, album->albumTitle()) };
            expression = Op { new Operation (Action::And, std::move (expression), std::move (comparison)) };
        }

        // Duration
        auto *song = album->asSong();
        if (type == MusicThingie::Type::Song && song) {
            if (manner [Field::Duration]) {
                int duration = song->duration();
                empty = empty || duration == 0;
                int margin = duration < 30 ? 1 : duration > 120 ? 3 : 2;
                Op duration_min = Op {new Operation (Action::GreaterEqual, Field::Duration, duration - margin) };
                Op duration_max = Op { new Operation (Action::LessEqual, Field::Duration, duration + margin) };
                Op comparison = Op {new Operation (Action::And, std::move (duration_min), std::move (duration_max)) };
                expression = Op { new Operation (Action::And, std::move (expression), std::move (comparison)) };
            }

            // Track number
            if (manner [Field::Track]) {
                empty = empty || song->trackNumber() == 0;
                Op comparison = Op { new Operation (Action::Equal, Field::Track, song->trackNumber()) };
                expression = Op { new Operation (Action::And, std::move (expression), std::move (comparison)) };
            }

            // Song name
            if (manner [Field::Title]) {
                empty = empty || song->title().empty();
                Op comparison { new Operation (Action::Equal, Field::Title, song->title()) };
                expression = Op { new Operation (Action::And, std::move (expression), std::move (comparison)) };
            }
        }
    }
    if (empty)
        throw std::invalid_argument ("Criteria fields are empty");
    Op comparison { new Operation (Action::Equal, Field::Type, type) };
    parsetree.reset (new Operation (Action::And, std::move (expression), std::move (comparison)));
}


/** Assign a new expression to a filter.
    @param expr The new expression. */
Filter &Filter::operator=(const std::string &expr) {
    Filter f (expr);
    *this = std::move (f);
    return *this;
}


/// Destroy a filter
Filter::~Filter () {
}


/** Create a new permuted filter from a single phrase.
    @param query The search text to permute.
    "Mary's Little Lamb" permuted is equivalent to the expression:
    `search =~ "Mary's" && search =~ "Little" && search =~ "Lamb"`.
    @param search_target The field to match on.
    @throw invalid_argument if the expression is invalid.
    (For a permuted expression, this simply means there are no words.) */
PermutedFilter::PermutedFilter (const std::string &query,
                                Field search_target)
: target_field (search_target) {
    if (Operation::isStringField (search_target)) {
        std::vector<std::string> words { split_string (query) };
        if (words.empty())
            throw std::invalid_argument ("Empty phrase");
        Op expression;
        for (auto &value : words) {
            expression = Op { new Operation (Action::And, std::move (expression),
                                            Op { new Operation (Action::Match, search_target, value) }) };
        }
        parsetree.reset (expression.release());
        phrases.push_back(query);
        return;
    }
    assert (!"Invalid search_target for permuted filter");
    throw (std::logic_error ("Invalid search target"));
}


/** Create a permuted filter from a several phrases.
    The resulting filter matches permutations of *any* of any of the phrases.
    @param phrasez The strings permute.
    @param search_target The field to match on.
    @throw invalid_argument if the expression is invalid. */
PermutedFilter::PermutedFilter (const std::vector<std::string> &phrasez,
                                Field search_target)
: phrases (phrasez), target_field (search_target) {
    Op expression;
    for (auto phrase : phrases) {
        std::vector<std::string> words { split_string (phrase) };
        if (words.empty())
            throw std::invalid_argument ("Empty phrase");

        Op permute = nullptr;
        for (auto &value : words) {
            permute = Op {new Operation (Action::And, std::move (permute),
                                         Op {new Operation (Action::Match, search_target, value) }) };
        }
        assert (permute);

        expression = Op {new Operation (Action::Or, std::move (expression), std::move (permute))};
    }
    parsetree.reset (expression.release());
}



/** Create a new list filter.
    *Any* of the values causes match, not *all* (this is OR, not AND).
    Values must match exactly.
    @param items The list of values to match.
    @param search_target The field to match on. */
ListFilter::ListFilter (const std::vector<std::string> &items,
                        Field search_target) {
    if (Operation::isStringField (search_target) || search_target == Field::Id) {
        if (items.empty())
            throw std::invalid_argument ("Missing criteria");
        Op expression;
        for (auto &value : items) {
            expression = Op {new Operation (Action::Or, std::move (expression),
                                           Op {new Operation (Action::Equal, search_target, value) }) };
        }
        parsetree.reset (expression.release());
        return;
    }
    assert (!"Invalid search_target for list filter");
    throw (std::logic_error ("Invalid search target"));
}



const Filter Filter::All;
const Filter Filter::None ("false");
