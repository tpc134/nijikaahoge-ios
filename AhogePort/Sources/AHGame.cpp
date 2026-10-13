#include "AHCore.hpp"
#include <cassert>

namespace ah {
Game::Game(Hooks h,uint32_t seed):rnd(seed),data(makeData()),hooks(h),world(b2Vec2(0,10)) {
    world.SetAllowSleeping(true);world.SetContactListener(this);
    for (const Desc& d:data.instances) {
        auto found=data.types.find(d.type);if (found==data.types.end()) continue;
        std::unique_ptr<Obj> ptr(new Obj());Obj* o=ptr.get();
        o->type=&found->second;o->x=d.x;o->y=d.y;o->angle=d.angle;o->ox=d.ox;o->oy=d.oy;
        o->layer=d.layer;o->z=d.z;o->uid=uidSeq++;o->color=d.color;o->ivars=d.ivars;o->visible=d.visible;
        if (!d.particles.empty()) {particleProps[d.type]=d.particles;o->visible=false;}
        if (!d.anim.empty()) {o->setAnim(d.anim);o->setFrame(d.frame);o->playing=d.playing;}
        if (o->frame()) {o->sx=d.w/o->frame()->w;o->sy=d.h/o->frame()->h;}
        if (d.physics.size()>=10) {
            if (d.type=="wall") {
                o->physics=d.physics;
                b2BodyDef def;def.position.Set((d.x+(.5f-d.ox)*d.w)*PHYS_SCALE,(d.y+(.5f-d.oy)*d.h)*PHYS_SCALE);
                def.angle=d.angle;def.userData=o;o->body=world.CreateBody(&def);
                b2PolygonShape shape;shape.SetAsBox(d.w*.5f*PHYS_SCALE,d.h*.5f*PHYS_SCALE);
                b2FixtureDef f;f.shape=&shape;f.density=d.physics[3];f.friction=d.physics[4];f.restitution=d.physics[5];
                o->body->CreateFixture(&f);o->dirty=false;
            } else createBody(*o,d.physics);
        }
        if (d.drag.size()>=2) o->dragEnabled=d.drag[1]!=0;
        if (d.sine.size()>=9 && d.sine[8]!=0 && d.sine[0]==1) {
            o->sinePeriod=d.sine[2];o->sineMag=d.sine[3];o->sineBase=o->y;
        }
        if (d.hasFont) {
            o->text=d.text;o->charset=d.charset;o->fontW=d.fontW;o->fontH=d.fontH;o->fontSpacing=d.fontSpacing;
            o->hAlign=d.hAlign;o->vAlign=d.vAlign;o->fontScale=d.h/d.fontH;o->fontBoxW=d.w;o->fontBoxH=d.h;
        }
        objects.push_back(o);byType[d.type].push_back(o);storage.push_back(std::move(ptr));
    }
    Obj* b=byName("bag");if (b) {b->setScale(.8f);b->setIV(1,0);b->child=byName("overlay");}
    Obj* overlay=byName("overlay");if (overlay) {overlay->setScale(.8f);overlay->visible=true;}
    if (byName("Sprite")) byName("Sprite")->setAnim("2");
    tweens.to(byName("ryo"),1740,2,15);tweens.to(byName("kita"),1740,2,15);
    flyerTimer=random(20,50);
}
Game::~Game() {world.SetContactListener(nullptr);}
float Game::random(float a,float b) {return a+std::generate_canonical<float,24>(rnd)*(b-a);}
int Game::choose(std::initializer_list<int> values) {
    std::uniform_int_distribution<size_t> distribution(0,values.size()-1);
    return *(values.begin()+distribution(rnd));
}
void Game::sound(int id,float db,float pan) {if (hooks.sound) hooks.sound(id,db,pan,false);}
void Game::vibrate(bool reward) {if (hooks.vibration) hooks.vibration(reward);}
Obj* Game::byName(const std::string& name) const {
    auto it=byType.find(name);return it==byType.end() || it->second.empty() ? nullptr:it->second.front();
}
const std::vector<Obj*>& Game::all(const std::string& name) const {
    static const std::vector<Obj*> empty;
    auto it=byType.find(name);return it==byType.end() ? empty:it->second;
}
const std::vector<Obj*>& Game::drawOrder() {
    if (orderDirty) {
        ordered=objects;
        std::sort(ordered.begin(),ordered.end(),[](Obj* a,Obj* b){
            if (a->layer!=b->layer) return a->layer<b->layer;
            if (a->z!=b->z) return a->z<b->z;
            return a->uid<b->uid;
        });
        orderDirty=false;
    }
    return ordered;
}
Obj* Game::create(const std::string& name,int layer,float x,float y,const std::string& anim) {
    auto found=data.types.find(name);if (found==data.types.end()) return nullptr;
    std::unique_ptr<Obj> ptr(new Obj());Obj* o=ptr.get();o->type=&found->second;o->uid=uidSeq++;
    o->layer=layer;o->x=x;o->y=y;o->z=int(std::count_if(objects.begin(),objects.end(),[layer](Obj* p){return p->layer==layer;}));
    o->setAnim(anim);objects.push_back(o);byType[name].push_back(o);storage.push_back(std::move(ptr));orderDirty=true;
    return o;
}
void Game::destroy(Obj* o) {
    if (!o || o->destroyed) return;
    assert(!world.IsLocked());o->destroyed=true;
    if (dragging==o) dragging=nullptr;
    if (o->body) {world.DestroyBody(o->body);o->body=nullptr;}
    tweens.kill(o);
    objects.erase(std::remove(objects.begin(),objects.end(),o),objects.end());
    auto& typeList=byType[o->type->name];typeList.erase(std::remove(typeList.begin(),typeList.end(),o),typeList.end());
    orderDirty=true;
}
void Game::createBody(Obj& o,const std::vector<float>& p) {
    o.physics=p;b2BodyDef def;
    def.type=p[0]!=0 ? b2_staticBody:b2_dynamicBody;def.position.Set(o.x*PHYS_SCALE,o.y*PHYS_SCALE);def.angle=o.angle;
    def.fixedRotation=p[2]!=0;def.linearDamping=p[6];def.angularDamping=p[7];def.bullet=p[8]!=0;def.userData=&o;
    o.body=world.CreateBody(&def);buildFixtures(o);o.body->SetActive(p[9]!=0);
}
void Game::buildFixtures(Obj& o) {
    assert(!world.IsLocked());
    for (b2Fixture* f=o.body->GetFixtureList();f;) {b2Fixture* next=f->GetNext();o.body->DestroyFixture(f);f=next;}
    const auto& p=o.physics;b2FixtureDef fd;fd.density=p[3];fd.friction=p[4];fd.restitution=p[5];
    float w=o.width(),h=o.height(),mx=o.mirrored ? -1.f:1.f;
    b2Vec2 center((.5f-o.ox)*w*PHYS_SCALE*mx,(.5f-o.oy)*h*PHYS_SCALE);
    const Frame* frame=o.frame();
    if (int(p[1])==2) {
        b2CircleShape shape;shape.m_radius=std::min(w,h)*.5f*PHYS_SCALE;shape.m_p=center;fd.shape=&shape;o.body->CreateFixture(&fd);
    } else if (int(p[1])==1 || !frame || frame->poly.size()<3) {
        b2PolygonShape shape;shape.SetAsBox(w*.5f*PHYS_SCALE,h*.5f*PHYS_SCALE,center,0);fd.shape=&shape;o.body->CreateFixture(&fd);
    } else {
        auto fixture=[&](const Polygon& poly,size_t start,size_t count) {
            b2Vec2 verts[b2_maxPolygonVertices];
            for (size_t i=0;i<count;++i) verts[i].Set(poly[start+i].x*w*PHYS_SCALE*mx,poly[start+i].y*h*PHYS_SCALE);
            float area=0;for (size_t i=0;i<count;++i) area+=b2Cross(verts[i],verts[(i+1)%count]);
            if (area<0) std::reverse(verts,verts+count);
            b2PolygonShape shape;shape.Set(verts,int(count));fd.shape=&shape;o.body->CreateFixture(&fd);
        };
        if (frame->poly.size()<=b2_maxPolygonVertices && convex(frame->poly)) fixture(frame->poly,0,frame->poly.size());
        else for (size_t i=0;i+2<frame->triangles.size();i+=3) fixture(frame->triangles,i,3);
    }
    o.dirty=false;o.body->ResetMassData();
}
void Game::BeginContact(b2Contact* c) {
    Obj* a=static_cast<Obj*>(c->GetFixtureA()->GetBody()->GetUserData());
    Obj* b=static_cast<Obj*>(c->GetFixtureB()->GetBody()->GetUserData());
    if (a && b) contacts.push_back(std::make_pair(a,b));
}
void Game::teleport(Obj* o,float x,float y) {
    if (!o || o->destroyed) return;
    o->x=x;o->y=y;
    if (o->body) {
        o->body->SetTransform(b2Vec2(x*PHYS_SCALE,y*PHYS_SCALE),o->angle);
        o->body->SetLinearVelocity(b2Vec2(0,0));o->body->SetAngularVelocity(0);o->body->SetAwake(true);
    }
}
void Game::kinematic(Obj* o,bool enabled) {
    if (!o || !o->body) return;
    if (!enabled) o->body->SetTransform(b2Vec2(o->x*PHYS_SCALE,o->y*PHYS_SCALE),o->angle);
    o->body->SetType(enabled ? b2_kinematicBody:b2_dynamicBody);
    o->body->SetLinearVelocity(b2Vec2(0,0));o->body->SetAngularVelocity(0);o->body->SetAwake(true);
}
void Game::beginDrag(Obj* o,float x,float y) {
    dragging=o;dragDX=o->x-x;dragDY=o->y-y;kinematic(o,true);o->bodyDrives=false;
    if (o->type->name=="p3") {
        if (o->iv(0)!=0 && byName("p2")) byName("p2")->setAnim("Animation 1",true);
        vibrate();sound(0,-20,0);
    }
    if (o==byName("bag")) sound(choose({3,6}),-5,random(-50,50));
}
void Game::touchDown(float x,float y,float headRadius) {
    if (dragging) return;
    const auto& candidates=drawOrder();
    for (auto it=candidates.rbegin();it!=candidates.rend();++it) {
        Obj* o=*it;
        if (o->visible && !o->destroyed && o->dragEnabled && o->contains(x,y)) {beginDrag(o,x,y);return;}
    }
    if (headRadius<=0 || x<0 || y<0 || x>1280 || y>1024) return;
    for (auto it=candidates.rbegin();it!=candidates.rend();++it) {
        Obj* o=*it;
        if (o->type->name=="p3" && o->iv(0)!=0 && o->visible && o->dragEnabled && !o->destroyed &&
            o->contains(x,y,std::min(96.f,headRadius))) {beginDrag(o,x,y);return;}
    }
}
void Game::touchMove(float x,float y) {
    if (dragging) {dragging->x=x+dragDX;dragging->y=y+dragDY;}
}
void Game::touchUp() {
    Obj* o=dragging;if (!o) return;dragging=nullptr;
    if (o->type->name=="p3" && o->iv(0)!=0) {
        grow();o->setIV(0,0);kinematic(o,false);
        if (o->body) {o->body->SetActive(true);teleport(o,o->x,o->y);}
    } else kinematic(o,false);
    o->bodyDrives=true;
}
void Game::grow() {
    std::uniform_int_distribution<int> pick(0,26);
    Obj* o=create("p3",3,461,227,pick(rnd)==26 ? "3":"1");
    if (!o) return;o->setIV(0,1);o->dragEnabled=true;createBody(*o,{0,0,0,1,.5f,0,0,0,0,0});
}
Obj* Game::spawnDebris(float x,float y,bool gold) {
    Obj* o=create("p3",3,x,y,gold ? "3":"1");
    if (o) {o->setIV(0,0);o->dragEnabled=true;o->setFrame(int(o->anim->frames.size())-1);createBody(*o,{0,0,0,1,.5f,0,0,0,0,1});}
    return o;
}
void Game::spawnParticles(const std::string& name,Obj* at,int layer,float angle) {
    auto t=data.types.find(name);auto p=particleProps.find(name);
    if (!at || t==data.types.end() || p==particleProps.end() || p->second.size()<20) return;
    Emitter e;e.type=&t->second;e.params=p->second;e.layer=layer;
    Point pos=at->imagePoint(0);const auto& v=e.params;
    float rad=std::isnan(angle) ? at->angle:angle*PI/180;
    e.particles.reserve(size_t(v[0]));
    for (int i=0;i<int(v[0]);++i) {
        Particle q;q.x=pos.x+random(-.5f,.5f)*v[8];q.y=pos.y+random(-.5f,.5f)*v[9];
        q.speed=v[4]+random(-.5f,.5f)*v[10];q.angle=rad+random(-.5f,.5f)*v[1]*PI/180;
        q.opacity=v[6]/100;q.size=v[5]+random(-.5f,.5f)*v[11];q.grow=v[7]+random(-.5f,.5f)*v[12];
        e.particles.push_back(q);
    }
    emitters.push_back(std::move(e));
}
void Game::particlesTick(float dt) {
    for (Emitter& e:emitters) {
        const auto& v=e.params;
        for (Particle& q:e.particles) {
            float dist=q.speed*dt;q.x+=std::cos(q.angle)*dist;q.y+=std::sin(q.angle)*dist+q.gs*dt;
            q.size+=q.grow*dt;q.speed+=v[13]*dt;q.gs+=v[14]*dt;q.age+=dt;
            if (v[15]!=0) q.angle+=random(-.5f,.5f)*v[15]*PI/180*dt;
            if (v[16]!=0) q.speed+=random(-.5f,.5f)*v[16]*dt;
            if (v[17]!=0) q.opacity=clamp(q.opacity+random(-.5f,.5f)*v[17]/100*dt,0,1);
        }
        e.particles.erase(std::remove_if(e.particles.begin(),e.particles.end(),[&](const Particle& q){
            return q.size<1 || (v[18]==2 ? q.speed<=0:q.age>=v[19]);
        }),e.particles.end());
    }
    emitters.erase(std::remove_if(emitters.begin(),emitters.end(),[](const Emitter& e){return e.particles.empty();}),emitters.end());
}
void Game::update(float dt) {
    if (!std::isfinite(dt) || dt<=0) dt=1.f/60;dt=std::min(.05f,dt);
    if (dragging && dragging->body && dragging->body->IsActive()) {
        b2Vec2 target(dragging->x*PHYS_SCALE,dragging->y*PHYS_SCALE);
        dragging->body->SetLinearVelocity((1/dt)*(target-dragging->body->GetPosition()));
    }
    for (Obj* o:objects) if (o->body && o->dirty && o->type->name!="wall") buildFixtures(*o);
    world.Step(dt,8,3);
    for (Obj* o:objects) if (o->body && o->body->IsActive() && o->body->GetType()==b2_dynamicBody && o->bodyDrives) {
        o->x=o->body->GetPosition().x/PHYS_SCALE;o->y=o->body->GetPosition().y/PHYS_SCALE;o->angle=o->body->GetAngle();
    }
    tweens.tick(dt);sched.tick(dt,tweens);
    for (Obj* o:objects) {
        o->animate(dt);
        if (o->sinePeriod>0) {o->sineT+=dt;o->y=o->sineBase+std::sin(o->sineT/o->sinePeriod*2*PI)*o->sineMag;}
    }
    particlesTick(dt);
    for (auto pair:contacts) if (!pair.first->destroyed && !pair.second->destroyed) contact(pair.first,pair.second);
    contacts.clear();tickLogic(dt);
    for (Obj* o:objects) if (o->child) {
        Obj* c=o->child;c->x=o->x;c->y=o->y;c->angle=o->angle;c->sx=o->sx;c->sy=o->sy;
    }
    // No delayed actions reference collectible p3 instances. Reclaim only after all contacts.
    storage.erase(std::remove_if(storage.begin(),storage.end(),[](const std::unique_ptr<Obj>& p){return p->destroyed;}),storage.end());
}
void Game::contact(Obj* a,Obj* b) {
    Obj* bag=byName("bag");Obj* p3=a==bag ? b:b==bag ? a:nullptr;
    if (!bag || !p3 || p3->type->name!="p3" || p3->destroyed || bag->fi!=0 || p3->iv(0)!=0 || !p3->anim) return;
    bool gold=p3->anim->name=="3";
    if (!gold && p3->anim->name!="1") return;
    destroy(p3);bag->setIV(0,bag->iv(0)+1);
    if (gold) {bag->setIV(1,1);bag->setIV(2,bag->iv(2)+1);}
    bag->setScale(.9f+bag->iv(0)*.03f);vibrate();sound(choose({4,3,6}),-5,random(-50,50));
    if (bag->iv(0)==10) sound(4,-5,0);
}
void Game::tickLogic(float dt) {
    Obj* b=byName("bag");if (!b) return;
    int count=int(b->iv(0)),goldCount=int(b->iv(2));bool n=b->iv(1)!=0,bb=b->iv(3)!=0;
    Obj* r=byName("ryo"),*k=byName("kita"),*overlay=byName("overlay"),*mouth=byName("Sprite");
    if (count>=5 && goldCount<=1) {tweens.to(r,1320,2,15);tweens.to(k,1740,2,15);}
    if (count>=5 && goldCount>=2) {tweens.to(r,bb ? 1320:1740,2,15);tweens.to(k,bb ? 1740:1346,2,15);}
    b->setFrame(count>=10 ? (n ? 2:1):0);
    if (overlay) overlay->visible=count<10;
    if (mouth) mouth->setAnim(count>=10 ? "1":"2");
    bool on=b->onScreen(data.w,data.h);
    if (!on && !bagOffscreenWas) {
        bagOffscreenWas=true;
        if (dragging==b) {dragging=nullptr;kinematic(b,false);b->bodyDrives=true;}
        b->dragEnabled=false;teleport(b,940,-200);
        sched.seq({Step::wait(1),Step::act([b](){b->dragEnabled=true;})});
    } else if (on) bagOffscreenWas=false;
    if (dragging==b) {
        if (count>=10 && !n && r) r->setFrame(1);
        if (count>=10 && n) {if (r) r->setFrame(4);if (k) k->setFrame(1);}
    } else if (count>=3) {if (r) r->setFrame(0);if (k) k->setFrame(0);}
    feeding();
    if (flyerTimer>=0) {
        flyerTimer-=dt;
        if (flyerTimer<0) {
            groupS=true;randV=random(0,100);flyerTimer=random(180,1200);
            startFlyer(randV<=25 ? 1:randV<=50 ? 2:randV<=75 ? 3:4);
        }
    }
    Obj* s5=byName("Sprite5"),*s9=byName("Sprite9");
    if (groupS && s5 && s9 && !smallBusy && overlap(*s5,*s9)) {
        smallBusy=true;teleport(s5,1182,-1109);s9->setFrame(1);spawnParticles("particle6",s9,5);
        sound(10,-15,random(-50,50));sched.signal("Z");
        sched.seq({Step::wait(2),Step::act([this,s5,s9](){
            s9->setFrame(0);teleport(s5,1182,-633);smallBusy=false;
        })});
    }
    for (int m:{10,50,100,500,1000,5000}) if (score==m && std::find(milestones.begin(),milestones.end(),m)==milestones.end()) {
        milestones.push_back(m);milestone(m);
    }
    const auto& ahoges=all("p3");
    for (int i=int(ahoges.size())-1;i>=0;--i) {
        Obj* o=ahoges[size_t(i)];
        if (!o->onScreen(data.w,data.h)) {if (o->iv(0)!=0) {grow();o->setIV(0,0);}destroy(o);}
    }
}
void Game::feedReset(Obj* b) {
    ++score;if (byName("font")) byName("font")->text=std::to_string(score);
    if (dragging==b) {dragging=nullptr;kinematic(b,false);b->bodyDrives=true;}
    b->dragEnabled=false;b->setScale(.8f);teleport(b,940,-200);
    Obj* overlay=byName("overlay");if (overlay) {overlay->setScale(.8f);overlay->visible=true;}
    ran=random(0,100);b->setIV(3,ran<=30 ? 1:0);b->setIV(0,0);b->setIV(2,0);
}
void Game::girlIdle(Obj* b,Obj* girl) {
    girl->setFrame(0);tweens.to(girl,1740,2,14);b->dragEnabled=true;
}
void Game::feedTail(Obj* b,Obj* girl) {
    sched.seq({Step::wait(1),Step::waitTween(b),Step::wait(1.5f),Step::act([this,b,girl](){girlIdle(b,girl);})});
}
void Game::feeding() {
    Obj* b=byName("bag");if (!b || b->iv(0)<10 || dragging!=b) return;
    Obj* r=byName("ryo"),*k=byName("kita"),*girl=nullptr;
    bool n=b->iv(1)!=0,bb=b->iv(3)!=0,max=false;int gold=int(b->iv(2));
    if (r && overlap(*b,*r)) {
        if (!n) {girl=r;r->setFrame(2);spawnParticles("particle1",r,3);sound(choose({1,2,2,2,2}),-10,random(-50,50));}
        else if (gold<=5) {girl=r;r->setFrame(3);spawnParticles("particle2",r,3);sound(5,-15,random(-50,50));}
        else if (bb) {girl=r;r->setFrame(5);spawnParticles("particle13",r,3);sound(5,-15,random(-50,50));max=gold>=10;}
    }
    if (!girl && k && n && overlap(*b,*k)) {
        if (gold<=5) {girl=k;k->setFrame(2);spawnParticles("particle3",k,3);spawnParticles("particle4",k,3);sound(9,-10,random(-50,50));}
        else if (!bb) {girl=k;k->setFrame(3);spawnParticles("particle5",k,3);sound(8,-10,random(-50,50));max=gold>=10;}
    }
    if (!girl) return;
    if (n) b->setIV(1,0);
    Obj* s7=byName("Sprite7");if (max && s7) s7->setAnim("1",true);
    feedReset(b);
    if (!max) {feedTail(b,girl);return;}
    tweens.to(s7,1162,1,15);
    sched.seq({Step::wait(1),Step::waitTween(s7 ? s7:b),Step::act([this,s7](){
        if (s7) {s7->setFrame(1);for (int p:{7,12,11,9,10,8}) spawnParticles("particle"+std::to_string(p),s7,5,234);}
        sound(7,-15,random(-50,50));
    }),Step::wait(1.5f),Step::act([this,s7](){tweens.to(s7,1515,2,14);}),Step::wait(1),
        Step::waitTween(s7 ? s7:b),Step::act([this,b,girl](){girlIdle(b,girl);})});
}
void Game::startFlyer(int direction) {
    Obj* s=byName("Sprite9");if (!s || direction<1 || direction>4) return;
    float x=0,y=0,ex=0,ey=0,angle=0;
    switch(direction) {
        case 1:x=-544;y=333;ex=-186;ey=333;angle=46;break;
        case 2:x=1557;y=1544;ex=1228;ey=1081;angle=341;break;
        case 3:x=-154;y=1578;ex=-30;ey=1050;angle=10;break;
        case 4:x=1520;y=-544;ex=1201;ey=-81;angle=217;break;
    }
    s->mirrored=direction==2 || direction==4;s->x=x;s->y=y;s->angle=angle*PI/180;
    if (direction==1) tweens.to(s,ex,2,16);else tweens.toXY(s,ex,ey,2,16);
    sched.seq({Step::waitSignal("Z"),Step::wait(.5f),Step::act([this,s,x,y,direction](){
        if (direction==1) tweens.to(s,x,2,16);else tweens.toXY(s,x,y,2,16);
    }),Step::act([this](){groupS=false;})});
}
void Game::milestone(int m) {
    Obj* s=byName("Sprite7");if (!s) return;s->setAnim("2");vibrate(true);sound(11,-5,random(-50,50));
    tweens.to(s,1000,1,15);
    sched.seq({Step::waitTween(s),Step::wait(1),Step::act([this,s](){tweens.to(s,1515,2,14);})});
    if (m>=50) flyerTimer=10;
}
}
