///
/// Convert a filter into a list of queries based on a list of capabilities.
/// @file       querylist.cpp - pianod
/// @author     Perette Barella
/// @date       2015-01-21
/// @copyright  Copyright 2015-2021 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <exception>
#include <stdexcept>

#include "querylist.h"

namespace Query {
    /** Create a new query and calculate its quality. */
    Details::Details (Filter::Field filter_field,
                      Filter::Field field, SearchMethod method, const char *val)
    : filterField (filter_field), searchField (field),
    searchMethod (method), value (val) {
        assert (method != SearchMethodNone);

        // For less exacting comparison methods, string length matters more.
        float good_length = (method == ExactCompare ? 1 :
                             method == MatchBeginning ? 6 : 20);
        // Assess string length, but give some bonus for using a specified field.
        quality = ((float (value.length()) / good_length) *
                   (field == Filter::Field::Search ? 3.0 : 1.0));
        if (quality > 1.0f) quality = 1.0f;
    }

    /** Determine field preference for a query list.
        Preference is the sum of the field preferences. */
    unsigned DetailList::preference (const Constraints &con) const {
        unsigned pref = 0;
        for (auto const &item: *this) {
            pref += con.fieldPreference [item.filterField];
        }
        return pref;
    }

    /** Calculate quality for a details list.
        Quality improves the more specific it is. */
    float DetailList::quality() const {
        if (empty())
            return 99999.0; // Very specific: never return anything.
        float qual = 0.0;
        for (auto const &item: *this) {
            qual += item.quality;
        }
        if (qual > 1.0f) qual = 1.0f;
        return qual;
    }



    /** Determine field preference for a query list.
        Preference is the highest-preference query detail list. */
    unsigned Queries::preference(const Constraints &con) const {
        unsigned pref = 0;
        for (auto const &item: *this) {
            unsigned this_pref = item.preference (con);
            if (this_pref > pref) {
                pref = this_pref;
            }
        }
        return pref;
    }

    /** Calculate quality for a query list.
        Quality declines the less specific it is;
        i.e., more queries is bad. */
    float Queries::quality() const {
        if (empty())
            return 99999.0; // Very specific: never return anything.
        float qual = 1.0f;
        for (auto const &item: *this) {
            qual *= item.quality();
        }
        return qual;
    }


    /** Determine the best method of querying for a field, taking
        into account the source's capabilities. */
    SearchMethod List::findMethodForField (SearchMethod requestedMethod,
                                           Filter::Field field) {

        if (requestedMethod == ExactCompare &&
            capabilities.canExactCompare [field]) {
            return ExactCompare;
        }

        if ((requestedMethod == ExactCompare ||
             requestedMethod == MatchBeginning) &&
            capabilities.canMatchBeginning [field]) {
            return MatchBeginning;
        }

        if (capabilities.canSubstringMatch [field]) {
            return SubstringMatch;
        }

        return SearchMethodNone;
    }


    Details List::interpretComparison (const Filter::Operation *filter) {
        // Special case of YEAR = n.
        if (filter->field == Filter::Field::Year &&
            filter->action == Filter::Action::Equal &&
            capabilities.canExactCompare [Filter::Field::Year]) {
            return Details (Filter::Field::Year, Filter::Field::Year,
                            SearchMethod::ExactCompare,
                            std::to_string (filter->value.numeric).c_str());
        }


        if (!Filter::Operation::isStringField (filter->field))
            throw impossible();

        // Filter match length is set to strlen + 1 for exact match
        // (Ensuring null byte checked.)
        // fa_equal && match_length == strlen (value) is wildcard (beginning) search.
        // fa_equal && match_length
        // fa_match -> substring search.

        // First see if we support the thing natively.
        SearchMethod requestedMethod;
        switch (filter->action) {
            case Filter::Action::Equal:
                requestedMethod = (strlen (filter->value.string.value) < filter->value.string.match_length
                                   ? ExactCompare : MatchBeginning);
                break;
            case Filter::Action::Match:
                requestedMethod = SubstringMatch;
                break;
            default:
            {
                throw impossible();
            }
        }

        SearchMethod actualMethod = findMethodForField (requestedMethod, filter->field);
        if (actualMethod != SearchMethodNone) {
            return Details (filter->field, filter->field, actualMethod, filter->value.string.value);
        }

        if (!capabilities.fieldInGeneralSearch [filter->field])
            throw impossible();

        actualMethod = findMethodForField (requestedMethod, Filter::Field::Search);
        return Details (filter->field, Filter::Field::Search, actualMethod, filter->value.string.value);
    };


