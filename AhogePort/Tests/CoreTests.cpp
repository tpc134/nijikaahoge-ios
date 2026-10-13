#include "AHCore.hpp"
#include "AHAudioMixer.hpp"
#include "AHAudioState.hpp"
#include "AHWave.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <thread>
#include <fstream>
#include <iterator>

using namespace ah;
static int passed=0;
#define CHECK(v) do {if (!(v)) {std::ostringstream s;s<<__FILE__<<":"<<__LINE__<<": "<<#v;throw std::runtime_error(s.str());}} while(0)
static void near(float a,float b,float eps=.02f) {CHECK(std::fabs(a-b)<=eps);}
static void advance(Game& g,float seconds) {for (int i=0;i<int(seconds*60);++i) g.update(1.f/60);}
static Point center(Obj* o) {Point c;for (Point p:o->poly()) {c.x+=p.x;c.y+=p.y;}c.x/=o->poly().size();c.y/=o->poly().size();return c;}
static Point rightOf(Obj* o,float distance) {
    Point p=*std::max_element(o->poly().begin(),o->poly().end(),[](Point a,Point b){return a.x<b.x;});p.x+=distance;return p;
}
static int heads(Game& g) {int n=0;for (Obj* o:g.all("p3")) n+=o->iv(0)!=0;return n;}
static void run(const char* name,std::function<void()> f) {f();++passed;std::cout<<"PASS "<<name<<"\n";}
int main(int argc,char** argv) {
    try {
        run("original data and fixture decomposition",[](){
            Game g;CHECK(g.data.instances.size()==31);CHECK(g.data.types.size()==36);CHECK(b2_version.major==2 && b2_version.minor==2 && b2_version.revision==1);
            CHECK(g.byName("p1")->frame()->poly.size()==17);CHECK(g.byName("p1")->frame()->triangles.size()==45);
            CHECK(g.byName("p1")->body->GetFixtureList()->GetRestitution()==.9f);
            CHECK(g.world.GetAllowSleeping());near(g.world.GetGravity().y,10);
            for (auto& type:g.data.types) for (auto& anim:type.second.anims) for (const Frame& f:anim.second.frames)
                CHECK(f.poly.size()<3 || f.triangles.size()==(f.poly.size()-2)*3);
        });
        run("letterbox and 12-point finger radius",[](){
            for (Point size:{Point(320,480),Point(320,568),Point(480,320),Point(768,1024),Point(320,320)}) {
                Viewport v;v.resize(size.x,size.y);Point p=v.logical(v.ox+461*v.scale,v.oy+227*v.scale);
                near(p.x,461);near(p.y,227);near(v.fingerRadius(),std::min(96.f,12/v.scale));
            }
        });
        run("head touch tolerance and no initial jump",[](){
            Game g;advance(g,3);Obj* h=g.byName("p3");Point p=rightOf(h,8);float x=h->x,y=h->y;
            CHECK(!h->contains(p.x,p.y));g.touchDown(p.x,p.y);CHECK(!g.dragging);
            g.touchDown(p.x,p.y,12);CHECK(g.dragging==h);near(h->x,x);near(h->y,y);
            g.touchMove(p.x+100,p.y-100);near(h->x,x+100);near(h->y,y-100);g.touchUp();
            CHECK(h->iv(0)==0);CHECK(heads(g)==1);advance(g,3);
            Obj* next=nullptr;for (Obj* o:g.all("p3")) if (o->iv(0)) next=o;
            p=rightOf(next,13);g.touchDown(p.x,p.y,12);CHECK(!g.dragging);
        });
        run("exact bag and debris hits win",[](){
            Game g;advance(g,3);Obj* h=g.byName("p3"),*b=g.byName("bag");Point p=rightOf(h,8),c=center(b);
            b->x+=p.x-c.x;b->y+=p.y-c.y;g.touchDown(p.x,p.y,12);CHECK(g.dragging==b);g.touchUp();b->x=2500;
            Obj* d=g.create("p3",3,0,0,"1");d->setFrame(5);d->playing=false;d->dragEnabled=true;d->setIV(0,0);c=center(d);
            d->x+=p.x-c.x;d->y+=p.y-c.y;g.touchDown(p.x,p.y,12);CHECK(g.dragging==d);g.touchUp();
            d->x=1000;d->y=400;p=rightOf(d,8);g.touchDown(p.x,p.y,12);CHECK(!g.dragging);
        });
        run("rotated mirrored head tolerance",[](){
            Game g;Obj* h=g.byName("p3");h->angle=.7f;
            for (bool mirror:{false,true}) {h->mirrored=mirror;Point p=rightOf(h,8),far=rightOf(h,13);
                CHECK(!h->contains(p.x,p.y));CHECK(h->contains(p.x,p.y,12));CHECK(!h->contains(far.x,far.y,12));}
        });
        run("letterbox padding excluded",[](){
            Game g;Obj* h=g.byName("p3");h->x=-8;h->y=100;g.touchDown(-1,100,96);CHECK(!g.dragging);
        });
        run("single drag owner and cancellation",[](){
            Game g;advance(g,3);Obj* h=g.byName("p3");Point p=center(h),b=center(g.byName("bag"));
            g.touchDown(p.x,p.y);CHECK(g.dragging==h);g.touchDown(b.x,b.y);CHECK(g.dragging==h);
            g.cancelTouch();CHECK(!g.dragging);CHECK(heads(g)==1);g.cancelTouch();CHECK(heads(g)==1);
        });
        run("release zeroes velocities at final position",[](){
            Game g;advance(g,3);Obj* b=g.byName("bag");Point p=center(b);g.touchDown(p.x,p.y);CHECK(g.dragging==b);
            g.touchMove(p.x+200,p.y-100);g.update(1.f/60);CHECK(b->body->GetLinearVelocity().LengthSquared()>0);
            float x=b->x,y=b->y;g.touchUp();near(b->body->GetLinearVelocity().LengthSquared(),0,0);near(b->body->GetAngularVelocity(),0,0);
            near(b->body->GetPosition().x/PHYS_SCALE,x);near(b->body->GetPosition().y/PHYS_SCALE,y);
        });
        run("mirror reflects polygon and bounds about origin",[](){
            Game g;Obj* s=g.byName("Sprite9");s->x=500;s->y=600;s->angle=0;s->mirrored=false;
            Polygon normal=s->poly();auto bounds=s->bounds();s->mirrored=true;
            for (size_t i=0;i<normal.size();++i) {near(s->poly()[i].x,1000-normal[i].x);near(s->poly()[i].y,normal[i].y);}
            near(s->bounds()[0],1000-bounds[2]);CHECK(&s->poly()==&s->poly());CHECK(&g.drawOrder()==&g.drawOrder());
        });
        run("draw corners match mirrored rotated bounds",[](){
            Game g;Obj* o=g.byName("Sprite9");o->x=500;o->y=600;o->angle=.4f;o->mirrored=true;
            std::array<float,4> b={{INFINITY,INFINITY,-INFINITY,-INFINITY}};
            for (int i=0;i<4;++i) {
                float dx=((i==1 || i==2) ? 1-o->ox:-o->ox)*o->width()*-1,dy=(i>=2 ? 1-o->oy:-o->oy)*o->height();
                float x=o->x+dx*std::cos(o->angle)-dy*std::sin(o->angle),y=o->y+dx*std::sin(o->angle)+dy*std::cos(o->angle);
                b[0]=std::min(b[0],x);b[1]=std::min(b[1],y);b[2]=std::max(b[2],x);b[3]=std::max(b[3],y);
            }
            for (int i=0;i<4;++i) near(b[i],o->bounds()[i]);
        });
        run("all four Seika directions and signal retreat",[](){
            Game g;g.flyerTimer=1200;g.byName("Sprite5")->body->SetActive(false);
            const float expected[4][4]={{-186,333,46,0},{1228,1081,341,1},{-30,1050,10,0},{1201,-81,217,1}};
            for (int i=0;i<4;++i) {
                g.groupS=true;g.startFlyer(i+1);advance(g,2.2f);Obj* s=g.byName("Sprite9");
                near(s->x,expected[i][0]);near(s->y,expected[i][1]);near(s->angle*180/PI,expected[i][2]);CHECK(s->mirrored==(expected[i][3]!=0));CHECK(s->onScreen());
                g.sched.signal("Z");advance(g,2.7f);CHECK(!g.groupS);
            }
        });
        run("sleeping small Bocchi teleports and returns three times",[](){
            Game g;g.flyerTimer=1200;advance(g,10);Obj* s=g.byName("Sprite5");CHECK(!s->body->IsAwake());
            for (int i=0;i<3;++i) {
                g.teleport(s,1182,-1109);CHECK(s->body->IsAwake());advance(g,2);g.teleport(s,1182,-633);advance(g,8);
                CHECK(s->y>=800 && s->y<=1050);CHECK(!s->body->IsAwake());CHECK(g.all("Sprite5").size()==1);
            }
        });
        run("actual Seika overlap and two-second small Bocchi return",[](){
            Game g;g.flyerTimer=1200;advance(g,10);Obj* small=g.byName("Sprite5"),*s=g.byName("Sprite9");
            CHECK(!small->body->IsAwake());g.groupS=true;g.startFlyer(1);g.tweens.kill(s);s->angle=0;s->mirrored=false;
            s->x=small->x-50;s->y=small->y+100;CHECK(overlap(*s,*small));g.update(1.f/60);
            CHECK(s->fi==1);CHECK(small->y<-1000);CHECK(small->body->IsAwake());advance(g,2.1f);
            CHECK(s->fi==0);CHECK(small->y<-500);advance(g,6);CHECK(small->y>=800 && small->y<=1050);CHECK(!g.groupS);
        });
        run("single convex full-size ahoge fixture",[](){
            Game g;for (bool gold:{false,true}) {
                Obj* o=g.spawnDebris(800,400,gold);o->setFrame(5);g.update(1.f/60);
                CHECK(o->body->GetFixtureList());CHECK(!o->body->GetFixtureList()->GetNext());
                CHECK(((b2PolygonShape*)o->body->GetFixtureList()->GetShape())->GetVertexCount()==5);
            }
        });
        run("growth animation enlarges fixtures",[](){
            Game g;Obj* h=g.byName("p3");float before=h->body->GetMass();advance(g,3);
            CHECK(h->fi==5);CHECK(h->body->GetMass()>before*2);CHECK(!h->body->IsActive());CHECK(h->frame()->poly.size()==5);
        });
        run("animation duration, looping, pingpong and restart",[](){
            Type t;t.name="test";Anim a;a.name="a";a.speed=10;Frame f;f.w=20;f.h=30;f.ox=.3f;f.dur=2;
            a.frames={f,f,f};t.anims["a"]=a;Obj o;o.type=&t;o.setAnim("a");o.animate(.19f);CHECK(o.fi==0);o.animate(.02f);CHECK(o.fi==1);
            o.animate(1);CHECK(o.fi==2 && !o.playing);o.setAnim("a",true);CHECK(o.fi==0 && o.playing);near(o.ox,.3f);
            t.anims["a"].loop=true;o.setAnim("a",true);o.animate(.61f);CHECK(o.fi==0 && o.playing);
            t.anims["a"].pingPong=true;o.setAnim("a",true);o.animate(.81f);CHECK(o.fi==0 && !o.forward);
        });
        run("bag offscreen preserves contents",[](){
            Game g;advance(g,3);Obj* b=g.byName("bag");b->setIV(0,10);b->setIV(1,1);b->setIV(2,3);b->setIV(3,1);g.update(1.f/60);
            Point p=center(b);g.touchDown(p.x,p.y);CHECK(g.dragging==b);g.touchMove(p.x,p.y+1500);g.update(1.f/60);
            CHECK(!g.dragging);advance(g,5);CHECK(b->iv(0)==10 && b->iv(2)==3 && b->iv(1)==1);CHECK(b->fi==2);
            CHECK(b->y>=700 && b->y<=1000);CHECK(b->dragEnabled);
        });
        run("real ten-pull collect feed cycle",[](){
            Game g(Hooks(),1234);g.flyerTimer=1200;advance(g,3);
            for (int i=0;i<10;++i) {
                Obj* h=nullptr;for (Obj* o:g.all("p3")) if (o->iv(0)) h=o;
                Point c=center(h);g.touchDown(c.x,c.y);CHECK(g.dragging==h);
                g.touchMove(c.x+940-h->x,c.y+600-h->y);g.touchUp();advance(g,2.5f);CHECK(g.byName("bag")->iv(0)==i+1);
            }
            Obj* b=g.byName("bag");Point c=center(b),r=center(g.byName("ryo"));g.touchDown(c.x,c.y);g.touchMove(r.x,r.y);g.update(1.f/60);
            CHECK(g.score==1 && b->iv(0)==0);advance(g,5);CHECK(b->dragEnabled);
        });
        run("seven feeding branches and repeated max celebration",[](){
            Game g;g.flyerTimer=1200;
            const int cases[8][5]={{0,0,0,0,2},{0,1,2,1,3},{0,1,7,1,5},{0,1,10,1,5},{0,1,10,1,5},{1,1,2,0,2},{1,1,7,0,3},{1,1,10,0,3}};
            for (int i=0;i<8;++i) {
                advance(g,6);Obj* b=g.byName("bag");const int* c=cases[i];b->setIV(0,10);b->setIV(1,c[1]);b->setIV(2,c[2]);b->setIV(3,c[3]);advance(g,4);
                Obj* girl=g.byName(c[0] ? "kita":"ryo");Point p=center(b),q=center(girl);g.touchDown(p.x,p.y);CHECK(g.dragging==b);
                g.touchMove(q.x,q.y);g.update(1.f/60);CHECK(g.score==i+1);CHECK(girl->fi==c[4]);CHECK(b->iv(0)==0 && b->iv(2)==0);
                CHECK(std::any_of(g.emitters.begin(),g.emitters.end(),[](const Emitter& e){return e.layer==3;}));
                if (c[2]>=10) {CHECK(g.byName("Sprite7")->fi==0);advance(g,1.2f);CHECK(g.byName("Sprite7")->fi==1);
                    CHECK(std::any_of(g.emitters.begin(),g.emitters.end(),[](const Emitter& e){return e.layer==5;}));}
            }
        });
        run("normal bag does not feed Kita",[](){
            Game g;advance(g,3);Obj* b=g.byName("bag"),*k=g.byName("kita");b->setIV(0,10);g.update(1.f/60);
            Point p=center(b),q=center(k);g.touchDown(p.x,p.y);g.touchMove(q.x,q.y);g.update(1.f/60);CHECK(g.score==0);g.touchUp();
        });
        run("all milestones fire only once",[](){
            Game g;g.flyerTimer=1200;
            for (int score:{10,50,100,500,1000,5000}) {
                g.score=score;g.update(1.f/60);CHECK(g.byName("Sprite7")->anim->name=="2");if (score>=50) near(g.flyerTimer,10);
                advance(g,5);near(g.byName("Sprite7")->x,1515);size_t n=g.milestones.size();g.update(1.f/60);CHECK(g.milestones.size()==n);
            }
        });
        run("offscreen head regenerates once and is safely reclaimed",[](){
            Game g;Obj* h=g.byName("p3");g.teleport(h,2500,500);g.update(1.f/60);CHECK(heads(g)==1);CHECK(g.all("p3").size()==1);
            advance(g,3);CHECK(heads(g)==1);
        });
        run("sleep, debris stress and finite state at 60 120 240",[](){
            for (int count:{60,120,240}) {
                Game g;g.flyerTimer=1200;advance(g,3);
                for (int i=0;i<count;++i) g.spawnDebris(100+(i%20)*52,300+(i/20)*20);
                std::vector<double> times;
                for (int i=0;i<600;++i) {
                    auto start=std::chrono::steady_clock::now();g.update(1.f/60);
                    for (Obj* o:g.drawOrder()) {o->bounds();CHECK(std::isfinite(o->x) && std::isfinite(o->y));}
                    times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
                }
                CHECK(heads(g)==1);std::sort(times.begin(),times.end());
                std::cout<<"METRIC debris="<<count<<" median_ms="<<times[300]<<" p95_ms="<<times[570]<<"\n";
            }
        });
        run("long frame clamp and no post-pause catch-up",[](){
            Game a,b;a.update(60);b.update(.05f);near(a.byName("bag")->y,b.byName("bag")->y);
        });
        run("audio exact linear stereo pan and dB",[](){
            AudioMixer m;m.setClip(0,{10000,20000,10000,20000});int16_t pcm[4];CHECK(m.play(0,0,50));m.render(pcm,2);
            CHECK(pcm[0]==5000 && pcm[1]==20000);m.stop();CHECK(m.play(0,-20,-50));m.render(pcm,2);near(pcm[0],1000,1);near(pcm[1],1000,1);
        });
        run("audio loop, twelve voices, clipping and suspension",[](){
            AudioMixer m;m.setClip(0,{10000,-10000});int16_t pcm[8];CHECK(m.play(0,0,0,true));m.render(pcm,4);
            for (int i=0;i<4;++i) CHECK(pcm[i*2]==10000 && pcm[i*2+1]==-10000);
            m.stop();m.render(pcm,4);for (int16_t v:pcm) CHECK(v==0);
            for (int i=0;i<13;++i) CHECK(m.play(0,0,0));m.render(pcm,1);CHECK(pcm[0]==32767 && pcm[1]==-32768);
        });
        run("audio ring producer consumer safety",[](){
            AudioMixer m;m.setClip(0,{1000,1000,500,500});std::atomic<bool> done(false);
            std::thread producer([&](){for (int i=0;i<10000;++i) while (!m.play(0,-20,0)) std::this_thread::yield();done.store(true);});
            int16_t pcm[128];while (!done.load()) m.render(pcm,64);producer.join();m.render(pcm,64);m.stop();
        });
        run("audio lifecycle intent never resumes hidden or background game",[](){
            AudioState s;CHECK(!s.canStart(true));s.resume();CHECK(s.canStart(true));CHECK(!s.canStart(false));
            s.interruptionBegan();CHECK(!s.canStart(true));s.suspend();s.interruptionEnded(true);CHECK(!s.canStart(true));
            s.resume();CHECK(s.canStart(true));s.resume();CHECK(s.canStart(true));s.suspend();s.suspend();CHECK(!s.canStart(true));
        });
        run("audio reset and declined interruption require explicit user action",[](){
            AudioState s;s.resume();s.interruptionBegan();s.interruptionEnded(false);CHECK(!s.canStart(true));
            s.resume();CHECK(!s.canStart(true));s.userAction();CHECK(s.canStart(true));
            s.mediaReset();CHECK(!s.canStart(true));s.interruptionEnded(true);CHECK(!s.canStart(true));
            s.suspend();s.userAction();CHECK(!s.canStart(true));s.resume();CHECK(s.canStart(true));
        });
        run("audio missing interruption-ended notification can recover on touch",[](){
            AudioState s;s.resume();s.interruptionBegan();s.suspend();s.resume();CHECK(!s.canStart(true));
            s.userAction();CHECK(s.canStart(true));CHECK(!s.canStart(false));
        });
        run("audio variable hardware slices require no fixed scratch buffer",[](){
            AudioMixer m;m.setClip(0,{1000,-1000});CHECK(m.play(0,0,0,true));
            for (size_t frames:{size_t(128),size_t(4096),size_t(8192)}) {
                std::vector<int16_t> pcm(frames*2);m.render(pcm.data(),frames);
                for (size_t i=0;i<frames;++i) CHECK(pcm[i*2]==1000 && pcm[i*2+1]==-1000);
            }
        });
        run("actual bundled WAV parser loads twelve clips with correct channels",[&](){
            CHECK(argc>=2);
            for (int i=0;i<12;++i) {
                std::string file=std::string(argv[1])+"/media/"+std::to_string(i)+".wav";
                std::ifstream stream(file.c_str(),std::ios::binary);CHECK(stream.good());
                std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)),std::istreambuf_iterator<char>());
                std::vector<int16_t> pcm;CHECK(loadWave(bytes.data(),bytes.size(),pcm));CHECK(!pcm.empty() && pcm.size()%2==0);
                if (i==0) for (size_t f=0;f<pcm.size();f+=2) CHECK(pcm[f]==pcm[f+1]);
            }
        });
        run("WAV malformed chunks and every truncated prefix fail safely",[](){
            const uint8_t raw[]={ 'R','I','F','F',40,0,0,0,'W','A','V','E',
                'f','m','t',' ',16,0,0,0,1,0,2,0,128,187,0,0,0,238,2,0,4,0,16,0,
                'd','a','t','a',4,0,0,0,1,0,255,255 };
            std::vector<uint8_t> b(raw,raw+sizeof(raw));std::vector<int16_t> pcm;CHECK(loadWave(b.data(),b.size(),pcm));
            CHECK(pcm.size()==2 && pcm[0]==1 && pcm[1]==-1);
            for (size_t n=0;n<b.size();++n) CHECK(!loadWave(b.data(),n,pcm));
            CHECK(!loadWave(nullptr,48,pcm));
            for (size_t offset:{size_t(4),size_t(16),size_t(40)}) {
                auto bad=b;for (size_t i=0;i<4;++i) bad[offset+i]=255;CHECK(!loadWave(bad.data(),bad.size(),pcm));
            }
            auto bad=b;bad[32]=2;CHECK(!loadWave(bad.data(),bad.size(),pcm));
            bad=b;bad[24]=0;CHECK(!loadWave(bad.data(),bad.size(),pcm));
            bad=b;bad[40]=3;CHECK(!loadWave(bad.data(),bad.size(),pcm));
        });
        std::cout<<"RESULT "<<passed<<" tests passed\n";
        return 0;
    } catch (const std::exception& ex) {std::cerr<<"FAIL "<<ex.what()<<"\n";return 1;}
}
