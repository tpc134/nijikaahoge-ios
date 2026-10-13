#include "AHRenderer.hpp"
#include <EGL/egl.h>
#include <stdexcept>
#include <cstring>

typedef const unsigned char* (*LoadPixels)(const char*,int*,int*);
static LoadPixels loader=nullptr;
static std::unique_ptr<ah::Game> game;
static std::unique_ptr<ah::Renderer> renderer;
static EGLDisplay display=EGL_NO_DISPLAY;
static EGLSurface surface=EGL_NO_SURFACE;
static EGLContext context=EGL_NO_CONTEXT;
static int width,height;
static std::string error;
namespace ah {
ImagePixels loadImage(const std::string& file) {
    ImagePixels image;const unsigned char* data=loader(file.c_str(),&image.width,&image.height);
    if (!data) throw std::runtime_error("Image callback failed");
    image.rgba.assign(data,data+size_t(image.width)*image.height*4);return image;
}
}
extern "C" __declspec(dllexport) const char* ah_error() {return error.c_str();}
extern "C" __declspec(dllexport) int ah_init(const char* root,int w,int h,LoadPixels fn) {
    try {
        loader=fn;width=w;height=h;display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint major=0,minor=0;if (!eglInitialize(display,&major,&minor)) throw std::runtime_error("eglInitialize failed");
        const EGLint attrs[]={EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_NONE};
        EGLConfig cfg;EGLint count=0;if (!eglChooseConfig(display,attrs,&cfg,1,&count) || count!=1) throw std::runtime_error("eglChooseConfig failed");
        const EGLint size[]={EGL_WIDTH,w,EGL_HEIGHT,h,EGL_NONE};surface=eglCreatePbufferSurface(display,cfg,size);
        const EGLint version[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};context=eglCreateContext(display,cfg,EGL_NO_CONTEXT,version);
        if (!eglMakeCurrent(display,surface,surface,context)) throw std::runtime_error("eglMakeCurrent failed");
        game.reset(new ah::Game(ah::Hooks(),1234));game->flyerTimer=1200;
        for (int i=0;i<180;++i) game->update(1.f/60);
        renderer.reset(new ah::Renderer(game->data,root));return 1;
    } catch (const std::exception& e) {error=e.what();return 0;}
}
extern "C" __declspec(dllexport) void ah_scene(int scene) {
    if (scene>=1 && scene<=4) {
        game->byName("Sprite5")->body->SetActive(false);game->startFlyer(scene);
        for (int i=0;i<130;++i) game->update(1.f/60);
    } else if (scene==5) {
        game->score=38;game->byName("font")->text="38";
        ah::Obj* b=game->byName("bag");b->setIV(0,10);b->setIV(1,1);b->setIV(2,10);b->setIV(3,0);
        for (int i=0;i<240;++i) game->update(1.f/60);
        auto center=[](ah::Obj* o){ah::Point p;for (ah::Point q:o->poly()){p.x+=q.x;p.y+=q.y;}p.x/=o->poly().size();p.y/=o->poly().size();return p;};
        ah::Point p=center(b),q=center(game->byName("kita"));game->touchDown(p.x,p.y);game->touchMove(q.x,q.y);
        for (int i=0;i<78;++i) game->update(1.f/60);
    } else if (scene==6) {
        for (int i=0;i<240;++i) game->spawnDebris(float(100+(i%20)*52),float(300+(i/20)*20));
        for (int i=0;i<150;++i) game->update(1.f/60);
    }
}
extern "C" __declspec(dllexport) int ah_draw(unsigned char* pixels) {
    try {renderer->draw(*game,width,height);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        if (glGetError()!=GL_NO_ERROR) throw std::runtime_error("OpenGL error");return renderer->drawCalls;
    } catch (const std::exception& e) {error=e.what();return -1;}
}
extern "C" __declspec(dllexport) void ah_purge() {renderer->purgeTextures();}
extern "C" __declspec(dllexport) void ah_close() {
    renderer.reset();game.reset();eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);
    display=EGL_NO_DISPLAY;surface=EGL_NO_SURFACE;context=EGL_NO_CONTEXT;
}