    Queries List::interpretAnd (const Filter::Operation *filter) {
        Queries left (interpret (filter->value.nodes.l_eval));
        Queries right (interpret (filter->value.nodes.r_eval));

        // We can't handle ORs within ANDs.  Avoid them.
        if (left.size() > 1 && right.size() >> 1)
            return Queries {};
        if (left.size() != 1) return right;
        if (right.size() != 1) return left;

        // If we have AND capabilities, assemble them.
        if (capabilities.andCapable) {
            for (const Details &details: right [0]) {
                left [0].push_back (details);
            }
            return left;
        }

        // If certain queries are preferred, use those.
        int pref = (left.preference(capabilities) -
                    right.preference (capabilities));
        if (pref)
            return pref > 0 ? left : right;

        // If no preference, subjectively pick the "best"
        // query of the two (based on specificity of queries).
        return (left.quality() < right.quality()
                ? right : left);
    }

    Queries List::interpret (const Filter::Operation *filter) {
        switch (filter->action) {
            case Filter::Action::And:
            {
                return interpretAnd (filter);
            }
            case Filter::Action::Or:
            {
                Queries left (interpret (filter->value.nodes.l_eval));
                // Impossible; need both for OR
                if (left.empty())
                    return left;

                Queries right (interpret (filter->value.nodes.r_eval));
                left.insert (left.begin(), right.begin(), right.end());
                return left;
            }
            case Filter::Action::Noop:
                // False or compilation checks; these queries never return anything
                return Queries();
            default:
            {
                Queries results;
                try {
                    DetailList details;
                    details.push_back (interpretComparison (filter));
                    results.push_back (details);
                    return results;
                } catch (const impossible &) {
                    return results;
                }
            }
        }
    }

    /** Build a query directly from a permuted filter. */
    Queries List::interpretFuzzy (const PermutedFilter &filter) {
        assert (filter.parsetree);
        Filter::Field target = filter.target_field;
        if (!capabilities.participatesInFuzzy [target] &&
            capabilities.fieldInGeneralSearch [target]) {
            target = Filter::Field::Search;
        }
        if (capabilities.participatesInFuzzy [target]) {
            Queries queries;
            for (auto const &phrase : filter.phrases) {
                DetailList detail_list;
                detail_list.push_back (Details { filter.target_field, target, Fuzzy, phrase.c_str() });
                queries.push_back (detail_list);
            }
            return queries;
        }
        return interpret (filter.parsetree.get());
    }


    List::List (const Filter &filter, const Constraints &constraints)
    : capabilities (constraints) {
        Queries queries {
            typeid (filter) == typeid (PermutedFilter) ?
            interpretFuzzy (static_cast <const PermutedFilter &> (filter)) :
            interpret(filter.parsetree.get()) };

        if (queries.empty())
            throw impossible();
        insert (begin(), queries.begin(), queries.end());
    }

}



#ifdef FILTER_TEST
int main (int argc, char **argv) {
    Filter filter_search("'art of noise'");
    Filter filter_and ("artist = \"madonna\" && albumname =~ 'like a virgin'");
    Filter filter_or("artist='aqua' || artist =~ 'ace of bass'");

    Query::Constraints noCapability;

    Query::Constraints broadSearchOnly;
    broadSearchOnly.canSubstringMatch [Filter::ff_search] = true;
    broadSearchOnly.fieldInGeneralSearch [Filter::ff_author] = true;
    broadSearchOnly.fieldInGeneralSearch [Filter::ff_title] = true;

    Query::Constraints canSpecificSearch;
    canSpecificSearch.canExactCompare [Filter::ff_author] = true;
    canSpecificSearch.canExactCompare [Filter::ff_title] = true;
    canSpecificSearch.canSubstringMatch [Filter::ff_author] = true;
    canSpecificSearch.canSubstringMatch [Filter::ff_title] = true;
    canSpecificSearch.canSubstringMatch [Filter::ff_albumname] = true;
    canSpecificSearch.canSubstringMatch [Filter::ff_search] = true;
    canSpecificSearch.fieldInGeneralSearch [Filter::ff_search] = true;
    canSpecificSearch.fieldInGeneralSearch [Filter::ff_author] = true;
    canSpecificSearch.fieldInGeneralSearch [Filter::ff_title] = true;
    canSpecificSearch.fieldInGeneralSearch [Filter::ff_albumname] = true;

    Query::List search_none (filter_search, noCapability);
    assert (search_none.empty());

    Query::List and_none (filter_and, noCapability);
    assert (search_none.empty());

    Query::List or_none (filter_or, noCapability);
    assert (or_none.empty());




    Query::List search_broad (filter_search, broadSearchOnly);
    assert (!search_broad.empty());

    Query::List and_broad (filter_and, broadSearchOnly);
    assert (!search_broad.empty());

    Query::List or_broad (filter_or, broadSearchOnly);
    assert (!or_broad.empty());



    Query::List search_specific (filter_search, canSpecificSearch);
    assert (!search_specific.empty());

    Query::List and_specific (filter_and, canSpecificSearch);
    assert (!search_specific.empty());

    Query::List or_specific (filter_or, canSpecificSearch);
    assert (!or_specific.empty());
    



    return 0;
}
#endif
