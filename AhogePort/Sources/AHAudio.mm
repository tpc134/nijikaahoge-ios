#import "AHAudio.h"
#include "AHAudioMixer.hpp"
#include "AHAudioState.hpp"
#include "AHWave.hpp"
#import <AudioToolbox/AudioToolbox.h>
#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>
#import <dispatch/dispatch.h>
#include <cstring>
#include <memory>

static OSStatus AHRender(void* ref,AudioUnitRenderActionFlags* flags,const AudioTimeStamp* time,
                         UInt32 bus,UInt32 frames,AudioBufferList* buffers) {
    ah::AudioMixer* mixer=static_cast<ah::AudioMixer*>(ref);
    if (!buffers) return kAudio_ParamError;
    if (buffers->mNumberBuffers!=1 || !buffers->mBuffers[0].mData || buffers->mBuffers[0].mDataByteSize/4<frames) {
        for (UInt32 i=0;i<buffers->mNumberBuffers;++i) if (buffers->mBuffers[i].mData)
            std::memset(buffers->mBuffers[i].mData,0,buffers->mBuffers[i].mDataByteSize);
        if (flags) *flags|=kAudioUnitRenderAction_OutputIsSilence;
        return noErr;
    }
    // RemoteIO converts our fixed 48kHz client stream to the hardware rate.
    // No fixed-size scratch buffers: any negotiated slice size can be rendered.
    mixer->render((int16_t*)buffers->mBuffers[0].mData,frames);return noErr;
}
static OSStatus AHCreateOutput(ah::AudioMixer* mixer,AudioUnit* unit) {
    *unit=NULL;
    AudioComponentDescription d={0};d.componentType=kAudioUnitType_Output;d.componentSubType=kAudioUnitSubType_RemoteIO;d.componentManufacturer=kAudioUnitManufacturer_Apple;
    AudioComponent component=AudioComponentFindNext(NULL,&d);
    OSStatus status=component ? AudioComponentInstanceNew(component,unit):-1;
    AudioStreamBasicDescription format={0};format.mSampleRate=48000;format.mFormatID=kAudioFormatLinearPCM;
    format.mFormatFlags=kAudioFormatFlagIsSignedInteger|kAudioFormatFlagIsPacked;
    format.mBytesPerPacket=4;format.mFramesPerPacket=1;format.mBytesPerFrame=4;format.mChannelsPerFrame=2;format.mBitsPerChannel=16;
    if (!status) status=AudioUnitSetProperty(*unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input,0,&format,sizeof(format));
    AURenderCallbackStruct callback={AHRender,mixer};
    if (!status) status=AudioUnitSetProperty(*unit,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input,0,&callback,sizeof(callback));
    if (!status) status=AudioUnitInitialize(*unit);
    if (status && *unit) {AudioComponentInstanceDispose(*unit);*unit=NULL;}
    return status;
}
@interface AHAudio ()
- (void)startIfAllowed;
- (void)stopOutput;
- (void)interruption:(NSNotification*)notification;
- (void)mediaReset:(NSNotification*)notification;
@end
@implementation AHAudio {
    std::unique_ptr<ah::AudioMixer> _mixer;
    ah::AudioState _state;
    AudioUnit _unit;
    BOOL _running,_sessionActive;
}
- (instancetype)initWithAssetRoot:(NSString*)root error:(NSError**)error {
    self=[super init];if (!self) return nil;
    _mixer.reset(new ah::AudioMixer());
    for (int i=0;i<12;++i) {
        @autoreleasepool {
            NSData* data=[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:[NSString stringWithFormat:@"media/%d.wav",i]]];
            std::vector<int16_t> pcm;
            if (!ah::loadWave((const uint8_t*)data.bytes,data.length,pcm)) {
                if (error) *error=[NSError errorWithDomain:@"AhogeAudio" code:1 userInfo:@{NSLocalizedDescriptionKey:@"Missing or invalid PCM audio resource"}];
                return nil;
            }
            _mixer->setClip(i,std::move(pcm));
        }
    }
    // Do not activate the session during launch, an interruption, or background.
    AVAudioSession* session=[AVAudioSession sharedInstance];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(interruption:) name:AVAudioSessionInterruptionNotification object:session];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(mediaReset:) name:AVAudioSessionMediaServicesWereResetNotification object:session];
    return self;
}
- (void)playClip:(int)clip decibels:(float)db pan:(float)pan loop:(BOOL)loop {
    if (_running) _mixer->play(clip,db,pan,loop);
}
- (void)stopOutput {
    if (_running) {AudioOutputUnitStop(_unit);_running=NO;}
    if (_mixer) _mixer->stop();
}
- (void)suspend {
    _state.suspend();[self stopOutput];
    if (_sessionActive) {
        [[AVAudioSession sharedInstance] setActive:NO withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation error:NULL];
        _sessionActive=NO;
    }
}
- (void)resume { _state.resume();[self startIfAllowed]; }
- (void)userDidInteract {
    if (!_state.wanted || [UIApplication sharedApplication].applicationState!=UIApplicationStateActive) return;
    _state.userAction();[self startIfAllowed];
}
- (void)startIfAllowed {
    if (_running || !_state.canStart([UIApplication sharedApplication].applicationState==UIApplicationStateActive)) return;
    AVAudioSession* session=[AVAudioSession sharedInstance];NSError* error=nil;
    if (![session setCategory:AVAudioSessionCategoryAmbient error:&error]) {NSLog(@"Ahoge audio category: %@",error);return;}
    // Preferences are requests; Ambient sessions may retain 44.1kHz hardware.
    [session setPreferredSampleRate:48000 error:NULL];
    [session setPreferredIOBufferDuration:.01 error:NULL];
    if (![session setActive:YES error:&error]) {NSLog(@"Ahoge audio activation: %@",error);return;}
    _sessionActive=YES;
    OSStatus status=_unit ? noErr:AHCreateOutput(_mixer.get(),&_unit);
    if (!status) status=AudioOutputUnitStart(_unit);
    _running=status==noErr;
    if (status) {
        NSLog(@"Ahoge RemoteIO initialization/start failed: %d (game continues; next touch retries)",(int)status);
        [session setActive:NO withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation error:NULL];
        _sessionActive=NO;
    }
}
- (void)interruption:(NSNotification*)notification {
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{[self interruption:notification];});return;
    }
    NSNumber* type=notification.userInfo[AVAudioSessionInterruptionTypeKey];
    if (!type) return;
    if (type.unsignedIntegerValue==AVAudioSessionInterruptionTypeBegan) {
        _state.interruptionBegan();[self stopOutput];_sessionActive=NO;
    } else if (type.unsignedIntegerValue==AVAudioSessionInterruptionTypeEnded) {
        NSUInteger options=[notification.userInfo[AVAudioSessionInterruptionOptionKey] unsignedIntegerValue];
        _state.interruptionEnded((options & AVAudioSessionInterruptionOptionShouldResume)!=0);
        [self startIfAllowed];
    }
}
- (void)mediaReset:(NSNotification*)notification {
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{[self mediaReset:notification];});return;
    }
    if (_unit) {AudioOutputUnitStop(_unit);AudioUnitUninitialize(_unit);AudioComponentInstanceDispose(_unit);_unit=NULL;}
    _running=NO;_sessionActive=NO;_state.mediaReset();_mixer->stop();
    // Apple QA1749: rebuild and reactivate only following a user action.
    // startIfAllowed configures session properties and recreates RemoteIO then.
}
- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [self suspend];
    if (_unit) {AudioUnitUninitialize(_unit);AudioComponentInstanceDispose(_unit);_unit=NULL;}
}
@end
