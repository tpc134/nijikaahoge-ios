#pragma once
#include <atomic>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <utility>

namespace ah {
class AudioMixer {
    struct Command { int clip; float left,right; bool loop; };
    struct Voice {
        int clip=-1;
        size_t frame=0;
        float left=0,right=0;
        bool loop=false;
        uint32_t serial=0;
    };
    std::array<std::vector<int16_t>,12> clips;
    std::array<Command,256> commands;
    std::atomic<uint32_t> writeIndex,readIndex;
    std::array<Voice,12> voices;
    uint32_t serial=0;
public:
    AudioMixer():writeIndex(0),readIndex(0){}
    // Load or clear only with the AudioUnit stopped; callback never allocates or locks.
    void setClip(int id,std::vector<int16_t> pcm) { clips.at(id)=std::move(pcm); }
    bool play(int id,float db,float pan,bool loop=false) {
        if (id<0 || id>=12 || clips[id].empty()) return false;
        uint32_t w=writeIndex.load(std::memory_order_relaxed),r=readIndex.load(std::memory_order_acquire);
        if (w-r>=commands.size()) return false;
        float gain=std::min(1.f,std::pow(10.f,db/20)),p=std::max(-1.f,std::min(1.f,pan/100));
        Command c;c.clip=id;c.left=gain*(p>0 ? 1-p:1);c.right=gain*(p<0 ? 1+p:1);c.loop=loop;
        commands[w%commands.size()]=c;writeIndex.store(w+1,std::memory_order_release);return true;
    }
    void stop() {
        for (Voice& v:voices) v.clip=-1;
        readIndex.store(writeIndex.load(std::memory_order_acquire),std::memory_order_release);
    }
    void render(int16_t* stereo,size_t frames) {
        uint32_t r=readIndex.load(std::memory_order_relaxed),w=writeIndex.load(std::memory_order_acquire);
        while (r!=w) {
            Command c=commands[r%commands.size()];Voice* target=nullptr;
            for (Voice& v:voices) if (v.clip<0) {target=&v;break;}
            if (!target) {
                target=&voices[0];
                for (Voice& v:voices) if (int32_t(v.serial-target->serial)<0) target=&v;
            }
            target->clip=c.clip;target->frame=0;target->left=c.left;target->right=c.right;target->loop=c.loop;target->serial=++serial;
            ++r;
        }
        readIndex.store(r,std::memory_order_release);
        for (size_t f=0;f<frames;++f) {
            int32_t l=0,right=0;
            for (Voice& v:voices) if (v.clip>=0) {
                const auto& clip=clips[v.clip];
                if (v.frame>=clip.size()/2) {
                    if (v.loop) v.frame=0;else {v.clip=-1;continue;}
                }
                l+=int32_t(clip[v.frame*2]*v.left);right+=int32_t(clip[v.frame*2+1]*v.right);++v.frame;
            }
            stereo[f*2]=int16_t(std::max(-32768,std::min(32767,l)));
            stereo[f*2+1]=int16_t(std::max(-32768,std::min(32767,right)));
        }
    }
};
}
