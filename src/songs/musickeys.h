///
/// Keys for music library or other persistence needs.
/// @file       musickeys.h - pianod
/// @author     Perette Barella
/// @date       2020-04-15
/// @copyright  Copyright 2020-2021 Devious Fish.  All rights reserved.
///

/// Keys for persisting library indexes.
namespace Music {
    namespace Key {
        // These are for JSON transmission only
        extern const char *PrimaryId;
        extern const char *PrimaryName;

        // Ratings stuff
        extern const char *SongRatings;
        extern const char *PlaylistRatings;
        extern const char *RatingNumeric;
        extern const char *RatingName;

        // Capabilities
        extern const char *Actions;
        extern const char *CanRate;
        extern const char *CanRequest;
        extern const char *ArtistSeed;
        extern const char *AlbumSeed;
        extern const char *SongSeed;

        // These are used by JSON transmission and datastores.
        extern const char *PlaylistName;
        extern const char *PlaylistId;
        extern const char *PlaylistEnabled;
        extern const char *PlaylistSelector;
        extern const char *PlaylistSeeds;
        extern const char *PlaylistGenre;

        extern const char *MediaArtists;
        extern const char *MediaPath;
        extern const char *MediaLastScan;

        extern const char *ArtistName;
        extern const char *ArtistId;
        extern const char *ArtistAlbums;
        extern const char *ArtistPath;

        extern const char *AlbumName;
        extern const char *AlbumId;
        extern const char *AlbumIsCompilation;
        extern const char *AlbumSongs;
        extern const char *AlbumPath;
        extern const char *AlbumArtUrl;

        extern const char *SongName;
        extern const char *SongId;
        extern const char *SongTrackNumber;
        extern const char *SongDuration;
        extern const char *SongYear;
        extern const char *SongLastPlayed;
        extern const char *SongGenre;
        extern const char *SongPath;
        extern const char *SongInfoUrl;
        
        // These are used only for datastores
        extern const char *LibraryMedia;
        extern const char *LibraryPlaylists;

        namespace Type {
            extern const char *Playlist;
            extern const char *Artist;
            extern const char *Album;
            extern const char *Song;
            
            extern const char *PlaylistSuggestion;
            extern const char *ArtistSuggestion;
            extern const char *AlbumSuggestion;
            extern const char *SongSuggestion;
            
            extern const char *PlaylistSeed;
            extern const char *ArtistSeed;
            extern const char *AlbumSeed;
            extern const char *SongSeed;
            
            extern const char *SongRating;
        }

    }  // namespace Key
}  // namespace Music
