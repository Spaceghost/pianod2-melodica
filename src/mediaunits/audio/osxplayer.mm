///
/// OS X audio player.
/// Play media files on OS X using AVFoundation framework.
/// @file       osxplayer.mm - pianod2
/// @author     Perette Barella
/// @date       2015-11-18
/// @copyright  Copyright (c) 2015-2023 Devious Fish. All rights reserved.
///

#include <config.h>

#include <stdio.h>

#include <string>
#include <thread>

#include "fundamentals.h"
#include "logging.h"
#include "mediaplayer.h"
#include "audiooutput.h"

#include "osxplayer.h"

#import <AppKit/AppKit.h>
#import <AVFoundation/AVFoundation.h>
#import <Cocoa/Cocoa.h>

/*
 *          Launch NSapplication on the main thread,
 *          with pianod behind the scenes.
 */

static std::thread *thread = nullptr;
static void (*real_app) (void);

@interface PianodAppDelegate : NSObject <NSApplicationDelegate>
- (void)pianodRequestTerminate;
- (void)applicationDidFinishLaunching:(NSNotification *) __unused notification;
@end


@implementation PianodAppDelegate
- (void)pianodRequestTerminate {
    [[NSApplication sharedApplication] terminate: [NSApplication sharedApplication]];
}
- (void)applicationDidFinishLaunching:(NSNotification *) __unused notification
{
    // In theory, we should be able to make a thread and std::move it to the static.
    // In practice, this doesn't work on OS X.  Memory seems to get jacked up
    // and Cocoa randomly crashes at some subsequent point.
    thread = new std::thread {[self]() {
        real_app();
        [self performSelectorOnMainThread: @selector(pianodRequestTerminate)
                               withObject: self
                            waitUntilDone: YES ];
    }};
}
@end

/** The AVAudioPlayer works great, until end of song when it won't tell us
    that it's done.  To get that, we need NSApplication and its main run loop
    going, and as a bonus, the fucker asserts if it's not the first thread. */
void wrapInNSApplication (void real_application(void)) {
    real_app = real_application;
    // The first time we ask for it, the sharedApplication singleton gets initialized.
    // Get it once before spawning the thread, so we don't try to make it twice.

    AUTORELEASEPOOL {
        NSApplication *app = [NSApplication sharedApplication];
        PianodAppDelegate *delegate = [[PianodAppDelegate alloc] init];
        [app setDelegate:delegate];
        [app run];
        [app setDelegate:nil];
        [delegate release];
        if (thread) {
            thread->join();
            delete thread;
        }
    }
};

/* End of launch code */



namespace Audio {
    class AVFoundationPlayer;
}

@interface PlayerHelper : NSObject <AVAudioPlayerDelegate>
- (PlayerHelper *)initWithRecipientPlayer:(Audio::AVFoundationPlayer *) cpp_relay;
- (oneway void)release;
- (PlayerHelper *)monitorPlayer:(AVPlayer *) player;
- (PlayerHelper *)monitorPlayerItem:(AVPlayerItem *) item;
- (void)itemDidFinishPlaying:(AVPlayerItem *) player;
- (void) itemDidFailPlayback: (AVPlayerItem *) player;
- (void)observeValueForKeyPath: (NSString *) keyPath
                      ofObject: (id) object
                        change: (NSDictionary *) change
                       context: (void *) context;
@end



namespace Audio {

    /// Base class for OS X's AVFoundation audio player
    class AVFoundationPlayer : public Media::Player {
    private:
        std::mutex avplayer_mutex;
        AVPlayer *audio_player = nil;
        mutable volatile Media::Player::State state = Initializing;
        PlayerHelper *monitor = nullptr;
        float audio_gain;

        // Implement MediaPlayer API
    public:
        virtual void pause (void) override;
        virtual void abort (void) override;
        virtual void cue (void) override;
        virtual void play (void) override;

        virtual void setVolume (float volume) override;
        virtual State currentState (void) const override;
        virtual float trackDuration (void) const override;
        virtual float playPoint (void) const override;
        virtual RESPONSE_CODE completionStatus (void) override;

    public:
        AVFoundationPlayer (const std::string &media_url,
                            float initial_audio_gain = 0);
        virtual ~AVFoundationPlayer (void);
    };

    /** Play a media file or URL using ffmpeg.
     @param media_url The filename or URL of the media.
     @param initial_audio_gain Gain to apply when playing file, in decibels.
     If ReplayGain is encountered during playback, that is preferred
     over this value. */
    AVFoundationPlayer::AVFoundationPlayer (const std::string &media_url,
                                            float initial_audio_gain) : audio_gain (initial_audio_gain) {
        const char *reason = nullptr;

        AUTORELEASEPOOL {
            bool is_network = (strncasecmp (media_url.c_str(), "http", 4) == 0);
            NSString *path = [NSString stringWithCString:media_url.c_str() encoding:NSUTF8StringEncoding ];
            if (!path)
                throw std::bad_alloc();
            NSURL *nsurl = (is_network ? [NSURL URLWithString:path] : [NSURL fileURLWithPath:path]);
            if (!nsurl)
                throw AudioException ("Invalid URL");
            AVPlayerItem *item = [AVPlayerItem playerItemWithURL: nsurl];
            if (!item)
                throw AudioException ("Cannot setup player item");

            monitor = [PlayerHelper alloc];
            if (!monitor)
                throw std::bad_alloc();
            [[monitor initWithRecipientPlayer:this] monitorPlayerItem:item];

            if ((audio_player = [AVPlayer playerWithPlayerItem: item])) {
                if ([audio_player status] != AVPlayerStatusFailed) {
                    [monitor monitorPlayer: audio_player];
                    audio_player.actionAtItemEnd = AVPlayerActionAtItemEndPause;
                    setVolume (0);
                    [audio_player retain];
                    return;
                }
                reason = [[audio_player error].localizedDescription UTF8String];
            }
            if (!reason)
                reason = "AVPlayer failed";
            flog (LOG_WHERE (Log::ERROR), reason);
            [monitor release];
        }
        throw AudioException (reason);
    };

