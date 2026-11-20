///
/// Implementations of common functions for Media sources.
/// @file       mediasource.cpp - pianod
/// @author     Perette Barella
/// @date       2014-12-15
/// @copyright  Copyright 2014-2021 Devious Fish.  All rights reserved.
///

#include <config.h>

#include <iostream>
#include <string>

#include "retainedlist.h"
#include "fundamentals.h"
#include "mediaunit.h"
#include "mediaparameters.h"
#include "ownership.h"
#include "user.h"

namespace Media {
    /** Initialize the source.
        @param params Source parameters. */
    Source::Source (SourceParameters *params) : PrimaryOwnership (params->permissions, params->owner) {
        assert (params);
        assert (!params->name.empty());
        parameters = params;
    };

    /** Destroy a source. */
    Source::~Source (void) {
        delete parameters;
    };

    /** Get the name of this source.  The kind/name combination is used
        by the media manager to identify individual sources.
        @return The name of the source. */
    const std::string &Source::name (void) const {
        return parameters->name;
    }

    /** Indicate if arbitrary tracks can be requested from this source.
        @return True if requests are allowed, false otherwise. */
    bool Source::canExpandToAllSongs (void) const {
        return false;
    }

    /** Indicate if a name is required when creating a playlist.
        @return True if a name is required, false otherwise. */
    bool Source::requireNameForCreatePlaylist (void) const {
        return true;
    }

    Parsnip::Data Source::serializeIdentity() const {
        // clang-format off
        return Parsnip::Data {Parsnip::Data::Dictionary,
            Key::SourceId, serialNumber(),
            Key::SourceKind, kind(),
            Key::SourceName, name()
        };
        // clang-format on
    }

    /** Identify the unique source.
        @return A string containing the source's serial number, kind, and name. */
    std::string Source::identify() const {
        return std::to_string (serialNumber()) + " (" + kind() + "/" + name() + ")";
    }

    /** Log status and alert messages by sending them to stdout.
        This function is invoked indirectly so that it may be
        replaced by the media manager, which then posts the
        status notifications to its subscribers. . */
    void Source::defaultStatusHandler (RESPONSE_CODE status, const char *detail) {
        std::cerr << "Source status: " << (int) status << ' ' << detail << '\n';
    }

    /** Send an alert notification regarding this source.
        The source's identity is appended and the message routed through the
        status handler, which is usually routed to the media manager.
        @param message The numeric status code for the alert. */
    void Source::alert (RESPONSE_CODE message) const {
        statusHandler (message, ("Source " + identify()).c_str());
    }

    /** Send an alert notification regarding this source.
        The source's identity is appended and the message routed through the
        status handler, which is usually routed to the media manager.
        @param message The numeric status code for the alert.
        @param detail Additional text pertaining to the alert.
        @param reason Optional text describing the reason for the alert. */
    void Source::alert (RESPONSE_CODE message, const char *detail, const char *reason) const {
        std::string full_details (detail);
        if (reason) {
            full_details += ": ";
            full_details += reason;
        }
        full_details += " for Source " + identify();
        statusHandler (message, full_details.c_str());
    }

    /** Send a status notification regarding this source.  The status is
        routed through `alert`.
        @param detail The status message. */
    void Source::reportStatus (const char *detail) const {
        alert (V_SOURCE_STATUS, detail);
    }

    /** Persist source parameters, attaching them to the owner's user data. */
    void Source::persist (void) const {
        if (!parameters || !isOwned())
            return;
        try {
            UserData::JSONData *data = new UserData::JSONData (kind(), name().c_str());
            try {
                if (isOwned())
                    (*data) ["owner"] = ownerName();
                parameters->persist (*data);
                getOwner()->attachData (data);
            } catch (const std::exception &) {
                delete data;
            }
        } catch (const std::exception &) {
            // Nothing.
        }
    }

    /** Persist data for a source.
        @return True on success, false on error. */
    bool Source::flush (void) {
        return true;
    }

    /** Create a playlist using a thingie as an initial seed.
        Seed type allows, for example, to use a song's artist.
        @param name The name of the playlist.
        @param type The type of seed to create.
        @param from The initial seed.
        @return The playlist created.
        @throw CommandError if a request is invalid. */
    PianodPlaylist *Source::createPlaylist (const char *name, MusicThingie::Type type, MusicThingie *from) {
        (void) name;
        (void) type;
        (void) from;
        throw CommandError (E_NOT_IMPLEMENTED);
    }

