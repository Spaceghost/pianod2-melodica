///
/// Convert a filter into a list of queries based on a list of capabilities.
/// @file       querylist.h - pianod
/// @author     Perette Barella
/// @date       2015-01-21
/// @copyright  Copyright (c) 2015-2017 Devious Fish. All rights reserved.
///

#ifndef __pianod__querylist__
#define __pianod__querylist__

#include <config.h>

#include <cstdint>

#include <typeinfo>
#include <vector>
#include <stdexcept>

#include "enumeratedarray.h"
#include "querylist.h"
#include "filter.h"

/// Filter-to-query converter.
namespace Query {
    /// Exception indicating a query is impossible to perform on a source.
    class impossible : public std::invalid_argument {
    public:
        explicit impossible (void) : invalid_argument ("impossible") { };
    };
    typedef int8_t Preference;

    /// Manners in which fields may be queried for a source.
    typedef enum search_methods_t {
        SearchMethodNone,
        ExactCompare,
        MatchBeginning,
        SubstringMatch,
        Fuzzy
    } SearchMethod;

    /** Media sources mark the available fields (array index) to
        true to indicate they are capable of searching in that manner.  */
    class Constraints {
    public:
        /// Fields searched by permuted filters, i.e., 'like'.
        EnumeratedArray<Filter::Field, bool> participatesInFuzzy;
        EnumeratedArray<Filter::Field, bool> canExactCompare;
        EnumeratedArray<Filter::Field, bool> canMatchBeginning;
        EnumeratedArray<Filter::Field, bool> canSubstringMatch;
        /// Fields that can't be properly searched on, but are part of Field::Search searches.
        EnumeratedArray<Filter::Field, bool> fieldInGeneralSearch;
        /// Relative field preferences: higher is better
        EnumeratedArray<Filter::Field, Preference> fieldPreference;
        /// May query with 'AND' semantics.
        bool andCapable = false;
    };

    class DetailList;
    class Queries;
    class List;

    /// Information about how a query should be performed by a source.
    class Details {
        friend class DetailList; // Collection manages quality.
    private:
        float quality = 1; /// Specificity of the search; more is better.
        Filter::Field filterField = Filter::Field::Invalid; /// Field filtered on
    public:
        Filter::Field searchField = Filter::Field::Invalid; ///< Field to search on
        SearchMethod searchMethod = SearchMethodNone; ///< Method of comparison
        std::string value; ///< Value to search for

        Details (Filter::Field filter_field,
                 Filter::Field field, SearchMethod method, const char *val);
    };

    /** A list of queries representing a filter expression or portion thereof.
        Queries represent intersection (AND) of data. */
    class DetailList : public std::vector<Details> {
        friend class Queries;
        unsigned preference (const Constraints &con) const; ///< Field Preference of searches
        float quality (void) const; ///< Specificity of the collection of searches.

    };


    /** A list of queries representing a filter expression or portion thereof.
        Queries represent union (OR) of data. */
    class Queries: public std::vector<DetailList> {
        friend class List;
        unsigned preference (const Constraints &con) const; ///< Field Preference of searches
        float quality (void) const; ///< Specificity of the collection of searches.
    };

    /** A adapter to convert filters into a list of queries for a source. */
    class List : public Queries {
    public:
        const Constraints &capabilities;
        SearchMethod findMethodForField (SearchMethod requestedMethod,
                                         Filter::Field field);
        Details interpretComparison (const Filter::Operation *filter);
        Queries interpretAnd (const Filter::Operation *filter);
        Queries interpret (const Filter::Operation *filter);
        Queries interpretFuzzy (const PermutedFilter &filter);
        List (const Filter &filter, const Constraints &constraints);
    };
}

#endif // defined(__pianod__querylist__) 
