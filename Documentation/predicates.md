Predicates [predicates]
----------
Several commands use a standard predicate form:

	[manner] [type] <ID | NAME | LIKE | WHERE> {specifier}
	[manner] [type] SOURCE ID {id} <ID | NAME | LIKE | WHERE> {specifier}
	[manner] [type] SOURCE TYPE {type} NAME {name} <ID | NAME | LIKE | WHERE> {specifier}


Where 

	manner :== [AUTHORITATIVE | DISCRETIONARY]

	type :== [ANY | ARTIST | ALBUM | SONG | PLAYLIST | GENRE]

Predicates accept item lists (`ID`, `NAME`) or are patterns that may match multiple items (`LIKE`, `WHERE`).

`type` does not apply if type is implied by the command.  At present, it only applies to LIKE; if omitted when accepted, it is equivalent to ANY.

The predicate types are as follow:

`ID`
: The predicate is a list of IDs of a song, album, artist or playlist.

`NAME`
: The predicate is a list of names of a song, album, artist or playlist (as appropriate for the command).

`LIKE`
: The predicate is a list of phrases.  Each is split into words, and matches songs, albums, artists or playlists (as appropriate for the command or as specified by `type`) which contain all the words in the predicate.

`WHERE`
: The predicate is an expression which matches songs, albums, artists or playlists (as appropriate for the command) in accordance with the expression.  See the filter grammar for details.

`SOURCE`
: Allows a specific source to be specified for fulfilling a predicate.

`manner` has meaning when interpreting predicates against the media manager source.

`AUTHORITATIVE`
: Require all sources execute the query successfully.  If any source cannot perform a query because the query exceeds its ability, the command fails.

`DISCRETIONARY`
: Search sources that are capable of the query.  If a source can’t perform a query, that source is ignored and results gathered from other sources.

If a source encounters an error (as opposed to a limitation), a discretionary search will still fail.

### Infix vs. Suffix
Predicates may occur infix (in the middle of a command line) or suffix (at the end).  Suffix predicates allow multiple parameters.  Infix predicates, however, accept only one parameter as their parameter.

Furthermore, filter expressions (predicate form `WHERE`) are outside the standard Football/pianod command line parsing.  The usual word boundary and quoting rules are not applicable, preventing WHERE predicates from infix use entirely.

### Playlists vs. Others
* Playlist predicates NAME, LIKE, and WHERE search playlist names only.  Playlist ID predicates can accept playlist IDs or song IDs.  A song’s playlist will be used if a song is given; it is an error if the song does not have a playlist.
* Other predicates search albums, artists, songs and genres/playlists.

### Efficiency
IDs are encoded with source, type, and a unique item identifier.  Items can be retrieved by ID more quickly than via other methods; a decoded ID is routed to the correct source, which in turn probably uses hash tables.  This gain *only* applies to ID predicate form (such as `ID "3ss3578329"`); building an expression that uses ID (such as `WHERE ID = "3ss3578329"`) will function but, like most predicates, locate items by exhaustive comparison.  Since the ID is unique, it guarantees the correct item is manipulated, and no side-effects to similar items.

Additionally, ID predicates are validated to ensure each item exists.  With other predicate forms, lack of a match is not an error; for IDs, it is an error if a corresponding item is not found.  If multiple IDs were specified, none are processed.

Thus, `ID` predicates are the “gold standard,” and should be used by client implementations whenever possible.

### Filter Mechanism
Filter expressions are used with the `WHERE` predicate form.

#### History & Thanks
I originally developed the filter in late 2005 as a [replacement for the mserv filter] mechanism.  It was backward-compatible with the original mserv filter, with new features and a performance boost.  `pianod2` retains a similar syntax but includes various enhancements.

My thanks to Kimmo Suominen for documenting the [original mserv filter syntax].

This document describes the `pianod2` filter syntax.

[replacement for the mserv filter]: http://sourceforge.net/p/mserv/mailman/mserv-dev/?viewmonth=200512
[original mserv filter syntax]: http://kimmo.suominen.com/docs/mserv-filters/

#### Overview

