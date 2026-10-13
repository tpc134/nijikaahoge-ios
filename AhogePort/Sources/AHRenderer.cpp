#include "AHRenderer.hpp"
#include <stdexcept>

namespace ah {
const size_t MAX_QUADS=1024;
static GLuint shader(GLenum type,const char* source) {
    GLuint s=glCreateShader(type);glShaderSource(s,1,&source,nullptr);glCompileShader(s);
    GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if (!ok) {char log[2048]={0};glGetShaderInfoLog(s,sizeof(log),nullptr,log);glDeleteShader(s);throw std::runtime_error(log);}
    return s;
}
Renderer::Renderer(Data& d,const std::string& assetRoot,ImageLoader loader):data(&d),root(assetRoot),imageLoader(loader) {
    const char* vs="uniform mat4 u_mvp;attribute vec2 a_pos;attribute vec2 a_uv;attribute vec4 a_col;"
        "varying mediump vec2 v_uv;varying lowp vec4 v_col;void main(){v_uv=a_uv;v_col=a_col;gl_Position=u_mvp*vec4(a_pos,0.,1.);}";
    const char* fs="precision mediump float;varying mediump vec2 v_uv;varying lowp vec4 v_col;uniform sampler2D u_tex;"
        "void main(){gl_FragColor=texture2D(u_tex,v_uv)*v_col;}";
    GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);
    program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);
    glBindAttribLocation(program,0,"a_pos");glBindAttribLocation(program,1,"a_uv");glBindAttribLocation(program,2,"a_col");
    glLinkProgram(program);glDeleteShader(v);glDeleteShader(f);
    GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if (!ok) {glDeleteProgram(program);program=0;throw std::runtime_error("Unable to link ES2 sprite shader");}
    projectionLocation=glGetUniformLocation(program,"u_mvp");textureLocation=glGetUniformLocation(program,"u_tex");
    vertices.resize(MAX_QUADS*4*8);
    glGenBuffers(1,&vbo);glGenBuffers(1,&ibo);
    std::vector<GLushort> indices(MAX_QUADS*6);
    for (size_t i=0;i<MAX_QUADS;++i) {
        const int ids[]={0,1,2,0,2,3};for (int j=0;j<6;++j) indices[i*6+j]=GLushort(i*4+ids[j]);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(GLushort),indices.data(),GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),nullptr,GL_STREAM_DRAW);
}
Renderer::~Renderer() {
    purgeTextures();if (vbo) glDeleteBuffers(1,&vbo);if (ibo) glDeleteBuffers(1,&ibo);if (program) glDeleteProgram(program);
}
void Renderer::purgeTextures() {
    for (auto item:textures) glDeleteTextures(1,&item.second);
    textures.clear();current=0;
}
GLuint Renderer::texture(const std::string& file) {
    auto it=textures.find(file);if (it!=textures.end()) return it->second;
        ImagePixels image=imageLoader(root+"/"+file);
        size_t w=image.width,h=image.height;
        if (!w || !h || image.rgba.size()!=w*h*4) throw std::runtime_error("Invalid texture: "+file);
        GLint limit=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);
        if (w>size_t(limit) || h>size_t(limit)) throw std::runtime_error("Texture exceeds device limit: "+file);
        GLuint id=0;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT,4);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,GLsizei(w),GLsizei(h),0,GL_RGBA,GL_UNSIGNED_BYTE,image.rgba.data());
        GLenum error=glGetError();if (error!=GL_NO_ERROR) {glDeleteTextures(1,&id);throw std::runtime_error("Texture upload failed: "+file);}
        textures[file]=id;return id;
}
void Renderer::quad(GLuint tex,float u0,float v0,float u1,float v1,
                    float x,float y,float w,float h,float px,float py,float angle,bool mirror,
                    float r,float g,float b,float a) {
    if (tex!=current || quads>=MAX_QUADS) {flush();current=tex;}
    float c=std::cos(angle),s=std::sin(angle);
    for (int i=0;i<4;++i) {
        bool right=i==1 || i==2,bottom=i>=2;
        float dx=((right ? x+w:x)-px)*(mirror ? -1.f:1.f),dy=(bottom ? y+h:y)-py;
        float* out=&vertices[(quads*4+i)*8];
        out[0]=px+dx*c-dy*s;out[1]=py+dx*s+dy*c;out[2]=right ? u1:u0;out[3]=bottom ? v1:v0;
        out[4]=r;out[5]=g;out[6]=b;out[7]=a;
    }
    ++quads;
}
void Renderer::flush() {
    if (!quads) return;
    glBindTexture(GL_TEXTURE_2D,current);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),nullptr,GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER,0,quads*4*8*sizeof(float),vertices.data());
    glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);glEnableVertexAttribArray(2);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(const GLvoid*)0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(const GLvoid*)(2*sizeof(float)));
    glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,8*sizeof(float),(const GLvoid*)(4*sizeof(float)));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo);glDrawElements(GL_TRIANGLES,GLsizei(quads*6),GL_UNSIGNED_SHORT,nullptr);
    ++drawCalls;quads=0;
}
void Renderer::sprite(const Obj& o) {
    const Frame* f=o.frame();if (!f) return;
    Region region;region.file=f->file;
    auto it=data->regions.find(f->file);if (it!=data->regions.end()) region=it->second;
    GLuint id=texture(region.file);
    // Loading binds a texture, so restore the pending batch's binding in flush().
    float w=o.width(),h=o.height(),a=clamp(o.color[3],0,1);
    quad(id,region.u0,region.v0,region.u1,region.v1,o.x-o.ox*w,o.y-o.oy*h,w,h,o.x,o.y,o.angle,o.mirrored,
         o.color[0]*a,o.color[1]*a,o.color[2]*a,a);
}
void Renderer::font(const Obj& o) {
    if (o.text.empty() || o.type->image.empty() || o.fontW<=0 || o.fontH<=0) return;
    GLuint id=texture(o.type->image);float scale=o.fontScale,cw=o.fontW,ch=o.fontH;
    int cols=std::max(1,int(o.type->iw/cw));
    float total=o.text.size()*(cw+o.fontSpacing)*scale-o.fontSpacing*scale;
    float x=o.x-o.ox*o.fontBoxW,y=o.y-o.oy*o.fontBoxH;
    if (o.hAlign==1) x+=(o.fontBoxW-total)/2;else if (o.hAlign==2) x+=o.fontBoxW-total;
    if (o.vAlign==1) y+=(o.fontBoxH-ch*scale)/2;else if (o.vAlign==2) y+=o.fontBoxH-ch*scale;
    float a=clamp(o.color[3],0,1);
    for (char c:o.text) {
        size_t index=o.charset.find(c);
        if (index!=std::string::npos) {
            float gx=(index%cols)*cw,gy=(index/cols)*ch;
            quad(id,gx/o.type->iw,gy/o.type->ih,(gx+cw)/o.type->iw,(gy+ch)/o.type->ih,
                 x,y,cw*scale,ch*scale,x,y,0,false,o.color[0]*a,o.color[1]*a,o.color[2]*a,a);
        }
        x+=(cw+o.fontSpacing)*scale;
    }
}
void Renderer::emitter(const Emitter& e) {
    if (e.type->image.empty()) return;
    Region region;region.file=e.type->image;
    auto it=data->regions.find(e.type->image);if (it!=data->regions.end()) region=it->second;
    GLuint id=texture(region.file);
    for (const Particle& p:e.particles) {
        float size=p.size;if (p.x+size<0 || p.x-size>1280 || p.y+size<0 || p.y-size>1024) continue;
        float a=clamp(e.opacity(p),0,1);
        quad(id,region.u0,region.v0,region.u1,region.v1,p.x-size/2,p.y-size/2,size,size,p.x,p.y,p.angle,false,a,a,a,a);
    }
}
void Renderer::draw(Game& g,int width,int height) {
    if (width<=0 || height<=0) return;
    Viewport v;v.resize(float(width),float(height));
    glDisable(GL_SCISSOR_TEST);glViewport(0,0,width,height);glClearColor(245/255.f,132/255.f,132/255.f,1);glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    std::fill(matrix,matrix+16,0.f);matrix[0]=2*v.scale/width;matrix[5]=-2*v.scale/height;
    matrix[10]=-1;matrix[12]=2*v.ox/width-1;matrix[13]=1-2*v.oy/height;matrix[15]=1;
    glEnable(GL_SCISSOR_TEST);glScissor(GLint(v.ox),GLint(height-v.oy-1024*v.scale),GLsizei(1280*v.scale),GLsizei(1024*v.scale));
    glUseProgram(program);glUniformMatrix4fv(projectionLocation,1,GL_FALSE,matrix);
    glActiveTexture(GL_TEXTURE0);glUniform1i(textureLocation,0);quads=0;current=0;drawCalls=0;
    const auto& list=g.drawOrder();size_t index=0;
    for (int layer=0;layer<data->layers;++layer) {
        while (index<list.size() && list[index]->layer==layer) {
            Obj* o=list[index++];if (!o->visible || o->destroyed || !o->onScreen()) continue;
            if (o->type->kind=="sprite") sprite(*o);else if (o->type->kind=="spritefont") font(*o);
        }
        for (const Emitter& e:g.emitters) if (e.layer==layer) emitter(e);
    }
    flush();glDisable(GL_SCISSOR_TEST);
}
}
