///
/// Ratings support.
/// Ratings enumerations, conversion to stars, averaging and text equivalence.
/// @file       ratings.h - pianod2
/// @author     Perette Barella
/// @date       2017-02-04
/// @copyright  Copyright © 2017-2021 Devious Fish. All rights reserved.
///

#pragma once

#include <config.h>

#include "lookup.h"

/// Rules of who may rate an object.
enum class RatingScheme {
    NOBODY,     ///< Ratings are unsupported.
    INDIVIDUAL, ///< Each user gets a private rating for this song
    OWNER       ///< One rating/song, so only ratable by owner.
};

/// Discrete ratings values.
enum class Rating {
    UNRATED,     ///< Unrated/no rating found, 0 enum.
    REPUGNANT,   ///< 0.5 star, 1 enum
    AWFUL,       ///< 1 star, 2 enum
    BAD,         ///< 1.5 star, 3 enum
    POOR,        ///< 2 star, 4 enum
    LACKLUSTER,  ///< 2.5 star, 5 enum
    NEUTRAL,     ///< 3 star, 6 enum
    OKAY,        ///< 3.5 star, 7 enum
    GOOD,        ///< 4 star, 8 enum
    EXCELLENT,   ///< 4.5 star, 9 enum
    SUPERB       ///< 5 star, 10 enum
};

/// Convert from a ratings enumeration to a floating-point star-rating.
inline constexpr float ratingAsFloat (Rating rating) {
    return (((float)(rating)) / 2);
}

/// A class to average ratings.
class RatingAverager {
    unsigned count = 0; ///< Number of ratings being averaged.
    unsigned total = 0; ///< Total of enumeration values being averaged.
public:
    /** Add mote data to the average.
     @param rating The rating to add.  If UNRATED, the rating is ignored. */
    inline void add (Rating rating) {
        if (rating != Rating::UNRATED) {
            count++;
            total += static_cast <int> (rating);
        }
    }
    /** Return the average as a star-rating float.
     @param default_value Value to return if no values were included in the average.
     @return A floating point average in the star-rating range. */
    float operator ()(Rating default_value = Rating::UNRATED) {
        return (count ? (static_cast <float> (total) / static_cast <float> (count * 2))
                : ratingAsFloat (default_value));
    }
    /** Return the average as a rating-range double.
     @param default_value Value to return if no values were included in the average.
     @return A floating point average across the ratings enumeration values. */
    double rating (Rating default_value = Rating::UNRATED) {
        return (count ? (static_cast <double> (total) / static_cast <double> (count))
                : static_cast <double> (default_value));
    }
    /// Retrieve the sum of the ratings in the average.
    inline unsigned sum () const { return total; };
    /// Retrieve the number of items included in the average.
    inline unsigned items () const { return count; };

};


Rating floatToRating (float rating);

typedef struct rating_lookup_t {
    Rating value;
    const char *name;
} RATING_LOOKUP;

/** String to rating conversion.  The standard Lookup functions use the ratings
    enumeration values, which requires rounding, suitable for storage.
    The `Precise` variations return unrounded star-range ratings (0 < rating ≤ 5),
    suitable for working with averages. */
class RatingLookup : public LookupTable<Rating, RATING_LOOKUP> {
    typedef LookupTable<Rating, RATING_LOOKUP> base_class;
public:
    using base_class::LookupTable;
    bool tryGetPrecise (const char *s, float *rating) const noexcept;
    inline bool tryGetPrecise (const std::string &s, float *rating) const noexcept {
        return tryGetPrecise (s.c_str(), rating);
    }
    float getPrecise (const char *s) const;
    inline float getPrecise (const std::string &s) const {
        return getPrecise (s.c_str());
    }
    virtual const RATING_LOOKUP *get (const char *s) const noexcept override;
};

extern const RatingLookup RATINGS;

