#import "AHGameViewController.h"
#import "AHAudio.h"
#include "AHCore.hpp"
#include "AHRenderer.hpp"
#import <GLKit/GLKit.h>
#import <QuartzCore/QuartzCore.h>
#import <AudioToolbox/AudioToolbox.h>
#include <ctime>
#include <stdexcept>

#ifndef AH_ENABLE_VIBRATION
#define AH_ENABLE_VIBRATION 1
#endif

@interface AHGameViewController (Input)
- (void)beginTouches:(NSSet*)touches;
- (void)moveTouches:(NSSet*)touches;
- (void)endTouches:(NSSet*)touches cancelled:(BOOL)cancelled;
@end
@interface AHGLView : GLKView
@property(nonatomic,weak) AHGameViewController* inputOwner;
@end
@implementation AHGLView
- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event { [self.inputOwner beginTouches:touches]; }
- (void)touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event { [self.inputOwner moveTouches:touches]; }
- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event { [self.inputOwner endTouches:touches cancelled:NO]; }
- (void)touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event { [self.inputOwner endTouches:touches cancelled:YES]; }
@end
// CADisplayLink retains its target. A weak proxy lets a removed controller deallocate.
@interface AHDisplayProxy : NSObject
@property(nonatomic,weak) AHGameViewController* owner;
- (void)frame:(CADisplayLink*)link;
@end
@interface AHGameViewController () <GLKViewDelegate>
- (void)frame:(CADisplayLink*)link;
- (void)showError:(NSString*)message;
- (UIImage*)rotationImage;
- (void)cancelInput;
- (void)rotateGame;
- (void)willResignActive:(NSNotification*)notification;
- (void)didBecomeActive:(NSNotification*)notification;
@end
@implementation AHDisplayProxy
- (void)frame:(CADisplayLink*)link { [self.owner frame:link]; }
@end