    /** Create a playlist using a filter.
        @param name The name of the playlist
        @param filter The selection criteria.
        @return The playlist created.
        @throw CommandError if a request is invalid.*/
    PianodPlaylist *Source::createPlaylist (const char *name, const Filter &filter) {
        (void) name;
        (void) filter;
        throw CommandError (E_NOT_IMPLEMENTED);
    };

    /** Retrieve a playlist by name.
        @param name The name of the playlist to retrieve.
        @return The playlist found, or a nullptr if not found. */
    PianodPlaylist *Source::getPlaylistByName (const char *name) {
        assert (name);
        auto list = getPlaylists();
        for (PianodPlaylist *item : getPlaylists()) {
            if (*item == name) {
                return item;
            }
        }
        return nullptr;
    }

    /** Create a temporary playlist based on a filter.
        @param criteria A filter specifying the song selection algorithm.
        @return A temporary playlist, or nullptr if not viable for some reaon.
        @throw E_MEDIA_ACTION if a non-request source. */
    PianodPlaylist *Source::getTransientPlaylist (const Filter &criteria) {
        (void) criteria;
        assert (!"Tried to get transient playlist for non-request source.");
        throw CommandError (E_MEDIA_ACTION);
    }

    /** Get a thing as a suggestion of a specific type.
        @param thing The thing to transform.
        @param type The type of suggestion to retrieve.
        @param where Where to search:
        - KNOWN searches cache/local indexes only
        - SHALLOW will also query music services.
        - Other SearchRange values are invalid for this method.
        @param fully_confirm If false, album and title are enough for a
        song match.  If true, either album name or duration must also match.
        @return A single thingie with a matching description.
        @throw CommandError if not found or ambiguous results. */
    MusicThingie *Source::getSuggestion (MusicThingie *thing,
                                         MusicThingie::Type type,
                                         SearchRange where,
                                         bool fully_confirm) {
        assert (thing);
        assert (MusicThingie::isPrimary (type));
        assert (where == SearchRange::KNOWN || where == SearchRange::SHALLOW);
        if (thing->primaryType() == MusicThingie::Type::Playlist)
            return nullptr;
        if (thing->source() == this && thing->isSuggestion() && thing->primaryType() == type)
            return thing;
        // Use a filter to find a similar object.  Start with broadest search.
        Filter::DuplicationFlags fields (true);
        fields [Filter::Field::Title] = (type == MusicThingie::Type::Song);
        fields [Filter::Field::Album] = (type == MusicThingie::Type::Album);
        fields [Filter::Field::Duration] = false;
        fields [Filter::Field::Track] = false;

        ThingieList matches;
        try {
            matches = getSuggestions (Filter (thing, type, fields), where);
        } catch (const std::invalid_argument &) {
            throw CommandError (E_INVALID, "Criteria too vague");
        }
        if (matches.empty())
            throw CommandError (E_NOTFOUND);
        if (!fully_confirm && matches.size() == 1) {
            return matches.front();
        }

        // Confirm/refine the search results.
        // Use flags as a bitfield and try different permutations of fields,
        // until we find a confirming match.
        for (int flags = (matches.size() == 1 ? 4 : 7); flags >= 0; flags--) {
            if (fully_confirm && flags == 1)
                break;
            fields [Filter::Field::Track] = (flags & 0x01) != 0;
            fields [Filter::Field::Duration] = (flags & 0x02) != 0;
            fields [Filter::Field::Album] = (flags & 0x04) != 0;

            try {
                Filter filter (thing, type, fields);
                ThingieList refinement;
                for (auto item : matches) {
                    if (item->matches (filter)) {
                        // If match quality is good enough, ignore ambiguity.
                        if (flags >= 5)
                            return item;
                        refinement.push_back (item);
                    }
                }
                if (refinement.size() == 1) {
                    return refinement.front();
                }
            } catch (std::invalid_argument &) {
                continue;
            }
        }
        throw CommandError (E_AMBIGUOUS);
    }

    /** Do intermittent or background tasks.
        @return Suggested time before periodic() be invoked again.
        No guarantees are offered about actual behavior. */
    float Source::periodic (void) {
        return A_LONG_TIME;
    }
}  // namespace Media
