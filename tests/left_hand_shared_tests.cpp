#include "anatomical_palette_logic.h"
#include <cstdio>
#include <cstring>
#include <limits>
#include <initializer_list>

namespace {
unsigned checks=0, failures=0;
void Check(bool value,const char* message) {
    ++checks; if (!value) { ++failures; std::fprintf(stderr,"FAIL: %s\n",message); }
}
using T=Halo4FloatingTransform;
T Pose(float scale,float x,float y,float z,float angle) {
    T t{}; t.scale=scale;
    const float q[]{.2f*std::sin(angle),.6f*std::sin(angle),.774596669f*std::sin(angle),std::cos(angle)};
    Halo4QuaternionToBlamBasis(q,t.rotation);
    t.translation[0]=x;t.translation[1]=y;t.translation[2]=z;return t;
}
T Mul(const T& a,const T& b) {
    T result{}; Check(Halo4ComposeFloatingTransforms(a,b,result),"valid compose");return result;
}
bool Near(const T& a,const T& b) {
    if (std::fabs(a.scale-b.scale)>.0001f) return false;
    for (int i=0;i<9;++i) if (std::fabs(a.rotation[i]-b.rotation[i])>.0001f) return false;
    for (int i=0;i<3;++i) if (std::fabs(a.translation[i]-b.translation[i])>.0001f) return false;
    return true;
}
void Dump(const T& t) {
    std::printf(" %.9g",t.scale);
    for (float f:t.rotation) std::printf(" %.9g",f);
    for (float f:t.translation) std::printf(" %.9g",f);
}
}

int main(int argc,char** argv) {
    const bool dump=argc==2 && std::strcmp(argv[1],"--markers")==0;
    AnatomicalPalmMarkers rigs[8]{};
    Check(Halo3AnatomicalPalmMarkers(504041493,37,rigs[0]),"H3 Chief");
    Check(Halo3AnatomicalPalmMarkers(269159697,31,rigs[1]),"H3 Elite");
    Check(Halo3AnatomicalPalmMarkers(268439051,31,rigs[2]),"H3 Dervish");
    Check(OdstAnatomicalPalmMarkers(286525724,37,rigs[3]),"ODST Recon");
    Check(OdstAnatomicalPalmMarkers(403178001,37,rigs[4]),"ODST ONI");
    Check(ReachAnatomicalPalmMarkers(404622103,47,rigs[5]),"Reach Spartan");
    Check(ReachAnatomicalPalmMarkers(419566353,41,rigs[6]),"Reach Elite");
    Check(Halo4AnatomicalPalmMarkers(rigs[7]),"H4 Storm");
    if (dump) {
        for (const auto& p:rigs) {
            std::printf("%d %d",p.rightNode,p.leftNode); Dump(p.right);Dump(p.left);std::puts("");
        }
        return failures?1:0;
    }
    AnatomicalPalmMarkers sentinel=rigs[0], denied=sentinel;
    Check(!Halo3AnatomicalPalmMarkers(404622103,37,denied) &&
          !OdstAnatomicalPalmMarkers(504041493,37,denied) &&
          !ReachAnatomicalPalmMarkers(404622103,41,denied) &&
          std::memcmp(&sentinel,&denied,sizeof(denied))==0,"unknown/cross-title identities write nothing");
    unsigned cases=0;
    for (const auto& palms:rigs)
    for (float scale:{.5f,1.f,2.f})
    for (float eye:{-.032f,.032f})
    for (bool support:{false,true}) {
        const T root=Pose(1.17f,eye,2,-1,.35f);
        T p[80]{};
        p[1]=Pose(scale,.12f,-.3f,.07f,.7f);
        p[2]=Pose(scale*.73f,.47f,.13f,-.11f,-.3f);
        if (support) {
            T target{};
            Check(Halo4BuildFloatingRigidSupportTarget(p[1],Pose(1,0,0,0,0),p[2],target),"engaged support solve");
            p[2]=target;
        }
        p[3]=Mul(p[1],Pose(1,.04f,.02f,.01f,.1f));
        p[4]=Mul(p[2],Pose(1,.03f,-.02f,.01f,-.2f));
        p[65]=Pose(.9f,.7f,.08f,.5f,-.9f); // appended gun beyond mask width
        T before[80];std::memcpy(before,p,sizeof(p));
        const T primaryPalm=Mul(Mul(root,p[1]),palms.right);
        const T supportPalm=Mul(Mul(root,p[2]),palms.left);
        Check(RouteLeftHandedFloatingPalette(p,80,1,10,2,20,root,palms.right,palms.left),"route complete palette");
        Check(Near(Mul(Mul(root,p[2]),palms.left),primaryPalm),"anatomical left palm equals final primary grip");
        Check(Near(Mul(Mul(root,p[1]),palms.right),supportPalm),"anatomical right palm equals final support grip");
        Check(std::fabs(p[1].scale-before[2].scale)<.00001f &&
              std::fabs(p[2].scale-before[1].scale)<.00001f,"role scales preserved");
        for (int n=0;n<80;++n) if (n<1 || n>4)
            Check(std::memcmp(&p[n],&before[n],sizeof(T))==0,"guns and other nodes byte-identical");
        T invBefore{},invAfter{};
        Check(Halo4InvertFloatingTransform(before[1],invBefore)&&Halo4InvertFloatingTransform(p[1],invAfter),"wrist inverses");
        Check(Near(Mul(invBefore,before[3]),Mul(invAfter,p[3])),"authored finger local pose preserved");
        Check(RouteLeftHandedFloatingPalette(p,80,1,10,2,20,root,palms.right,palms.left),"second exchange");
        for (int n=1;n<=4;++n) Check(Near(p[n],before[n]),"palm exchange is an involution");
        p[4].translation[2]=std::numeric_limits<float>::quiet_NaN();
        std::memcpy(before,p,sizeof(p));
        Check(!RouteLeftHandedFloatingPalette(p,80,1,10,2,20,root,palms.right,palms.left) &&
              std::memcmp(before,p,sizeof(p))==0,"late invalid finger atomically rejects route");
        ++cases;
    }
    std::printf("Shared left-hand alignment: %u rig/scale/eye/support cases, %u checks, %u failures\n",cases,checks,failures);
    return failures?1:0;
}
