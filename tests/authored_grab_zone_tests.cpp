#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../src/common/authored_grab_zone.h"
#include <cassert>
#include <limits>
int main(){
    using namespace authored_grab;
    const Key pistol{1,2,3,4},rifle{1,2,3,5};
    const Point rear{-.025f,0,.01f},front{0,0,-.35f};
    assert(Hit(rear,{0,0,0}));assert(!Hit(front,{0,0,0}));assert(Hit(front,{.02f,0,-.34f}));
    assert(!Hit(rear,{0,0,-.6f}));assert(!Hit(rear,{std::numeric_limits<float>::quiet_NaN(),0,0}));
    Learner learner;assert(!learner.Observe(pistol,rear,100));
    for(uint64_t t=200;t<600;t+=100)assert(!learner.Observe(pistol,rear,t));
    assert(learner.Observe(pistol,rear,600));
    for(uint64_t t=700;t<=1600;t+=100)assert(learner.Observe(pistol,rear,t));
    assert(!learner.Observe(pistol,front,1700)); // reload movement revokes readiness
    assert(!learner.Observe(pistol,rear,1800));
    for(uint64_t t=1900;t<2300;t+=100)assert(!learner.Observe(pistol,rear,t));
    assert(learner.Observe(pistol,rear,2300)); // never freezes the reload pose
    assert(!learner.Observe(rifle,front,2400));
    for(uint64_t t=2500;t<2900;t+=100)assert(!learner.Observe(rifle,front,t));
    assert(learner.Observe(rifle,front,2900)); // replacement also waits exactly 500ms
    assert(!learner.Observe(rifle,front,3200)); // tracking gap restarts learning
    for(uint64_t t=3300;t<3700;t+=100)assert(!learner.Observe(rifle,front,t));
    assert(learner.Observe(rifle,front,3700));
    Publication pub;Point out{};
    pub.Refresh(pistol,1900);
    assert(!pub.Owns(pistol)&&!pub.Read(pistol,1900,out)); // first legacy grip cannot poison acquisition
    pub.Publish(pistol,rear,2000);
    assert(pub.Read(pistol,2050,out));assert(out==rear);
    assert(!pub.Read(rifle,2050,out));assert(!pub.Read(pistol,2101,out));assert(!pub.Read(pistol,1999,out));
    for(int i=0;i<20;++i){assert(!Hit(rear,{0,0,-.6f}));assert(pub.Read(pistol,2050,out));}
    assert(Hit(out,{0,0,0})); // an outside attempt never poisons the next attempt
    assert(pub.Owns(pistol)&&!pub.Owns(rifle));
    pub.Refresh(pistol,2200); // attachment heartbeat preserves learned zone
    assert(pub.Read(pistol,2250,out)&&out==rear);
    pub.Publish(rifle,front,2300,false);
    assert(pub.Owns(rifle)&&!pub.Read(rifle,2300,out)); // pending never inherits old gun
    pub.Publish(rifle,front,2400,true);assert(pub.Read(rifle,2400,out)&&out==front);
    pub.Refresh(pistol,2450); // another owner cannot steal or refresh the current endpoint
    assert(pub.Owns(rifle)&&!pub.Owns(pistol));
    assert(!pub.Read(rifle,2501,out));
    for(uint64_t t=2500;t<=3000;t+=100) {
        pub.Refresh(rifle,t);
        assert(pub.Read(rifle,t,out)&&out==front); // repeated grip/release cycles retain the point
    }
    assert(Hit(front,{0,-.16f,-.35f}));assert(!Hit(front,{0,-.18f,-.35f}));
    assert(!Hit(front,{0,.13f,-.35f})); // upward limit unchanged
    assert(ShortHit({0,-.18f,0}));assert(!ShortHit({0,-.20f,0}));
    assert(!ShortHit({0,.15f,0}));
    assert(Short(rear)&&!Short(front));assert(ShortHit({-.12f,0,0}));
    assert(!ShortHit({0,0,-.3f}));assert(!ShortHit({.3f,0,0}));
    pub.Publish(pistol,rear,0);assert(!pub.Read(pistol,2050,out));
    // Wider acquisition, still shorter than the old forward barrel interval.
    assert(Hit(front,{.10f,0,-.50f}));assert(Hit(front,{0,0,-.60f}));
    assert(!Hit(front,{0,0,-.70f}));assert(!Hit(front,{.13f,0,-.35f}));
    assert(ShortHit({-.20f,.12f,-.16f}));assert(!ShortHit({0,0,-.20f}));
    assert(CloseHandAuthority({-.22f,0,0})==0);
    assert(CloseHandAuthority({.22f,.05f,-.05f})==0);
    assert(CloseHandAuthority({0,0,-.15f})==0);
    assert(CloseHandAuthority({0,0,-.17f})==1);
    assert(CloseHandAuthority({0,0,-.23f})==1);
    assert(CloseHandAuthority({0,0,-.35f})==1);
}
