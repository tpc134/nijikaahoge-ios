#import <Foundation/Foundation.h>
@interface AHAudio : NSObject
- (instancetype)initWithAssetRoot:(NSString*)root error:(NSError**)error;
- (void)playClip:(int)clip decibels:(float)db pan:(float)pan loop:(BOOL)loop;
- (void)suspend;
- (void)resume;
- (void)userDidInteract;
@end