@implementation AHGameViewController {
    EAGLContext* _context;
    AHGLView* _gl;
    AHAudio* _audio;
    CADisplayLink* _link;
    AHDisplayProxy* _proxy;
    UIButton* _rotate;
    UITouch* _ownerTouch;
    std::unique_ptr<ah::Game> _game;
    std::unique_ptr<ah::Renderer> _renderer;
    ah::Viewport _viewport;
    CFTimeInterval _last;
    BOOL _active,_ready,_manualRotated,_purgeNeeded,_screenVisible,_needsGLFinish;
    CFTimeInterval _lastVibration;
    BOOL _previousIdleDisabled;
    CGSize _layoutSize;
    BOOL _layoutRotated,_hasLayout;
}
- (void)loadView {
    self.view=[[UIView alloc] initWithFrame:[UIScreen mainScreen].bounds];
    self.view.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    self.view.backgroundColor=[UIColor colorWithRed:245/255.f green:132/255.f blue:132/255.f alpha:1];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(willResignActive:) name:UIApplicationWillResignActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didBecomeActive:) name:UIApplicationDidBecomeActiveNotification object:nil];
    if ([UIApplication sharedApplication].applicationState==UIApplicationStateBackground) {
        [self showError:@"Initialize the game view in the foreground."];return;
    }
    _context=[[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    if (!_context) { [self showError:@"This device cannot create an OpenGL ES 2 context."];return; }
    _gl=[[AHGLView alloc] initWithFrame:self.view.bounds context:_context];
    _gl.delegate=self;_gl.inputOwner=self;_gl.enableSetNeedsDisplay=NO;_gl.multipleTouchEnabled=YES;
    _gl.drawableColorFormat=GLKViewDrawableColorFormatRGBA8888;
    _gl.drawableDepthFormat=GLKViewDrawableDepthFormatNone;_gl.drawableStencilFormat=GLKViewDrawableStencilFormatNone;
    _gl.drawableMultisample=GLKViewDrawableMultisampleNone;
    _gl.contentScaleFactor=[UIScreen mainScreen].scale;
    [self.view addSubview:_gl];
    NSString* root=[[NSBundle mainBundle] pathForResource:@"AhogeAssets" ofType:nil];
    if (!root) { [self showError:@"Add Resources/AhogeAssets as a blue folder reference with target membership enabled."];return; }
    NSError* error=nil;_audio=[[AHAudio alloc] initWithAssetRoot:root error:&error];
    if (!_audio) { [self showError:[error localizedDescription]];return; }
    __weak AHGameViewController* weakSelf=self;
    ah::Hooks hooks;
    hooks.sound=[weakSelf](int clip,float db,float pan,bool loop) {
        AHGameViewController* strong=weakSelf;if (strong) [strong->_audio playClip:clip decibels:db pan:pan loop:loop];
    };
    hooks.vibration=[weakSelf](bool reward) {
        AHGameViewController* strong=weakSelf;if (!strong) return;
        if (!reward) return;   // 只保留里程碑 也就是只保留彩蛋的震动
        if (AH_ENABLE_VIBRATION && [UIDevice currentDevice].userInterfaceIdiom==UIUserInterfaceIdiomPhone) {
            CFTimeInterval now=CACurrentMediaTime();
            if (reward || now-strong->_lastVibration>.15) {AudioServicesPlaySystemSound(kSystemSoundID_Vibrate);strong->_lastVibration=now;}
        }
    };
    try {
        if (![EAGLContext setCurrentContext:_context]) throw std::runtime_error("Unable to make ES2 context current");
        _needsGLFinish=YES;
        _game.reset(new ah::Game(hooks,uint32_t(std::time(NULL))));
        _renderer.reset(new ah::Renderer(_game->data,[root UTF8String]));
    } catch (const std::exception& ex) { [self showError:[NSString stringWithUTF8String:ex.what()]];return; }
    _rotate=[UIButton buttonWithType:UIButtonTypeCustom];
    [_rotate setImage:[self rotationImage] forState:UIControlStateNormal];_rotate.alpha=.55;
    _rotate.accessibilityLabel=@"Rotate screen";
    [_rotate addTarget:self action:@selector(rotateGame) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:_rotate];
    _proxy=[AHDisplayProxy new];_proxy.owner=self;
    _link=[CADisplayLink displayLinkWithTarget:_proxy selector:@selector(frame:)];_link.frameInterval=1;_link.paused=YES;
    [_link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    _ready=YES;
}
- (void)showError:(NSString*)message {
    _link.paused=YES;[_audio suspend];
    UILabel* label=[[UILabel alloc] initWithFrame:CGRectInset(self.view.bounds,20,20)];
    label.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    label.numberOfLines=0;label.textAlignment=NSTextAlignmentCenter;label.text=message;
    label.font=[UIFont systemFontOfSize:16];label.textColor=[UIColor blackColor];label.backgroundColor=[UIColor clearColor];
    [self.view addSubview:label];NSLog(@"Ahoge: %@",message);
}
- (UIImage*)rotationImage {
    CGSize size=CGSizeMake(40,40);UIGraphicsBeginImageContextWithOptions(size,NO,0);
    CGContextRef c=UIGraphicsGetCurrentContext();
    CGContextSetRGBFillColor(c,0,0,0,.4);CGContextFillEllipseInRect(c,CGRectMake(0,0,40,40));
    CGContextSetRGBStrokeColor(c,1,1,1,1);CGContextSetLineWidth(c,3.2);CGContextSetLineCap(c,kCGLineCapRound);
    CGContextAddArc(c,20,20,10,40*ah::PI/180,320*ah::PI/180,0);CGContextStrokePath(c);
    float x=20+10*std::cos(320*ah::PI/180),y=20+10*std::sin(320*ah::PI/180);
    CGContextSetRGBFillColor(c,1,1,1,1);CGContextMoveToPoint(c,x,y);
    CGContextAddLineToPoint(c,x-6.4,y-.8);CGContextAddLineToPoint(c,x-.8,y-6.4);CGContextClosePath(c);CGContextFillPath(c);
    UIImage* result=UIGraphicsGetImageFromCurrentImageContext();UIGraphicsEndImageContext();return result;
}
- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];_screenVisible=YES;_previousIdleDisabled=[UIApplication sharedApplication].idleTimerDisabled;
    [self resumeGame];
}
- (void)viewWillDisappear:(BOOL)animated {
    [self prepareForBackground];_screenVisible=NO;
    [UIApplication sharedApplication].idleTimerDisabled=_previousIdleDisabled;
    [super viewWillDisappear:animated];
}
- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    CGSize size=self.view.bounds.size;
    if (_hasLayout && CGSizeEqualToSize(size,_layoutSize) && _layoutRotated==_manualRotated) return;
    [self cancelInput];_layoutSize=size;_layoutRotated=_manualRotated;_hasLayout=YES;
    _gl.transform=CGAffineTransformIdentity;
    _gl.bounds=CGRectMake(0,0,_manualRotated ? size.height:size.width,_manualRotated ? size.width:size.height);
    _gl.center=CGPointMake(size.width/2,size.height/2);
    if (_manualRotated) _gl.transform=CGAffineTransformMakeRotation(ah::PI/2);
    _viewport.resize(float(_gl.bounds.size.width),float(_gl.bounds.size.height));
    BOOL compact=MIN(size.width,size.height)<=320;
    CGFloat button=compact ? 40:48,margin=compact ? 8:12;
    _rotate.frame=CGRectMake(size.width-button-margin,size.height-button-margin,button,button);
}
- (void)rotateGame {
    if (!_active) return;
    [_audio userDidInteract];[self cancelInput];_manualRotated=!_manualRotated;
    [self.view setNeedsLayout];[self.view layoutIfNeeded];
}
- (BOOL)shouldAutorotate { return YES; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskAll; }
- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation { return UIInterfaceOrientationPortrait; }
- (BOOL)prefersStatusBarHidden { return YES; }
- (void)cancelInput { _ownerTouch=nil;if (_game) _game->cancelTouch(); }
- (void)beginTouches:(NSSet*)touches {
    if (!_active || !_game || _ownerTouch) return;
    [_audio userDidInteract];
    UITouch* first=nil;
    for (UITouch* touch in touches) if (!first || touch.timestamp<first.timestamp) first=touch;
    _ownerTouch=first;if (!first) return;
    CGPoint p=[first locationInView:_gl];ah::Point q=_viewport.logical(p.x,p.y);
    float radius=_viewport.fingerRadius();
    if ([first respondsToSelector:@selector(type)] && first.type==UITouchTypeStylus) radius=0;
    _game->touchDown(q.x,q.y,radius);
}
- (void)moveTouches:(NSSet*)touches {
    if (!_active || !_ownerTouch || ![touches containsObject:_ownerTouch]) return;
    CGPoint p=[_ownerTouch locationInView:_gl];ah::Point q=_viewport.logical(p.x,p.y);_game->touchMove(q.x,q.y);
}
- (void)endTouches:(NSSet*)touches cancelled:(BOOL)cancelled {
    if (!_ownerTouch || ![touches containsObject:_ownerTouch]) return;
    if (!cancelled) [self moveTouches:touches];[self cancelInput];
}
- (void)prepareForBackground {
    if (!_active && !_needsGLFinish) return;
    _active=NO;_link.paused=YES;_last=0;[self cancelInput];[_audio suspend];
    if (_needsGLFinish && [UIApplication sharedApplication].applicationState!=UIApplicationStateBackground) {
        if ([EAGLContext setCurrentContext:_context]) glFinish();
    }
    _needsGLFinish=NO;
    if (_screenVisible) [UIApplication sharedApplication].idleTimerDisabled=_previousIdleDisabled;
}
- (void)resumeGame {
    if (_active || !_ready || !_screenVisible || [UIApplication sharedApplication].applicationState!=UIApplicationStateActive) return;
    _last=0;_active=YES;_link.paused=NO;[_audio resume];[UIApplication sharedApplication].idleTimerDisabled=YES;
}
- (void)frame:(CADisplayLink*)link {
    if (!_active || !_ready || [UIApplication sharedApplication].applicationState!=UIApplicationStateActive) return;
    float dt=_last>0 ? float(link.timestamp-_last):1.f/60;_last=link.timestamp;
    try { _game->update(dt);[_gl display]; }
    catch (const std::exception& ex) { _ready=NO;[self prepareForBackground];[self showError:[NSString stringWithUTF8String:ex.what()]]; }
}
- (void)willResignActive:(NSNotification*)notification { [self prepareForBackground]; }
- (void)didBecomeActive:(NSNotification*)notification { [self resumeGame]; }
- (void)glkView:(GLKView*)view drawInRect:(CGRect)rect {
    if (!_active || !_renderer || [UIApplication sharedApplication].applicationState!=UIApplicationStateActive) return;
    try {
        _needsGLFinish=YES;
        if (_purgeNeeded) {_renderer->purgeTextures();_purgeNeeded=NO;}
        _renderer->draw(*_game,int(view.drawableWidth),int(view.drawableHeight));
    } catch (const std::exception& ex) { _ready=NO;[self prepareForBackground];[self showError:[NSString stringWithUTF8String:ex.what()]]; }
}
- (void)didReceiveMemoryWarning { [super didReceiveMemoryWarning];_purgeNeeded=YES; }
- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];[_link invalidate];_proxy.owner=nil;[_audio suspend];
    if ([UIApplication sharedApplication].applicationState!=UIApplicationStateBackground) {
        [EAGLContext setCurrentContext:_context];_renderer.reset();[EAGLContext setCurrentContext:nil];
    } else {
        if (_renderer) _renderer->abandonGL();_renderer.reset();
    }
    _game.reset();
}
@end
