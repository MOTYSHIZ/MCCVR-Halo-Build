#include <windows.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include "../src/dll/odst_finger_pose_backend.inl"
#include "odst_finger_bind_fixture.h"
#include "halo3_finger_bind_fixture.h"
namespace {
using namespace odst_fingers;
unsigned checks=0,failures=0;
void Check(bool value,const char* name){++checks;if(!value){++failures;std::printf("FAIL: %s\n",name);}}
bool Near(const Matrix& a,const Matrix& b) {
    const auto* x=reinterpret_cast<const float*>(&a);const auto* y=reinterpret_cast<const float*>(&b);
    for(unsigned i=0;i<sizeof(Matrix)/sizeof(float);++i)if(std::fabs(x[i]-y[i])>2e-5f)return false;
    return true;
}
void Build(Pose& source,Pose& inverse) {
    Pose bind{};Matrix world{};world.translation[0]=8;world.translation[1]=-3;world.translation[2]=1.8f;
    for(unsigned node=0;node<Count;++node) {
        Check(Parents[node]==OdstFpBindParents[node],"own official ODST parent graph");
        Matrix local{};std::memcpy(local.translation,OdstFpBindLocal[node],12);
        Check(Halo4QuaternionToBlamBasis(OdstFpBindLocal[node]+3,local.rotation),"own official ODST default quaternion");
        if(Parents[node]>=0)Check(Halo4ComposeFloatingTransforms(bind[Parents[node]],local,bind[node]),"own local bind graph composed");
        else bind[node]=local;
        Check(Halo4InvertFloatingTransform(bind[node],inverse[node]),"own model default inverse derived");
        Check(Halo4ComposeFloatingTransforms(world,bind[node],source[node]),"native-style committed world palette");
    }
}
}
namespace {
void BuildFrom(const float (*local)[7],Pose& source,Pose& inverse) {
    Pose bind{};Matrix world{};world.translation[0]=-2;world.translation[1]=5;world.translation[2]=0.6f;
    for(unsigned node=0;node<Count;++node) {
        Matrix l{};std::memcpy(l.translation,local[node],12);
        Check(Halo4QuaternionToBlamBasis(local[node]+3,l.rotation),"own default quaternion");
        if(Parents[node]>=0)Check(Halo4ComposeFloatingTransforms(bind[Parents[node]],l,bind[node]),"local bind graph composed");
        else bind[node]=l;
        Check(Halo4InvertFloatingTransform(bind[node],inverse[node]),"model default inverse derived");
        Check(Halo4ComposeFloatingTransforms(world,bind[node],source[node]),"committed world palette");
    }
}
int RunHalo3AndStowed() {
    using namespace odst_fingers;
    {
        // HALO 3: masterchief/fp, from its own H3EK export (tools/re/generate_halo3_finger_fixture.py).
        Check(std::memcmp(Halo3FpBindParents,OdstFpBindParents,sizeof(Halo3FpBindParents))==0&&
              std::memcmp(Halo3FpBindParents,Parents,sizeof(Parents))==0,"Halo 3's own parent graph is the shared 37-node one");
        Check(!std::strcmp(Halo3FpBindNames[5],"l_hand")&&!std::strcmp(Halo3FpBindNames[6],"r_hand")&&
              !std::strcmp(Halo3FpBindNames[Chains[0][0][0]],"l_index_low")&&!std::strcmp(Halo3FpBindNames[Chains[0][2][0]],"l_pinky_low")&&
              !std::strcmp(Halo3FpBindNames[Chains[0][3][2]],"l_ring_tip")&&!std::strcmp(Halo3FpBindNames[Chains[1][4][1]],"r_thumb_mid"),
              "the chain tables name Halo 3's own bones");
        unsigned differing=0,which=0;
        for(unsigned node=0;node<Count;++node) {
            float d=0;for(unsigned i=0;i<7;++i)d=std::fmax(d,std::fabs(Halo3FpBindLocal[node][i]-OdstFpBindLocal[node][i]));
            if(d>1e-5f){++differing;which=node;}
        }
        Check(differing==1&&which==16,"only r_thumb_low's bind differs from ODST's, so Halo 3 needs its own");
        AnatomicalPalmMarkers markers{};
        Check(PalmMarkers(504041493u,Count,markers)&&RigTitle(504041493u)==GameTitle::Halo3,"Halo 3's Chief rig is accepted");
        Check(!PalmMarkers(504041493u,31,markers)&&!PalmMarkers(269159697u,31,markers),"other Halo 3 rigs (31-node) are not");
        Check(RigTitle(286525724u)==GameTitle::Halo3ODST,"ODST stays ODST");
        Pose source{},inverse{};BuildFrom(Halo3FpBindLocal,source,inverse);
        for(unsigned side=0;side<2;++side) {
            finger_joint::Inventory inventory{};
            Check(Describe(504041493u,Count,side,inventory),"Halo 3 finger inventory");
            Request request{};request.checksum=504041493u;request.count=Count;
            Check(ConfigureFreeSupport(request,side==1,false,false,{true,0,0}),"Halo 3 free support hand");
            Pose open{};Check(Solve(source,inverse,request,open),"Halo 3 open-hand solve");
            for(unsigned node=0;node<Count;++node)Check(Near(open[node],source[node]),"zero curl is Halo 3's own bind");
            request.fingers[side]={true,1,1};Pose fist{};
            Check(Solve(source,inverse,request,fist),"Halo 3 fist");
            for(unsigned node=0;node<Count;++node)
                if(IsFinger(node,side))Check(!Near(fist[node],source[node]),"every Halo 3 finger joint responds");
                else Check(std::memcmp(&fist[node],&source[node],sizeof(Matrix))==0,"Halo 3 wrists, arms and the other hand untouched");
            auto palette=source;
            Check(Apply(palette.data(),inverse,request)==Result::Applied,"Halo 3 transaction applies");
        }
    }
    {
        // THE STOWED GUN: the gun hand's fingers are free too.
        Pose original{},inverse{};BuildFrom(OdstFpBindLocal,original,inverse);
        for(bool alignment:{false,true}) {
            const unsigned support=FingerAnatomicalSupportSide(alignment),primary=1u-support;
            Request request{};request.checksum=286525724u;request.count=Count;
            Check(ConfigureFreeSupport(request,alignment,false,false,{true,0,0}),"support free");
            Check(ConfigureStowedPrimary(request,alignment,{true,1,1})&&request.allowed[0]&&request.allowed[1],
                "the stowed gun hand joins the support hand");
            Pose both{};Check(Solve(original,inverse,request,both),"both hands solve");
            Check(Near(both[Chains[support][1][1]],original[Chains[support][1][1]]),"support hand open (its own input)");
            Check(!Near(both[Chains[primary][1][1]],original[Chains[primary][1][1]]),"gun hand closed (its own input)");
            const float point[5]{0,1,1,1,1};
            Check(SetSideCurls(request,primary,point)&&request.curlsValid[primary]&&!request.curlsValid[support],
                "a gesture shapes the gun hand only");
            Pose pointing{};Check(Solve(original,inverse,request,pointing),"pointing gun hand solves");
            Check(Near(pointing[Chains[primary][0][1]],original[Chains[primary][0][1]])&&
                  !Near(pointing[Chains[primary][1][1]],original[Chains[primary][1][1]]),"the gun hand points");
            // The support hand on the gun (two-handed): only the stowed gun hand is free.
            Request alone{};alone.checksum=286525724u;alone.count=Count;
            Check(!ConfigureFreeSupport(alone,alignment,false,true,{true,1,1}),"support attached keeps its grip");
            Check(ConfigureStowedPrimary(alone,alignment,{true,1,1})&&!alone.allowed[support]&&alone.allowed[primary],
                "the gun hand alone");
            auto palette=original;
            Check(Apply(palette.data(),inverse,alone)==Result::Applied,"gun hand alone applies");
            for(unsigned node=0;node<Count;++node)
                if(!IsFinger(node,primary))Check(std::memcmp(&palette[node],&original[node],sizeof(Matrix))==0,
                    "nothing but the gun hand's fingers is written");
            Request none{};none.checksum=286525724u;none.count=Count;
            Check(!ConfigureStowedPrimary(none,alignment,{})&&!none.allowed[0]&&!none.allowed[1],"no input, no gun hand");
            Check(!SetSideCurls(none,primary,point)&&!SetSideCurls(request,2,point),"curls need an allowed side");
        }
    }
    return 0;
}
}
int main() {
    using namespace odst_fingers;
    RunHalo3AndStowed();
    Pose original{},inverse{};Build(original,inverse);
    for(uint32_t checksum:{286525724u,403178001u})for(unsigned side=0;side<2;++side) {
        Request request{};request.checksum=checksum;request.count=Count;
        Check(ConfigureFreeSupport(request,side==1,false,false,{true,0,0}),"unoccupied support accepts controller input");
        Check(request.allowed[side]&&!request.allowed[1-side],"actual anatomical route chooses one support hand");
        Pose result{};Check(Solve(original,inverse,request,result),"own ODST open-hand solve");
        for(unsigned node=0;node<Count;++node)Check(Near(result[node],original[node]),"zero curl reconstructs original own bind without moving wrist");
        request.fingers[side]={true,0,1};Pose pointing{};
        Check(Solve(original,inverse,request,pointing),"pointing gesture opens index and closes other fingers");
        for(unsigned joint=0;joint<3;++joint)Check(Near(pointing[Chains[side][0][joint]],original[Chains[side][0][joint]]),"unpressed trigger keeps pointing index open");
        for(unsigned finger=1;finger<5;++finger)Check(!Near(pointing[Chains[side][finger][1]],original[Chains[side][finger][1]]),"grip flexes other authored fingers");
        request.fingers[side]={true,1,1};Pose fist{};
        Check(Solve(original,inverse,request,fist),"own ODST closed fist");
        for(unsigned node=0;node<Count;++node) {
            if(IsFinger(node,side))Check(!Near(fist[node],original[node]),"all free finger joints respond");
            else Check(std::memcmp(&fist[node],&original[node],sizeof(Matrix))==0,"primary fingers wrists arms and gun carriers stay byte-identical");
        }
        Pose repeated{};Check(Solve(fist,inverse,request,repeated),"repeat committed palette can reapply pose");
        Check(std::memcmp(&fist,&repeated,sizeof(fist))==0,"controller curl never accumulates across eyes or repeat calls");
        request.fingers[side]={true,0,0};Pose reopened{};
        Check(Solve(fist,inverse,request,reopened),"release opens previously curled hand");
        for(unsigned node=0;node<Count;++node)Check(Near(reopened[node],original[node]),"release returns own default finger shape");
        const auto untouched=request;
        Check(!ConfigureFreeSupport(request,side==1,true,false,{true,1,1})&&std::memcmp(&request,&untouched,sizeof(request))==0,"secondary weapon preserves authored support grip");
        Check(!ConfigureFreeSupport(request,side==1,false,true,{true,1,1})&&std::memcmp(&request,&untouched,sizeof(request))==0,"persistent/two-hand attachment preserves authored fingers");
        Check(!ConfigureFreeSupport(request,side==1,false,false,{})&&std::memcmp(&request,&untouched,sizeof(request))==0,"unavailable raw input preserves current native shape");
        {
            // A track's hand shape replaces trigger/grip with per-finger curls.
            Request shaped=request;shaped.fingers[side]={true,0,0};
            const float fistCurls[5]{1,1,1,1,1};
            Check(SetSupportCurls(shaped,fistCurls)&&shaped.curlsValid[side]&&!shaped.curlsValid[1-side],
                "hand shape curls go to the free support hand only");
            Pose shapedFist{};Check(Solve(original,inverse,shaped,shapedFist),"shaped fist solves");
            Check(std::memcmp(&shapedFist,&fist,sizeof(fist))==0,"all-closed shape equals the trigger-and-grip fist");
            const float openCurls[5]{0,0,0,0,0};
            Request open=request;open.fingers[side]={true,1,1};
            Check(SetSupportCurls(open,openCurls),"open shape accepted");
            Pose shapedOpen{};Check(Solve(original,inverse,open,shapedOpen),"shaped open hand solves");
            for(unsigned node=0;node<Count;++node)Check(Near(shapedOpen[node],original[node]),"zero shape is the bind hand whatever the controller says");
            const float indexOnly[5]{1,0,0,0,0};
            Request point=request;point.fingers[side]={true,0,0};
            Check(SetSupportCurls(point,indexOnly),"index-only shape accepted");
            Pose shapedIndex{};Check(Solve(original,inverse,point,shapedIndex),"index-only shape solves");
            Check(!Near(shapedIndex[Chains[side][0][1]],original[Chains[side][0][1]])&&
                  Near(shapedIndex[Chains[side][1][1]],original[Chains[side][1][1]]),"curl order: first value is the index finger");
            const float ringOnly[5]{0,0,1,0,0};
            Check(SetSupportCurls(point,ringOnly)&&Solve(original,inverse,point,shapedIndex),"ring-only shape solves");
            Check(!Near(shapedIndex[Chains[side][3][1]],original[Chains[side][3][1]])&&
                  Near(shapedIndex[Chains[side][2][1]],original[Chains[side][2][1]]),"third value is the ring finger, not the pinky");
            const float tooFar[5]{0,0,2,0,0};
            Request rejected=request;
            Check(!SetSupportCurls(rejected,tooFar)&&!rejected.curlsValid[side],"out-of-range shape rejected");
            Request none=request;none.allowed[0]=none.allowed[1]=false;
            Check(!SetSupportCurls(none,fistCurls),"no free support hand, no shape");
        }
        auto native=original;request.fingers[side]={true,.4f,.7f};
        Check(Apply(native.data(),inverse,request)==Result::Applied,"production committed-palette transaction applies");
        for(unsigned node=0;node<Count;++node)if(!IsFinger(node,side))Check(std::memcmp(&native[node],&original[node],sizeof(Matrix))==0,"transaction only writes free finger descendants");
        auto badInverse=inverse;badInverse[Chains[side][4][2]].scale=std::numeric_limits<float>::quiet_NaN();
        auto before=native;
        Check(Apply(native.data(),badInverse,request)==Result::Refused&&std::memcmp(&native,&before,sizeof(native))==0,"late invalid inverse bind never partially changes fingers");
        request.fingers[side].trigger=2;
        Check(Apply(native.data(),inverse,request)==Result::Refused&&std::memcmp(&native,&before,sizeof(native))==0,"invalid physical input refuses atomically");
        request.checksum=0;
        Check(Apply(reinterpret_cast<Matrix*>(uintptr_t(1)),inverse,request)==Result::NotApplicable,"unknown model does not touch native memory");
    }
    Request request{};request.checksum=286525724;request.count=Count;
    ConfigureFreeSupport(request,false,false,false,{true,.6f,.8f});
    Check(Apply(reinterpret_cast<Matrix*>(uintptr_t(1)),inverse,request)==Result::Faulted,"real palette read AV isolated");
    struct WithWeapon {Pose hands;std::array<uint8_t,256> appended;} combined{};
    combined.hands=original;combined.appended.fill(0xAB);const auto appended=combined.appended;
    Check(Apply(combined.hands.data(),inverse,request)==Result::Applied&&combined.appended==appended,"appended weapon and camera records untouched");
    SYSTEM_INFO system{};GetSystemInfo(&system);const size_t page=system.dwPageSize;
    auto* memory=static_cast<uint8_t*>(VirtualAlloc(nullptr,2*page,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Check(memory!=nullptr,"allocate mixed-protection native palette fixture");
    if(memory) {
        auto* destination=reinterpret_cast<Matrix*>(memory+page-(18*sizeof(Matrix)+16));
        std::memcpy(destination,original.data(),sizeof(original));DWORD old=0;
        Check(VirtualProtect(memory+page,page,PAGE_READONLY,&old)!=0,"protect middle of own finger palette");
        Check(Apply(destination,inverse,request)==Result::Faulted,"partial finger write AV isolated");
        Check(std::memcmp(destination,original.data(),sizeof(original))==0,"all partially changed fingers roll back before native consumer");
        VirtualFree(memory,0,MEM_RELEASE);
    }
    std::printf("ODST fingers: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
