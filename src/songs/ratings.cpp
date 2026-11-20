///
/// Ratings support.
/// Ratings enumerations, conversion to stars, averaging and text equivalence.
/// @file       ratings.cpp - pianod2
/// @author     Perette Barella
/// @date       2017-02-04
/// @copyright  Copyright © 2017-2021 Devious Fish. All rights reserved.
///

#include <config.h>

#include <cmath>

#include "lookup.h"
#include "ratings.h"



/// Rating enumeration to text lookup values.
const static RATING_LOOKUP ratings_def [] = {
    // First values are in bad..good order; enum matches array index:
    // Numeric ratings retrieve item by index (checked by assert too).
    { Rating::UNRATED,   "unrated" },
    { Rating::REPUGNANT, "repugnant" },
    { Rating::AWFUL,     "awful" },
    { Rating::BAD,       "bad" },
    { Rating::POOR,      "poor" },
    { Rating::LACKLUSTER,"lackluster" },
    { Rating::NEUTRAL,   "neutral" },
    { Rating::OKAY,      "okay" },
    { Rating::GOOD,      "good" },
    { Rating::EXCELLENT, "excellent" },
    { Rating::SUPERB,    "superb" },
    // Alternate names must go at end:
    { Rating::AWFUL,     "terrible" },
    { Rating::OKAY,      "ok" },
    { Rating::UNRATED,   nullptr }
};

const RatingLookup RATINGS (ratings_def);

/** Retrieve the enumerator corresponding to text name.
    @param s The text name
    @return Pointer to table entry, or NULL if not found. */
const RATING_LOOKUP *RatingLookup::get (const char *s) const noexcept {
    if (isdigit (*s) || *s == '.') {
        char *err;
        float rating = strtof(s, &err);
        if ((*err) || rating < 0.5 || rating > 5.0) return nullptr;
        Rating r = floatToRating (rating);
        assert (ratings_def [static_cast <int> (r)].value == r);
        return &ratings_def [static_cast <int> (r)];
    } else {
        return base_class::get (s);
    }
}


/** Retrieve the enumerator corresponding to text name.
    @param s The text of the rating.
    @param rating [out] The parsed rating.
    @return True if the value was a valid rating, false otherwise. */
bool RatingLookup::tryGetPrecise (const char *s, float *rating) const noexcept {
    if (isdigit (*s) || *s == '.') {
        char *err;
        *rating = strtof(s, &err);
        return (!*err && *rating >= 0.0 && *rating <= 5.0);
    }
    Rating r;
    if (tryGetValue (s, &r)) {
        *rating = ratingAsFloat (r);
        return true;
    }
    return false;
}

/** Get a precise rating, accepting either a rating string or number.
    @param s A string containing a rating.
    @return The rating in the star-rating range.
    @throw E_INVALID if the value is out of range, not a rating, or otherwise invalid. */
float RatingLookup::getPrecise (const char *s) const {
    float rating;
    if (!tryGetPrecise (s, &rating))
        throw CommandError (E_INVALID, s);
    return rating;
};



/** Convert a star-rating to a ratings enumeration.
    @return Closes equivalent rating from the ratings enumeration. */
Rating floatToRating (float rating) {
    int intRating = round (rating * 2);
    if (intRating <= 0) return Rating::REPUGNANT;
    if (intRating > 10) return Rating::SUPERB;
    return (static_cast <Rating> (intRating));
}

