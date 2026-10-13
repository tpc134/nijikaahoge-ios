#pragma once
#include <Box2D/Box2D.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace ah {
const float PI = 3.14159265358979323846f;
const float PHYS_SCALE = 0.02f;
inline float clamp(float x, float a, float b) { return std::max(a, std::min(b, x)); }
struct Point { float x, y; Point(float a=0, float b=0): x(a), y(b) {} };
typedef std::vector<Point> Polygon;
Polygon clean(const Polygon& p);
Polygon triangulate(const Polygon& p);
bool convex(const Polygon& p);
struct Frame {
    std::string file;
    float w=0, h=0, ox=.5f, oy=.5f, dur=1;
    Polygon poly, ips, triangles;
};
struct Anim {
    std::string name;
    float speed=0;
    bool loop=false, pingPong=false;
    int repeatCount=1, repeatTo=0;
    std::vector<Frame> frames;
};
struct Type {
    std::string name, kind, image;
    float iw=0, ih=0;
    std::map<std::string, Anim> anims;
};
struct Desc {
    std::string type, anim, text, charset;
    int layer=0, z=0, frame=0, hAlign=0, vAlign=0;
    float x=0, y=0, w=0, h=0, angle=0, ox=.5f, oy=.5f;
    float fontW=46, fontH=71, fontSpacing=0;
    bool visible=true, playing=true, hasFont=false;
    std::array<float,4> color={{1,1,1,1}};
    std::vector<float> ivars, physics, drag, sine, particles;
};
struct Region {
    std::string file;
    float u0=0, v0=0, u1=1, v1=1;
};
struct Data {
    float w=1280, h=1024;
    int layers=6;
    std::map<std::string, Type> types;
    std::vector<Desc> instances;
    std::map<std::string, Region> regions;
};
Data makeData();
struct Obj {
    Type* type=nullptr;
    Anim* anim=nullptr;
    int uid=0, layer=0, z=0, fi=0, repeats=0;
    float x=0, y=0, angle=0, sx=1, sy=1, ox=.5f, oy=.5f, animT=0;
    bool visible=true, mirrored=false, playing=false, forward=true;
    bool dragEnabled=false, bodyDrives=true, destroyed=false, dirty=false;
    b2Body* body=nullptr;
    std::vector<float> physics, ivars;
    std::array<float,4> color={{1,1,1,1}};
    float sinePeriod=0, sineMag=0, sineT=0, sineBase=0;
    float fontW=46, fontH=71, fontSpacing=0, fontScale=1, fontBoxW=0, fontBoxH=0;
    int hAlign=0, vAlign=0;
    std::string text, charset;
    Obj* child=nullptr;
    const Frame* frame() const;
    float width() const; float height() const;
    float iv(size_t n) const { return n<ivars.size() ? ivars[n] : 0; }
    void setIV(size_t n, float v) { if (ivars.size()<=n) ivars.resize(n+1); ivars[n]=v; }
    void setAnim(const std::string& name, bool restart=false);
    void setFrame(int i);
    void setScale(float v) { sx=sy=v; dirty=true; }
    void animate(float dt);
    Point transform(Point p) const;
    Point imagePoint(size_t i) const;
    void geometry() const;
    const Polygon& poly() const { geometry(); return worldPoly; }
    const Polygon& triangles() const { geometry(); return worldTriangles; }
    const std::array<float,4>& bounds() const { geometry(); return box; }
    bool onScreen(float w=1280, float h=1024) const;
    bool contains(float px, float py, float padding=0) const;
private:
    mutable const Frame* cachedFrame=nullptr;
    mutable std::array<float,8> cached={{NAN,NAN,NAN,NAN,NAN,NAN,NAN,NAN}};
    mutable Polygon worldPoly, worldTriangles;
    mutable std::array<float,4> box;
};
bool overlap(const Obj& a, const Obj& b);
struct Tween {
    Obj* obj=nullptr;
    float sx=0, sy=0, ex=0, ey=0, duration=1, t=0;
    int ease=1;
    bool moveY=false;
};
class Tweens {
    std::vector<Tween> list;
public:
    void to(Obj* obj,float x,float duration,int ease);
    void toXY(Obj* obj,float x,float y,float duration,int ease);
    void kill(Obj* obj);
    bool busy(Obj* obj) const;
    void tick(float dt);
};
struct Step {
    enum Kind { Action, Delay, Signal, TweenWait } kind;
    float seconds=0;
    Obj* object=nullptr;
    std::string signal;
    std::function<void()> action;
    explicit Step(Kind k):kind(k){}
    static Step act(std::function<void()> f) { Step s(Action); s.action=f; return s; }
    static Step wait(float t) { Step s(Delay); s.seconds=t; return s; }
    static Step waitSignal(std::string v) { Step s(Signal); s.signal=v; return s; }
    static Step waitTween(Obj* o) { Step s(TweenWait); s.object=o; return s; }
};
class Scheduler {
    struct Seq {
        std::vector<Step> steps;
        size_t i=0;
        float wait=0;
        std::string signal;
        Obj* tween=nullptr;
        bool dead=false;
    };
    std::vector<std::shared_ptr<Seq> > list;
    void progress(const std::shared_ptr<Seq>& s);
public:
    void seq(std::initializer_list<Step> steps);
    void signal(const std::string& name);
    void tick(float dt, const Tweens& tweens);
};
struct Particle {
    float x=0,y=0,speed=0,angle=0,opacity=1,size=0,grow=0,gs=0,age=0;
};
struct Emitter {
    Type* type=nullptr;
    int layer=0;
    std::vector<float> params;
    std::vector<Particle> particles;
    float opacity(const Particle& q) const;
};
struct Hooks {
    std::function<void(int,float,float,bool)> sound;
    std::function<void(bool)> vibration;
};
class Game : public b2ContactListener {
    int uidSeq=1;
    bool orderDirty=true;
    std::vector<Obj*> ordered;
    std::map<std::string,std::vector<Obj*> > byType;
    std::vector<std::unique_ptr<Obj> > storage;
    std::vector<std::pair<Obj*,Obj*> > contacts;
    std::map<std::string,std::vector<float> > particleProps;
    std::mt19937 rnd;
    void buildFixtures(Obj& o);
    void createBody(Obj& o, const std::vector<float>& props);
    void contact(Obj* a,Obj* b);
    void tickLogic(float dt);
    void grow();
    void feedReset(Obj* b);
    void girlIdle(Obj* b,Obj* girl);
    void feedTail(Obj* b,Obj* girl);
    void feeding();
    void milestone(int m);
    void particlesTick(float dt);
    void beginDrag(Obj* obj,float x,float y);
    void kinematic(Obj* obj,bool enabled);
public:
    Data data;
    Hooks hooks;
    b2World world;
    Tweens tweens;
    Scheduler sched;
    std::vector<Obj*> objects;
    std::vector<Emitter> emitters;
    Obj* dragging=nullptr;
    float dragDX=0,dragDY=0;
    int score=0;
    float randV=0,ran=0,flyerTimer=-1;
    bool groupS=false,smallBusy=false,bagOffscreenWas=false;
    std::vector<int> milestones;
    explicit Game(Hooks h=Hooks(),uint32_t seed=5489);
    ~Game();
    void BeginContact(b2Contact* c) override;
    Obj* byName(const std::string& name) const;
    const std::vector<Obj*>& all(const std::string& name) const;
    const std::vector<Obj*>& drawOrder();
    Obj* create(const std::string& type,int layer,float x,float y,const std::string& anim);
    void destroy(Obj* obj);
    void teleport(Obj* obj,float x,float y);
    void update(float dt);
    void touchDown(float x,float y,float headRadius=0);
    void touchMove(float x,float y);
    void touchUp();
    void cancelTouch() { touchUp(); }
    float random(float a,float b);
    int choose(std::initializer_list<int> values);
    void sound(int id,float db,float pan);
    void vibrate(bool reward=false);
    void spawnParticles(const std::string& name,Obj* at,int layer,float angle=NAN);
    void startFlyer(int direction);
    Obj* spawnDebris(float x,float y,bool gold=false);
};
struct Viewport {
    float scale=1, ox=0, oy=0;
    void resize(float w,float h) { scale=std::min(w/1280,h/1024); ox=(w-1280*scale)/2; oy=(h-1024*scale)/2; }
    Point logical(float x,float y) const { return Point((x-ox)/scale,(y-oy)/scale); }
    float fingerRadius() const { return std::min(96.f,12.f/scale); }
};
}
