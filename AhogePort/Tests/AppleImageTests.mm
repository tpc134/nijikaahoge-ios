#include "AHImagePixels.hpp"
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

static void require(bool condition,const char* message) {if (!condition) throw std::runtime_error(message);}
int main(int argc,char** argv) {
    @autoreleasepool {
        NSString* temporary=[NSTemporaryDirectory() stringByAppendingPathComponent:[[NSProcessInfo processInfo] globallyUniqueString]];
        NSString* file=[temporary stringByAppendingPathExtension:@"png"];
        try {
            // Deliberately asymmetric rows and a partially transparent color.
            const unsigned char pixels[]={255,0,0,255, 0,255,0,255, 0,0,255,255, 255,0,255,128};
            CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
            CGDataProviderRef provider=CGDataProviderCreateWithData(NULL,pixels,sizeof(pixels),NULL);
            CGImageRef image=CGImageCreate(2,2,8,32,8,space,kCGImageAlphaLast|kCGBitmapByteOrder32Big,provider,NULL,false,kCGRenderingIntentDefault);
            CGDataProviderRelease(provider);CGColorSpaceRelease(space);
            require(image!=NULL,"cannot create probe CGImage");
            NSURL* url=[NSURL fileURLWithPath:file];
            CGImageDestinationRef destination=CGImageDestinationCreateWithURL((__bridge CFURLRef)url,CFSTR("public.png"),1,NULL);
            if (!destination) {CGImageRelease(image);throw std::runtime_error("cannot create probe PNG");}
            CGImageDestinationAddImage(destination,image,NULL);
            bool saved=CGImageDestinationFinalize(destination);CFRelease(destination);CGImageRelease(image);
            require(saved,"cannot save probe PNG");
            ah::ImagePixels decoded=ah::loadImage([file UTF8String]);
            require(decoded.width==2 && decoded.height==2 && decoded.rgba.size()==16,"probe dimensions changed");
            const unsigned char expected[]={255,0,0,255, 0,255,0,255, 0,0,255,255, 128,0,128,128};
            for (size_t i=0;i<16;++i) require(std::abs(int(decoded.rgba[i])-int(expected[i]))<=1,"incorrect top-first RGBA or premultiplication");
            std::cout<<"PASS native ImageIO/CoreGraphics asymmetric rows and premultiplied RGBA\n";
            bool rejected=false;
            try {ah::loadImage([temporary UTF8String]);} catch (const std::exception&) {rejected=true;}
            require(rejected,"missing PNG was accepted");
            std::cout<<"PASS missing PNG rejected\n";
            require(argc==2,"provide AhogeAssets folder");
            NSString* root=[NSString stringWithUTF8String:argv[1]];
            NSDirectoryEnumerator* files=[[NSFileManager defaultManager] enumeratorAtPath:root];
            int count=0;
            for (NSString* relative in files) if ([[relative pathExtension] isEqualToString:@"png"]) {
                decoded=ah::loadImage([[root stringByAppendingPathComponent:relative] UTF8String]);
                require(decoded.width>0 && decoded.height>0,"empty game texture");
                for (size_t i=0;i<decoded.rgba.size();i+=4) for (size_t c=0;c<3;++c)
                    require(decoded.rgba[i+c]<=decoded.rgba[i+3],"non-premultiplied game texture");
                ++count;
            }
            require(count==76,"expected 75 original frame PNGs and one atlas");
            std::cout<<"PASS native decoding of all 76 game PNGs\n";
            [[NSFileManager defaultManager] removeItemAtPath:file error:NULL];return 0;
        } catch (const std::exception& ex) {
            [[NSFileManager defaultManager] removeItemAtPath:file error:NULL];
            std::cerr<<"FAIL "<<ex.what()<<"\n";return 1;
        }
    }
}
