#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
namespace authored_grab {
using Key=std::array<uint32_t,4>; // title, generation, unit, weapon
using Point=std::array<float,3>; // primary-controller local OpenXR metres
inline bool Valid(Point p){float n=0;for(float v:p){if(!std::isfinite(v))return false;n+=v*v;}return n<=1.0f;}
inline float Distance2(Point a,Point b){float n=0;for(int i=0;i<3;++i)n+=(a[i]-b[i])*(a[i]-b[i]);return n;}
// Continuously relearn after motion; never permanently cache an equip/reload pose.
struct Learner {
    Key key{};Point anchor{};uint64_t start{},quiet{},last{};bool active=false;
    bool Observe(Key k,Point p,uint64_t now){
        if(!Valid(p)){active=false;return false;}
        if(!active||key!=k||now<last||now-last>250){key=k;anchor=p;start=quiet=last=now;active=true;return false;}
        last=now;
        if(Distance2(anchor,p)>.0009f){anchor=p;quiet=now;}
        return now-start>=500&&now-quiet>=500;
    }
};
struct Publication {
    std::atomic<uint64_t> sequence{0},at{0};std::atomic<bool> calibrated{false};std::atomic<uint32_t> key[4]{};std::atomic<float> point[3]{};
    void Publish(Key k,Point p,uint64_t now,bool ready=true){
        auto seq=sequence.load();if((seq&1)||!sequence.compare_exchange_strong(seq,seq+1))return;
        bool same=true;for(int i=0;i<4;++i)same=same&&key[i].load()==k[i];
        if(!same)calibrated.store(false);
        for(int i=0;i<4;++i)key[i].store(k[i]);
        if(ready&&now&&Valid(p)){
            for(int i=0;i<3;++i)point[i].store(p[i]);
            calibrated.store(true);
        }
        at.store(now);sequence.store(seq+2);
    }
    // An attached frame can refresh an existing endpoint, never claim a new
    // owner before any valid geometry was observed for that owner.
    void Refresh(Key k,uint64_t now){
        auto seq=sequence.load();if((seq&1)||!sequence.compare_exchange_strong(seq,seq+1))return;
        bool same=true;for(int i=0;i<4;++i)same=same&&key[i].load()==k[i];
        if(same)at.store(now);
        sequence.store(seq+2);
    }
    bool Owns(Key k)const{
        const auto seq=sequence.load();if(!seq||(seq&1))return false;
        bool same=true;for(int i=0;i<4;++i)same=same&&key[i].load()==k[i];
        return same&&sequence.load()==seq;
    }
    bool Read(Key k,uint64_t now,Point& p)const{
        const auto seq=sequence.load();if(!seq||(seq&1))return false;
        const auto time=at.load();const bool ready=calibrated.load();bool matches=true;
        for(int i=0;i<4;++i)matches=matches&&key[i].load()==k[i];
        for(int i=0;i<3;++i)p[i]=point[i].load();
        return sequence.load()==seq&&ready&&matches&&time&&now>=time&&now-time<=100&&Valid(p);
    }
};
inline Publication publication;
inline std::atomic<float> lastX{0},lastY{0},lastZ{0};
inline std::atomic<uint64_t> hits{0},misses{0},fallbacks{0};
// Short authored separation permits a side-by-side support grip. Longer arms
// use a short longitudinal capsule; no weapon-specific calibration.
inline bool Short(Point centre){return Valid(centre)&&Distance2(centre,{0,0,0})<=.24f*.24f;}
inline bool ShortHit(Point p){return Valid(p)&&std::fabs(p[0])<=.22f&&p[1]>=-.19f&&p[1]<=.14f&&p[2]>=-.18f&&p[2]<=.16f;}
// Binary exclusion: outside the close-hand volume, keep the user's full
// configured influence. Physical grip separation ignores controller rotation.
inline float CloseHandAuthority(Point local){
    if(!Valid(local))return 0;
    const bool close=Distance2(local,{0,0,0})<=.16f*.16f;
    const bool lateral=std::fabs(local[0])<=.24f&&
        std::fabs(local[1])<=.14f&&std::fabs(local[2])<=.12f;
    return close||lateral?0.f:1.f;
}
inline bool Hit(Point centre,Point sample){
    if(!Valid(centre)||!Valid(sample))return false;
    // 30cm segment plus 12cm rounded ends: 54cm total, versus the old
    // 72cm forward interval. The segment stays centred on the authored grip.
    Point nearest=centre;
    // Extend only downward by 5cm; keep the top and lateral boundaries.
    nearest[1]+=std::fmax(-.05f,std::fmin(0.f,sample[1]-centre[1]));
    nearest[2]+=std::fmax(-.15f,std::fmin(.15f,sample[2]-centre[2]));
    return Distance2(nearest,sample)<.12f*.12f;
}
}
