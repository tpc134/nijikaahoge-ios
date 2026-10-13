#include "AHCore.hpp"
namespace ah {
static float ease(int idx,float x) {
    x=clamp(x,0,1);
    if (idx==14) return 1-std::cos(x*PI/2);
    if (idx==15) return std::sin(x*PI/2);
    if (idx==16) return -(std::cos(x*PI)-1)/2;
    return x;
}
void Tweens::kill(Obj* obj) {
    list.erase(std::remove_if(list.begin(),list.end(),[obj](const Tween& t){return t.obj==obj;}),list.end());
}
void Tweens::to(Obj* obj,float x,float duration,int curve) {
    if (!obj || obj->destroyed) return;
    kill(obj);Tween t;t.obj=obj;t.sx=obj->x;t.sy=obj->y;t.ex=x;t.ey=obj->y;t.duration=duration;t.ease=curve;list.push_back(t);
}
void Tweens::toXY(Obj* obj,float x,float y,float duration,int curve) {
    to(obj,x,duration,curve);if (obj && !list.empty()) { list.back().ey=y;list.back().moveY=true; }
}
bool Tweens::busy(Obj* obj) const {
    for (const Tween& t:list) if (t.obj==obj && t.t<t.duration) return true;
    return false;
}
void Tweens::tick(float dt) {
    for (Tween& t:list) {
        t.t+=dt;float k=ease(t.ease,t.duration>0 ? t.t/t.duration:1);
        t.obj->x=t.sx+(t.ex-t.sx)*k;
        if (t.moveY) t.obj->y=t.sy+(t.ey-t.sy)*k;
    }
    list.erase(std::remove_if(list.begin(),list.end(),[](const Tween& t){return t.t>=t.duration;}),list.end());
}
void Scheduler::progress(const std::shared_ptr<Seq>& s) {
    s->wait=0;s->signal.clear();s->tween=nullptr;
    while (s->i<s->steps.size()) {
        Step& p=s->steps[s->i++];
        switch(p.kind) {
            case Step::Action: p.action();break;
            case Step::Delay: if (p.seconds>0) {s->wait=p.seconds;return;} break;
            case Step::Signal: s->signal=p.signal;return;
            case Step::TweenWait: s->tween=p.object;return;
        }
    }
    s->dead=true;
}
void Scheduler::seq(std::initializer_list<Step> steps) {
    auto s=std::make_shared<Seq>();s->steps=steps;list.push_back(s);progress(s);
}
void Scheduler::signal(const std::string& name) {
    auto snapshot=list;
    for (const auto& s:snapshot) if (!s->dead && s->signal==name) progress(s);
}
void Scheduler::tick(float dt,const Tweens& tweens) {
    // Actions can append sequences; hold stable sequence references while advancing.
    size_t n=list.size();
    for (size_t i=0;i<n;++i) {
        auto s=list[i];
        if (s->dead) continue;
        if (s->wait>0) { s->wait-=dt;if (s->wait<=0) progress(s); }
        else if (s->tween && !tweens.busy(s->tween)) progress(s);
    }
    list.erase(std::remove_if(list.begin(),list.end(),[](const std::shared_ptr<Seq>& s){return s->dead;}),list.end());
}
float Emitter::opacity(const Particle& q) const {
    return params[18]==0 ? q.opacity*(1-q.age/params[19]):q.opacity;
}
}
