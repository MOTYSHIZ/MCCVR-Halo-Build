#include "haloce_classic_view_pair.h"
#include <cstdio>
#include <limits>

using namespace halo_ce;
int main()
{
    int failures=0;
    const auto check=[&](bool ok,const char* message) {
        if (!ok) { std::fprintf(stderr,"CE Classic: %s\n",message); ++failures; }
    };
    const auto near=[](float a,float b) { return std::fabs(a-b)<0.00001f; };
    Window source{};
    source.player=0; source.reserved=0x91;
    source.render.position={12,34,56};
    source.render.forward={1,0,0}; source.render.up={0,0,1};
    source.render.viewport=source.render.window={0,0,1080,1920};
    source.render.verticalFov=1.2f;
    source.render.nearPlane=0.01f; source.render.farPlane=1000;
    source.render.nativeFlags=0x55aa;
    source.render.nativePlane[3]=43;
    source.raster=source.render;
    // The native alternate-camera path may give raster and visibility distinct
    // stock positions. Each receives the same tracked displacement from its
    // own source; these bytes must never be replaced with an assumed alias.
    source.raster.position.x+=0.125f;
    source.raster.nativeFlags=0xa55a;
    Tracking tracking{};
    tracking.generation=4; tracking.spaceEpoch=3; tracking.serial=77;
    tracking.headPosition={0.2f,0.3f,-0.4f};
    tracking.eyes[0].offset={-0.032f,0,0};
    tracking.eyes[1].offset={0.032f,0,0};
    for (auto& eye:tracking.eyes)
    {
        eye.fov[0]=-0.9f; eye.fov[1]=1.0f;
        eye.fov[2]=0.85f; eye.fov[3]=-0.95f;
    }
    Reference reference{}; reference.generation=4; reference.spaceEpoch=3;
    ClassicViewPair pair{};
    check(StageClassicViewPair(source,tracking,reference,2,1.0f/3.048f,true,pair)==
        ClassicPairResult::Ready,"both eyes and both native cameras stage");
    check(near(pair.eyes[0].raster.position.x-pair.eyes[0].render.position.x,0.125f)&&
        near(pair.eyes[1].raster.position.x-pair.eyes[1].render.position.x,0.125f),
        "alternate native camera relation is preserved in each eye");
    check(near(pair.eyes[0].render.position.y-pair.eyes[1].render.position.y,
        0.064f/3.048f),"native binocular separation uses CE unit conversion once");
    for (const auto& eye:pair.eyes)
        check(eye.reserved==0x91&&eye.render.nativeFlags==0x55aa&&
            eye.raster.nativeFlags==0xa55a&&eye.render.nativePlane[3]==43&&
            Same(eye.render.viewport,source.render.viewport)&&
            eye.render.nearPlane==source.render.nearPlane&&
            eye.raster.farPlane==source.raster.farPlane,
            "per-eye staging preserves opaque fields, clip and raster");
    check(ClassicPairCurrent(pair,source,4,3,77,2),"exact native frame is current");
    check(!ClassicPairCurrent(pair,source,4,3,77,3)&&
        !ClassicPairCurrent(pair,source,4,4,77,2)&&
        !ClassicPairCurrent(pair,source,5,3,77,2)&&
        !ClassicPairCurrent(pair,source,4,3,78,2),
        "renderer switch, reference-space reset, title change and serial revoke pair");
    Window changed=source; changed.raster.position.z+=1;
    check(!ClassicPairCurrent(pair,changed,4,3,77,2),"stock native motion revokes pair");
    const auto preserved=pair;
    tracking.eyes[1].orientation.w=std::numeric_limits<float>::quiet_NaN();
    check(StageClassicViewPair(source,tracking,reference,2,0.33f,true,pair)==
        ClassicPairResult::InvalidEye&&std::memcmp(&pair,&preserved,sizeof(pair))==0,
        "bad second eye never publishes a good first eye alone");
    tracking=preserved.tracking;
    changed=source; changed.isUi=1;
    check(StageClassicViewPair(changed,tracking,reference,2,0.33f,true,pair)==
        ClassicPairResult::InvalidWindow,"UI cannot take primary stereo ownership");
    changed=source; changed.player=1;
    check(StageClassicViewPair(changed,tracking,reference,2,0.33f,true,pair)==
        ClassicPairResult::InvalidWindow,"another native player cannot be overwritten");
    check(StageClassicViewPair(source,tracking,reference,0,0.33f,true,pair)==
        ClassicPairResult::InvalidRendererEpoch,"uninitialized graphics mode rejected");
    check(StageClassicViewPair(source,tracking,reference,2,0.33f,false,pair)==
        ClassicPairResult::Ready&&near(pair.eyes[0].render.position.x,source.render.position.x)&&
        near(pair.eyes[0].render.position.z,source.render.position.z),
        "positional toggle leaves binocular eye offsets active");
    return failures?1:0;
}