- The filter parses the command line once to build a parse tree that is utilized to evaluating each song, artist, album, or playlist.
- When matching, a filter uses short-circuit logic to improve performance.
- Operators include substring searches, exact matches, and regular expressions and greater/less than on several fields.
-  There is a “search” field which matches on both artist, title, and album name fields; for playlists, the playlist name is matched on.
- When comparing, “A”, “An” and ”The " are skipped at the start of songs, so “request where author=beatles" will match both "Beatles" and "The Beatles".  A '*' at the end of a string (but not the middle) acts as a wildcard.
- Filter expressions may be arbitrarily long and complex.
- Whitespace is allowed, allowing expressions to be more legible.

#### Operators and Precedence

- () (highest)
- comparison operators: = == != < > <= >= =~ “quoted text”
- ! (negation)
- | or || (binary or)
- & or && (binary and) (lowest)

Evaluation is left to right.

Operator =~
: Performs substring search operator (strings fields only).

Operator = and Operator ==
: Both = and == are equivalence operators.

Operator !=
: Inequivalence operator.

Operator <, >, <= and >=
: Perform strings or numeric comparison appropriate to the data field.

Operator :
: *Not implemented yet.*  Perform regular expression matching.

“Quoted text”
: Equivalent to SEARCH =~ “the text enclosed”.  Use either single quotes (apostrophes) or double quotes.

The RHS of comparisons can be quoted.  If the next non-whitespace character after the operator is a single or double quote, then the string is collected up to the close quote, which must be the same type as the open quote.  The quote character may be inserted into the string by doubling it in place. 

Ratings:
superb, good, neutral, bad, awful, several other adjectives or a number between 0.5 and 5.0.

#### Comparisons/Keywords
##### Binary
HEARD (songs)
: True if the song has ever been played.  Not all sources persist this information.

RATED (songs, playlists)
: True if the song or playlist has been rated by any user, logged in or not.

PLAYED
: True if the song has ever been played.  Equivalent to LASTPLAY > 0.

COMPILATION
: True if an album is a compilation or a song is from a compilation album.

FALSE
: False is always false.  Useful in creating expressions that return the empty set.

##### String
ID = {id}
: True if the ID matches.  No wildcards.

TYPE = type
: Match only the specified type, which is one of: ARTIST or AUTHOR, ALBUM or ALBUMNAME, TRACK or TITLE or SONG.

ALBUM {cmp} pattern (also ALBUMNAME; applies to albums, tracks)
: Compare album name.  False for artists or playlists.

ARTIST {cmp} pattern (also AUTHOR; albums, songs)
: Compare artist name.  False for playlists.

TITLE {cmp} pattern (also SONG; tracks, playlists)
: Compare song/track title.  False for artists, songs or playlists.

PLAYLIST {cmp} pattern
: Compare playlist name.  False for artists and albums.

NAME {cmp} pattern
: Compare primary field.  For songs, compares titles; for albums, album name; for artists, the artist name; for playlists, the playlist name.

SEARCH [ = =~ ] pattern (all)
: Compare the various components to the pattern.  {cmp} can be any of the comparison operators.  SEARCH checks artist name, album names, track title and and playlist name, and can not be used with less than/greater than.

GENRE [ = =~ ] {genre} (tracks, playlists)
: True if a song or playlist belongs to `genre`.  No wildcards, but =~ does a substring match rather than matching genre name - so for example, you could use GENRE =~ "ROCK/POP".  When using operator =, genres may be separated by commas, slashes and plus (common on FreeDb and CDDB).  

{user} {cmp} {rating} (songs, playlists)
: Compare the user’s rating for a song or playlist.

{user} = RATED
: True if specified user has rated the item.

##### Numeric
RATING {cmp} number
: Compare the average rating of a song or playlist.  

DURATION {cmp} number
: Compare duration of song, in seconds.

LASTPLAY {cmp} number
: Compare last played time, in hours.

TRACK {cmp} number
: Compare track number.

YEAR {cmp} number
: Compare year.

##### Unknown values vs. Nonexistent fields
When values are _unknown_, the comparison returns false.  For example, `year > 1955` returns one set of songs, `year <= 1955` returns a second, but there is a third set of songs without any assigned year.  These can be found with some finagling, such as !(year >= 1).

If a value _does not exist_, it is always false, even with preceding negation.  For example a search for `year > 1955 || year <= 1955 || !year >= 1` *on an artist* is false, because artists do not have years.  However, a filter for `year > 1955 | “Madonna”` could be true, because the non sequitur portion of the expression does not preclude the rest being potentially true.

