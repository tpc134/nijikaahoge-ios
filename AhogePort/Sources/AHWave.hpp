#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace ah {
inline uint32_t wave32(const uint8_t* p) { return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24; }
inline uint16_t wave16(const uint8_t* p) { return uint16_t(p[0])|uint16_t(p[1])<<8; }
inline bool loadWave(const uint8_t* bytes,size_t size,std::vector<int16_t>& result) {
    result.clear();
    if (!bytes || size<44 || std::memcmp(bytes,"RIFF",4) || std::memcmp(bytes+8,"WAVE",4)) return false;
    size_t end=wave32(bytes+4);
    if (end<36 || end>size-8) return false;
    end+=8;
    unsigned channels=0;const uint8_t* pcm=nullptr;size_t length=0;
    for (size_t offset=12;offset<end;) {
        if (end-offset<8) return false;
        size_t chunk=wave32(bytes+offset+4),start=offset+8;
        if (chunk>end-start) return false;
        if (!std::memcmp(bytes+offset,"fmt ",4)) {
            if (chunk<16 || wave16(bytes+start)!=1 || wave32(bytes+start+4)!=48000 || wave16(bytes+start+14)!=16) return false;
            channels=wave16(bytes+start+2);
            if ((channels!=1 && channels!=2) || wave16(bytes+start+12)!=channels*2 || wave32(bytes+start+8)!=48000*channels*2) return false;
        }
        if (!std::memcmp(bytes+offset,"data",4)) { pcm=bytes+start;length=chunk; }
        offset=start+chunk;
        if (chunk&1) { if (offset==end) return false; ++offset; }
    }
    if (!pcm || !channels || !length || length%(channels*2)) return false;
    size_t frames=length/(channels*2);result.resize(frames*2);
    for (size_t f=0;f<frames;++f) {
        result[f*2]=int16_t(wave16(pcm+f*channels*2));
        result[f*2+1]=channels==1 ? result[f*2]:int16_t(wave16(pcm+f*channels*2+2));
    }
    return true;
}
}
