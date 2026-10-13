#include "AHCore.hpp"
#include <limits>

namespace ah {
static float cross(Point a,Point b,Point c) {
    return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
}
Polygon clean(const Polygon& p) {
    Polygon out;
    for (Point q:p) {
        if (out.empty() || std::fabs(q.x-out.back().x)>1e-6f || std::fabs(q.y-out.back().y)>1e-6f)
            out.push_back(q);
    }
    if (out.size()>1 && std::fabs(out.front().x-out.back().x)<1e-6f && std::fabs(out.front().y-out.back().y)<1e-6f)
        out.pop_back();
    return out;
}
bool convex(const Polygon& p) {
    float sign=0;
    for (size_t i=0;i<p.size();++i) {
        float v=cross(p[i],p[(i+1)%p.size()],p[(i+2)%p.size()]);
        if (std::fabs(v)<1e-6f) continue;
        if (sign*v<0) return false;
        sign=v;
    }
    return sign!=0;
}
static bool inTri(Point p,Point a,Point b,Point c) {
    float x=cross(a,b,p), y=cross(b,c,p), z=cross(c,a,p);
    return !((x<0 || y<0 || z<0) && (x>0 || y>0 || z>0));
}
Polygon triangulate(const Polygon& p) {
    if (p.size()<=3) return p;
    float area=0;
    for (size_t i=0;i<p.size();++i) area+=p[i].x*p[(i+1)%p.size()].y-p[(i+1)%p.size()].x*p[i].y;
    std::vector<size_t> ids;
    for (size_t i=0;i<p.size();++i) ids.push_back(area>=0 ? i : p.size()-1-i);
    Polygon out;
    while (ids.size()>3) {
        bool clipped=false;
        for (size_t k=0;k<ids.size();++k) {
            size_t a=ids[(k+ids.size()-1)%ids.size()],b=ids[k],c=ids[(k+1)%ids.size()];
            if (cross(p[a],p[b],p[c])<=0) continue;
            bool ok=true;
            for (size_t m:ids) if (m!=a && m!=b && m!=c && inTri(p[m],p[a],p[b],p[c])) { ok=false; break; }
            if (ok) {
                out.push_back(p[a]); out.push_back(p[b]); out.push_back(p[c]);
                ids.erase(ids.begin()+k); clipped=true; break;
            }
        }
        if (!clipped) {
            for (size_t k=1;k+1<ids.size();++k) {
                out.push_back(p[ids[0]]);out.push_back(p[ids[k]]);out.push_back(p[ids[k+1]]);
            }
            return out;
        }
    }
    for (size_t i:ids) out.push_back(p[i]);
    return out;
}
const Frame* Obj::frame() const {
    if (!anim || anim->frames.empty()) return nullptr;
    return &anim->frames[std::max(0,std::min(fi,int(anim->frames.size())-1))];
}
float Obj::width() const { const Frame* f=frame(); return f ? f->w*sx : 0; }
float Obj::height() const { const Frame* f=frame(); return f ? f->h*sy : 0; }
void Obj::setAnim(const std::string& name,bool restart) {
    auto it=type->anims.find(name);
    if (it==type->anims.end()) return;
    if (anim!=&it->second || restart) {
        anim=&it->second; fi=0; animT=0; forward=true; repeats=0; playing=true; dirty=true;
        const Frame* f=frame(); if (f) { ox=f->ox; oy=f->oy; }
    }
}
void Obj::setFrame(int i) {
    int next=std::max(0,std::min(i,anim ? int(anim->frames.size())-1 : 0));
    if (next!=fi) dirty=true;
    fi=next;
    const Frame* f=frame(); if (f) { ox=f->ox;oy=f->oy; }
}
void Obj::animate(float dt) {
    if (!playing || !anim || anim->speed<=0 || anim->frames.size()<=1) return;
    animT+=dt;
    int before=fi,n=int(anim->frames.size()),guard=0;
    while (playing && guard++<100) {
        float duration=anim->frames[fi].dur/anim->speed;
        if (animT<duration) break;
        animT-=duration;
        fi+=forward ? 1:-1;
        if (fi>=n) {
            if (anim->pingPong) { forward=false; fi=n-2; }
            else if (anim->loop) fi=anim->repeatTo;
            else { ++repeats; if (repeats>=anim->repeatCount) { playing=false;fi=n-1; } else fi=anim->repeatTo; }
        }
        if (fi<0) {
            if (anim->pingPong) {
                fi=1;forward=true;
                if (!anim->loop && ++repeats>=anim->repeatCount) playing=false;
            } else fi=n-1;
        }
        fi=std::max(0,std::min(fi,n-1));
    }
    if (before!=fi) dirty=true;
    const Frame* f=frame(); if (f) { ox=f->ox;oy=f->oy; }
}
Point Obj::transform(Point p) const {
    float px=p.x*width()*(mirrored ? -1:1),py=p.y*height();
    float c=std::cos(angle),s=std::sin(angle);
    return Point(x+px*c-py*s,y+px*s+py*c);
}
Point Obj::imagePoint(size_t i) const {
    const Frame* f=frame();
    if (!f || i>=f->ips.size()) return Point(x,y);
    return transform(Point(f->ips[i].x-f->ox,f->ips[i].y-f->oy));
}
void Obj::geometry() const {
    std::array<float,8> state={{x,y,angle,sx,sy,ox,oy,mirrored?1.f:0.f}};
    const Frame* f=frame();
    if (cachedFrame==f && cached==state) return;
    cachedFrame=f;cached=state;
    const std::array<Point,4> corners={{Point(-ox,-oy),Point(1-ox,-oy),Point(1-ox,1-oy),Point(-ox,1-oy)}};
    box={{INFINITY,INFINITY,-INFINITY,-INFINITY}};
    for (Point p:corners) {
        Point q=transform(p);
        box[0]=std::min(box[0],q.x);box[1]=std::min(box[1],q.y);
        box[2]=std::max(box[2],q.x);box[3]=std::max(box[3],q.y);
    }
    const Polygon* source=(f && f->poly.size()>=3) ? &f->poly:nullptr;
    worldPoly.resize(source ? source->size():4);
    for (size_t i=0;i<worldPoly.size();++i) worldPoly[i]=transform(source ? (*source)[i]:corners[i]);
    if (f && f->poly.size()>=3) {
        worldTriangles.resize(f->triangles.size());
        for (size_t i=0;i<f->triangles.size();++i) worldTriangles[i]=transform(f->triangles[i]);
    } else {
        const int ids[]={0,1,2,0,2,3};
        worldTriangles.resize(6);
        for (int i=0;i<6;++i) worldTriangles[i]=transform(corners[ids[i]]);
    }
}
bool Obj::onScreen(float w,float h) const {
    if (!frame()) return true;
    const auto& b=bounds();return b[2]>=0 && b[0]<=w && b[3]>=0 && b[1]<=h;
}
bool Obj::contains(float px,float py,float padding) const {
    const Polygon& p=poly(); if (p.empty()) return false;
    bool inside=false;
    for (size_t i=0,j=p.size()-1;i<p.size();j=i++) {
        if ((p[i].y>py)!=(p[j].y>py) && px<(p[j].x-p[i].x)*(py-p[i].y)/(p[j].y-p[i].y)+p[i].x) inside=!inside;
    }
    if (inside || padding<=0) return inside;
    const auto& b=bounds();
    if (px<b[0]-padding || px>b[2]+padding || py<b[1]-padding || py>b[3]+padding) return false;
    for (size_t i=0,j=p.size()-1;i<p.size();j=i++) {
        float dx=p[i].x-p[j].x,dy=p[i].y-p[j].y,len=dx*dx+dy*dy;
        float t=len>0 ? clamp(((px-p[j].x)*dx+(py-p[j].y)*dy)/len,0,1):0;
        float ex=px-p[j].x-t*dx,ey=py-p[j].y-t*dy;
        if (ex*ex+ey*ey<=padding*padding) return true;
    }
    return false;
}
static bool projection(const Polygon& a,size_t ai,const Polygon& b,size_t bi) {
    for (int e=0;e<3;++e) {
        Point v=a[ai+e],w=a[ai+(e+1)%3];
        float nx=-(w.y-v.y),ny=w.x-v.x,minA=INFINITY,maxA=-INFINITY,minB=INFINITY,maxB=-INFINITY;
        for (int k=0;k<3;++k) {
            float av=a[ai+k].x*nx+a[ai+k].y*ny,bv=b[bi+k].x*nx+b[bi+k].y*ny;
            minA=std::min(minA,av);maxA=std::max(maxA,av);minB=std::min(minB,bv);maxB=std::max(maxB,bv);
        }
        if (maxA<minB || maxB<minA) return false;
    }
    return true;
}
bool overlap(const Obj& a,const Obj& b) {
    const auto& ab=a.bounds();const auto& bb=b.bounds();
    if (ab[2]<bb[0] || bb[2]<ab[0] || ab[3]<bb[1] || bb[3]<ab[1]) return false;
    const Polygon& at=a.triangles();const Polygon& bt=b.triangles();
    for (size_t i=0;i+2<at.size();i+=3) for (size_t j=0;j+2<bt.size();j+=3)
        if (projection(at,i,bt,j) && projection(bt,j,at,i)) return true;
    return false;
}
}
