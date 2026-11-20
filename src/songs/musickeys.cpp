///
/// Keys for music library or other persistence needs.
/// @file       musickeys.cpp - pianod
/// @author     Perette Barella
/// @date       2020-04-15
/// @copyright  Copyright 2020-2021 Devious Fish.  All rights reserved.
///

namespace Music {
    namespace Key {
        // These are for JSON transmission only
        const char *PrimaryId = "id";
        const char *PrimaryName = "name";

        // Ratings stuff
        const char *SongRatings = "songRating";
        const char *PlaylistRatings = "playlistRating";
        const char *RatingNumeric = "rating";
        const char *RatingName = "ratingText";

        // Capabilities
        const char *Actions = "actions";
        const char *CanRate = "rate";
        const char *CanRequest = "request";
        const char *ArtistSeed = "artistSeed";
        const char *AlbumSeed = "albumSeed";
        const char *SongSeed = "seed";

        const char *LibraryMedia = "media";
        const char *LibraryPlaylists = "playlists";

        const char *PlaylistName = "playlistName";
        const char *PlaylistId = "playlistId";
        const char *PlaylistEnabled = "enabled";
        const char *PlaylistSelector = "selector";
        const char *PlaylistSeeds = "seeds";
        const char *PlaylistGenre = "genre";

        const char *MediaArtists = "artists";
        const char *MediaPath = "mediaPath";
        const char *MediaLastScan = "lastNewMediaScan";

        const char *ArtistName = "artistName";
        const char *ArtistId = "artistId";
        const char *ArtistAlbums = "albums";
        const char *ArtistPath = "artistPath";

        const char *AlbumName = "albumName";
        const char *AlbumId = "albumId";
        const char *AlbumIsCompilation = "compilation";
        const char *AlbumSongs = "tracks";
        const char *AlbumPath = "albumPath";
        const char *AlbumArtUrl = "albumArtUrl";

        const char *SongName = "trackName";
        const char *SongId = "trackId";
        const char *SongTrackNumber = "trackNumber";
        const char *SongDuration = "duration";
        const char *SongYear = "year";
        const char *SongLastPlayed = "lastPlayed";
        const char *SongGenre = "genre";
        const char *SongPath = "trackPath";
        const char *SongInfoUrl = "songInfoUrl";

        namespace Type {
            const char *Playlist = "playlist";
            const char *Artist = "artist";
            const char *Album = "album";
            const char *Song = "song";
            
            const char *PlaylistSuggestion = "playlistSuggestion";
            const char *ArtistSuggestion = "artistSuggestion";
            const char *AlbumSuggestion = "albumSuggestion";
            const char *SongSuggestion = "songSuggestion";
            
            const char *PlaylistSeed = "playlistSeed";
            const char *ArtistSeed = "artistSeed";
            const char *AlbumSeed = "albumSeed";
            const char *SongSeed = "songSeed";
            
            const char *SongRating = "songRating";
        }
    }  // namespace Key
}  // namespace Music

