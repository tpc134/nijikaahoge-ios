#pragma once
#include "AHCore.hpp"
#include "AHImagePixels.hpp"
#ifdef __APPLE__
#include <OpenGLES/ES2/gl.h>
#else
#include <GLES2/gl2.h>
#endif

namespace ah {
typedef std::function<ImagePixels(const std::string&)> ImageLoader;
class Renderer {
    GLuint program=0,vbo=0,ibo=0,current=0;
    GLint projectionLocation=0,textureLocation=0;
    size_t quads=0;
    std::vector<float> vertices;
    std::map<std::string,GLuint> textures;
    Data* data;
    std::string root;
    ImageLoader imageLoader;
    float matrix[16];
    GLuint texture(const std::string& file);
    void flush();
    void quad(GLuint tex,float u0,float v0,float u1,float v1,
              float x,float y,float w,float h,float px,float py,float angle,bool mirror,
              float r,float g,float b,float a);
    void sprite(const Obj& o);
    void font(const Obj& o);
    void emitter(const Emitter& e);
public:
    int drawCalls=0;
    Renderer(Data& d,const std::string& assetRoot,ImageLoader loader=loadImage);
    ~Renderer();
    void draw(Game& g,int width,int height);
    void purgeTextures();
    void abandonGL() { program=vbo=ibo=current=0; textures.clear(); }
};
}