    AVFoundationPlayer::~AVFoundationPlayer (void) {
        AUTORELEASEPOOL {
            [monitor release];
            monitor = nil;
            [audio_player release];
            audio_player = nil;
        }
    }

    void AVFoundationPlayer::setVolume (float volume) {
        AUTORELEASEPOOL {
            const double multiplier = pow (10, (volume + audio_gain) / 20);
            audio_player.volume = (volume > 0 ? 1.0 : multiplier);
        }
    }

    float AVFoundationPlayer::trackDuration (void) const {
        AUTORELEASEPOOL {
            return ([audio_player status] == AVPlayerStatusUnknown ? -1 :
                    CMTimeGetSeconds ([[audio_player currentItem] duration]));
        }
    };

    float AVFoundationPlayer::playPoint (void) const {
        AUTORELEASEPOOL {
            return ([audio_player status] == AVPlayerStatusUnknown ? -1 :
                    CMTimeGetSeconds ([audio_player currentTime]));
        }
    }

    Media::Player::State AVFoundationPlayer::currentState (void) const  {
        return state;
    };

    void AVFoundationPlayer::pause (void) {
        [audio_player pause];
    };

    void AVFoundationPlayer::abort (void) {
        [audio_player pause];
        state = Done;
    }

    void AVFoundationPlayer::cue (void) {
        std::lock_guard<std::mutex> lock (avplayer_mutex);
        if (state < Cueing) {
            if ([audio_player status] == AVPlayerStatusReadyToPlay) {
                [audio_player prerollAtRate:1.5 completionHandler: nil];
                state = Cueing;
            }
        }
    }
    void AVFoundationPlayer::play (void) {
        std::lock_guard<std::mutex> lock (avplayer_mutex);
        if (state < Playing)
            state = Playing;
        [audio_player play];
    }

    RESPONSE_CODE AVFoundationPlayer::completionStatus (void) {
        return ([audio_player status] == AVPlayerStatusFailed ? F_AUDIO_FAILURE : S_OK);
    }

    /** Old-style C interface to provide bridge from C++ to Objective-C compiled code. */
    Media::Player *getAVFoundationPlayer (const AudioSettings &AudioSettings,
                                          const std::string &media_url,
                                          float initial_audio_gain) {
        return new AVFoundationPlayer (media_url, initial_audio_gain);
    };
}



/** Catch notifications that the player is ready or has completed.
    @warning
    None of the PlayerHelper infrastructure works without an NSApplication
    instance running the main run loop; apparently the callbacks get dispatched
    through there.
*/
@implementation PlayerHelper {
    Audio::AVFoundationPlayer *cpp_player;
    AVPlayerItem *play_item;
    AVPlayer *play;
}

- (PlayerHelper *)initWithRecipientPlayer:(Audio::AVFoundationPlayer *) cpp_relay {
    [super init];
    play_item = nil;
    play = nil;
    cpp_player = cpp_relay;
    return self;
}

/** Detach all observers and release resources. */
- (oneway void)release {
    if ([self retainCount] == 1) {
        if (play) {
            [play removeObserver:self forKeyPath:@"status" context:self];
            play = nil;
        }
        if (play_item) {
            [[NSNotificationCenter defaultCenter] removeObserver: self];
            play_item = nil;
        }
    }
    [super release];
}
/** Add the media item to the monitor. */
- (PlayerHelper *)monitorPlayerItem:(AVPlayerItem *)item {
    assert (cpp_player);
    assert (!play_item);
    play_item = item;
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector(itemDidFinishPlaying:)
                                                 name: AVPlayerItemDidPlayToEndTimeNotification
                                               object: play_item];
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector(itemDidFailPlayback:)
                                                 name: AVPlayerItemFailedToPlayToEndTimeNotification
                                               object: play_item];
    return self;
}

/** Callback function for when playback is complete. */
- (void) itemDidFinishPlaying: (AVPlayerItem *) __unused player {
    cpp_player->abort ();
}
/** Callback function for when playback is complete. */
- (void) itemDidFailPlayback: (AVPlayerItem *) __unused player {
    cpp_player->abort ();
}

/** Add the player to the monitor. */
- (PlayerHelper *)monitorPlayer:(AVPlayer *)player {
    assert (cpp_player);
    assert (!play);
    play = player;
    [play addObserver:self forKeyPath:@"status" options:0 context:self];
    return self;
}

/** Callback function for when player is ready. */
-(void)observeValueForKeyPath: (NSString *) __unused keyPath
                     ofObject: (id) __unused object
                       change: (NSDictionary *) __unused change
                      context: (void *) __unused context {
    if ([play status]==AVPlayerStatusReadyToPlay) {
        // It may not be safe here to unregister the observer, as this is not
        // the main thread.
        cpp_player->cue();
    }
}

@end

