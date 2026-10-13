#import <UIKit/UIKit.h>

@interface AHGameViewController : UIViewController
// Call from the corresponding AppDelegate methods; also observed automatically.
- (void)prepareForBackground;
- (void)resumeGame;
@end
