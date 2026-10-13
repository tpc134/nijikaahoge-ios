#include "AHImagePixels.hpp"
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <CoreGraphics/CoreGraphics.h>
#include <stdexcept>

namespace ah {
ImagePixels loadImage(const std::string& file) {
    @autoreleasepool {
        NSURL* url=[NSURL fileURLWithPath:[NSString stringWithUTF8String:file.c_str()]];
        CGImageSourceRef source=CGImageSourceCreateWithURL((__bridge CFURLRef)url,NULL);
        CGImageRef cg=source ? CGImageSourceCreateImageAtIndex(source,0,NULL):NULL;
        if (source) CFRelease(source);
        if (!cg) throw std::runtime_error("Missing or invalid texture: "+file);
        size_t width=CGImageGetWidth(cg),height=CGImageGetHeight(cg);
        if (!width || !height || width>2048 || height>2048) {CGImageRelease(cg);throw std::runtime_error("Invalid texture dimensions: "+file);}
        ImagePixels out;out.width=int(width);out.height=int(height);
        out.rgba.resize(size_t(out.width)*out.height*4);
        CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
        if (!space) {CGImageRelease(cg);throw std::runtime_error("Unable to allocate RGB color space");}
        CGContextRef ctx=CGBitmapContextCreate(out.rgba.data(),out.width,out.height,8,out.width*4,space,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(space);
        if (!ctx) {CGImageRelease(cg);throw std::runtime_error("Unable to allocate RGBA texture");}
        // An unflipped bitmap-to-bitmap copy retains top-first rows. UIKit's
        // y-down context correction would invert these pixels a second time.
        CGContextSetBlendMode(ctx,kCGBlendModeCopy);
        CGContextSetInterpolationQuality(ctx,kCGInterpolationNone);
        CGContextDrawImage(ctx,CGRectMake(0,0,out.width,out.height),cg);
        CGContextRelease(ctx);CGImageRelease(cg);
        return out;
    }
}
}
