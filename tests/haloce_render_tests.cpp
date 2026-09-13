#include "haloce_anniversary_logic.h"
#include <cstdio>
#include <cstring>
#include <limits>

using namespace halo_ce;
int main()
{
    int failures=0;
    const auto check=[&](bool ok,const char* what) {
        if (!ok) { std::fprintf(stderr,"CE: %s\n",what); ++failures; }
    };
    const auto near=[](float a,float b) { return std::fabs(a-b)<0.00001f; };
    Camera stock{};
    stock.position={10,20,30}; stock.forward={1,0,0}; stock.up={0,0,1};
    stock.viewport=stock.window={0,0,1000,1000};
    stock.nearPlane=0.01f; stock.farPlane=1000; stock.verticalFov=1.2f;
    stock.nativeFlags=0x1234; stock.nativePlane[0]=42;
    Window window{}; window.player=0; window.render=window.raster=stock;
    check(ValidPrimary(window),"normal primary window admitted");
    window.isUi=1; check(!ValidPrimary(window),"UI window rejected");
    window.isUi=0; window.player=1; check(!ValidPrimary(window),"split-screen secondary rejected");
    window.player=0; ++window.raster.viewport.left;
    check(!ValidPrimary(window),"mismatched camera viewports rejected");
    Tracking tracking{}; tracking.serial=7; tracking.spaceEpoch=3; tracking.generation=2;
    tracking.eyes[0].offset={-0.032f,0,0}; tracking.eyes[1].offset={0.032f,0,0};
    for (Eye& eye:tracking.eyes) {
        eye.fov[0]=-0.85f; eye.fov[1]=0.95f; eye.fov[2]=0.8f; eye.fov[3]=-0.9f;
    }
    Reference reference{}; reference.spaceEpoch=3; reference.generation=2;
    Cover cover{};
    check(BuildCover(tracking,stock.viewport,cover),"asymmetric runtime FOV covered");
    check(near(cover.halfX,0.95f)&&near(cover.halfY,0.95f),"square raster covers widest eye edge");
    Camera left{},right{};
    check(BuildEye(stock,tracking,reference,0,0.33f,true,cover,left)&&
          BuildEye(stock,tracking,reference,1,0.33f,true,cover,right),"both eyes generated");
    check(near(left.position.y,20+0.032f*0.33f)&&
          near(right.position.y,20-0.032f*0.33f),"IPD follows CE forward/up basis");
    check(near(left.position.x,10)&&near(left.position.z,30),"IPD has no forward or vertical drift");
    check(near(left.forward.x,1)&&near(left.up.z,1),"identity tracking preserves native orientation");
    check(left.nativeFlags==stock.nativeFlags&&Same(left.viewport,stock.viewport)&&
          left.nearPlane==stock.nearPlane&&left.farPlane==stock.farPlane&&
          left.nativePlane[0]==42,"non-owned camera fields preserved");
    tracking.headPosition={0,1,-2};
    check(BuildEye(stock,tracking,reference,0,0.33f,true,cover,left)&&
          near(left.position.x,10.66f)&&near(left.position.z,30.33f),"metres translate in native axes");
    check(BuildEye(stock,tracking,reference,0,0.33f,false,cover,left)&&
          near(left.position.x,10)&&near(left.position.z,30),"positional off preserves binocular separation only");
    tracking.headPosition={};
    const float s=std::sqrt(0.5f);
    tracking.headOrientation=tracking.eyes[0].orientation={0,s,0,s};
    check(BuildEye(stock,tracking,reference,0,0.33f,true,cover,left)&&
          near(left.forward.x,0)&&near(left.forward.y,1),"90-degree physical head turn has correct sign");
    reference.orientation=tracking.headOrientation;
    check(BuildEye(stock,tracking,reference,0,0.33f,true,cover,left)&&
          near(left.forward.x,1)&&near(left.forward.y,0),"recenter removes initial head rotation");
    const Camera saved=left;
    ++tracking.generation;
    check(!BuildEye(stock,tracking,reference,0,0.33f,true,cover,left)&&
          std::memcmp(&left,&saved,sizeof(left))==0,"previous title generation reference rejected");
    --tracking.generation;
    tracking.spaceEpoch=4;
    check(!BuildEye(stock,tracking,reference,0,0.33f,true,cover,left)&&
          std::memcmp(&left,&saved,sizeof(left))==0,"space reset rejects without partial output");
    tracking.spaceEpoch=3;
    tracking.eyes[0].offset.x=std::numeric_limits<float>::quiet_NaN();
    check(!BuildEye(stock,tracking,reference,0,0.33f,true,cover,left),"NaN eye rejected");
    tracking.eyes[0].offset={0.6f,0,0};
    check(!BuildEye(stock,tracking,reference,0,0.33f,true,cover,left),"implausible eye offset rejected");
    check(!BuildEye(stock,tracking,reference,2,0.33f,true,cover,left),"invalid eye index rejected");
    check(!ExactPair(0,0,0)&&!ExactPair(7,7,6)&&ExactPair(7,7,7),"only complete current serial pair admitted");
    SaberPose saber{};
    check(BuildSaberPose(stock,{100,200,300},0.25f,saber),"verified CE to Saber bridge pose");
    check(near(saber.matrix[0],0)&&near(saber.matrix[1],0)&&near(saber.matrix[2],1)&&
          near(saber.matrix[4],0)&&near(saber.matrix[5],1)&&near(saber.matrix[6],0)&&
          near(saber.matrix[8],1)&&near(saber.matrix[9],0)&&near(saber.matrix[10],0),
          "Saber axes match CE bridge cross product and signed axis permutation");
    check(near(saber.matrix[12],130.73f)&&near(saber.matrix[13],291.44f)&&
          near(saber.matrix[14],239.04f)&&saber.matrix[15]==1,
          "bridge scale, world offset and forward bias applied once");
    const SaberPose savedSaber=saber;
    check(!BuildSaberPose(stock,{0,std::numeric_limits<float>::infinity(),0},0,saber)&&
          std::memcmp(&saber,&savedSaber,sizeof(saber))==0,
          "invalid bridge origin rejected without partial pose");
    SaberCamera saberStock{};
    BuildSaberPose(stock,{},0,saberStock.pose);
    saberStock.viewportWidth=saberStock.viewportHeight=1000;
    saberStock.nearPlane=stock.nearPlane*kSaberUnitsPerNativeUnit;
    saberStock.farPlane=stock.farPlane*kSaberUnitsPerNativeUnit;
    saberStock.verticalFovDegrees=stock.verticalFov*57.29577951308232f;
    saberStock.derived160[100]=0xab;
    Camera roundTrip{};
    check(NativeCameraFromSaber(saberStock,roundTrip)&&
          near(roundTrip.position.x,stock.position.x)&&
          near(roundTrip.position.y,stock.position.y)&&
          near(roundTrip.position.z,stock.position.z),"Saber camera round trip preserves position");
    tracking.headOrientation={}; tracking.eyes[0].orientation={};
    tracking.eyes[0].offset={-0.032f,0,0}; reference.orientation={};
    SaberCamera saberEye{};
    check(StageSaberEye(saberStock,tracking,reference,0,0.33f,true,saberEye,cover)&&
          saberEye.derived160[100]==0xab&&saberEye.nearPlane==saberStock.nearPlane&&
          saberEye.farPlane==saberStock.farPlane,
          "staged Anniversary eye preserves every non-owned field");
    check(near(saberEye.pose.matrix[14],-20*3.048f-0.032f*0.33f*3.048f),
          "Saber binocular translation uses independently verified scale and axes");
    const SaberCamera savedEye=saberEye;
    saberStock.viewportWidth=std::numeric_limits<float>::infinity();
    check(!StageSaberEye(saberStock,tracking,reference,0,0.33f,true,saberEye,cover)&&
          std::memcmp(&saberEye,&savedEye,sizeof(saberEye))==0,
          "invalid raster rejected before float-to-integer conversion or partial writes");
    return failures?1:0;
}
