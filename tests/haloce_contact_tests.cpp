#include "../src/common/haloce_contact_logic.h"
#include <cstdio>
#include <limits>

using namespace halo_ce;
#define CHECK(value) do { if (!(value)) { std::fprintf(stderr,"CE contact line %d: %s\n",__LINE__,#value); return 1; } } while(false)
static bool Near(Vec3 a,Vec3 b) { return Dot(a-b,a-b)<0.0000001f; }
static Vec3 Vec(contact_melee::Point p) { return {p.x,p.y,p.z}; }
static void Node(AnimationNode& node,const char* name,int parent)
{ std::strcpy(node.name,name);node.parent=int16_t(parent); }
static void ConfigureRig(RenderContext& context,bool left,bool anatomical)
{
    auto& rig=context.tracking.controllers;
    rig.controlsPresentationBlocked=false;rig.leftHanded=left;rig.handAlignment=anatomical;
    rig.gunScale=rig.supportScale=1;rig.gunForwardM=rig.supportForwardM=0;rig.primaryShoulderDrop=0;
    rig.physical[0]={true,{-.3f,1.2f,-.6f},{}};
    rig.physical[1]={true,{.3f,1.2f,-.6f},{}};
    rig.primaryAim=rig.physical[left?0:1];rig.support=rig.physical[left?1:0];
    rig.independentPrimaryAim=rig.primaryAim;
}
static bool Tracked(const RenderContext& context,const FirstPersonBinding& binding,const NodeMatrix* authored,
    std::array<NodeMatrix,kFirstPersonMaxNodes>& palette)
{
    return BuildTrackedFirstPersonPalette(binding,authored,context.camera,context.tracking,context.reference,
        context.unitsPerMeter,context.positional,palette);
}
int main()
{
    AnimationNode nodes[13]{};
    Node(nodes[0],"frame root",-1);
    Node(nodes[1],"frame l upperarm",0);Node(nodes[2],"frame l forearm",1);
    Node(nodes[3],"frame l wrist",2);Node(nodes[4],"frame l finger",3);
    Node(nodes[5],"frame r upperarm",0);Node(nodes[6],"frame r forearm",5);
    Node(nodes[7],"frame r wrist",6);Node(nodes[8],"frame r finger",7);
    Node(nodes[9],"frame gun",7);Node(nodes[10],"frame muzzle",9);
    Node(nodes[11],"frame magazine",9);Node(nodes[12],"frame other",0);
    FirstPersonBinding binding{};CHECK(BuildFirstPersonBinding(25,3,nodes,13,binding));
    RenderContext context{};
    context.camera.position={100,200,300};context.camera.forward={1,0,0};context.camera.up={0,0,1};
    context.camera.viewport={0,0,800,1000};context.camera.window=context.camera.viewport;
    context.camera.verticalFov=1;context.camera.nearPlane=.01f;context.camera.farPlane=1000;
    context.tracking.serial=10;context.tracking.spaceEpoch=2;context.tracking.generation=3;
    context.tracking.predictedDisplayTimeNs=1'000'000'000;context.tracking.headPosition={.1f,1.6f,.2f};
    context.reference.generation=3;context.reference.spaceEpoch=2;
    context.reference.position={.2f,1.5f,.1f};context.referenceRevision=4;context.rendererEpoch=5;
    context.unitsPerMeter=1;context.positional=true;
    NodeMatrix authored[13]{};
    const Vec3 positions[13]={{0,0,0},{0,.2f,0},{.15f,.3f,0},{.3f,.3f,0},{.32f,.3f,0},
        {0,-.2f,0},{.15f,-.3f,0},{.3f,-.3f,0},{.32f,-.3f,0},{.35f,-.3f,0},
        {.6f,-.3f,0},{.45f,-.3f,-.02f},{0,0,.1f}};
    for (size_t i=0;i<13;++i) authored[i].position=context.camera.position+positions[i];
    for (bool left:{false,true}) for (bool anatomical:{false,true})
    for (bool armIk:{false,true}) for (bool floating:{false,true})
    {
        ConfigureRig(context,left,anatomical);context.tracking.controllers.armIk=armIk;
        context.tracking.controllers.floatingHands=floating;
        std::array<NodeMatrix,kFirstPersonMaxNodes> palette{};CHECK(Tracked(context,binding,authored,palette));
        ContactHandBinding hands[2]{};CHECK(BuildContactHandBindings(binding,context.tracking.controllers,hands));
        const unsigned primary=left?0:1,support=1-primary;
        CHECK((hands[primary].mask&binding.gunMask)==binding.gunMask);
        CHECK(!(hands[support].mask&binding.gunMask));
        CHECK(hands[primary].wrist==(left&&anatomical?binding.leftWrist:binding.rightWrist));
        contact_melee::Frame frames[2]{};CHECK(BuildContactFrames(context,binding,palette.data(),0x12340017,frames));
        for (unsigned side=0;side<2;++side)
        {
            CHECK(frames[side].Valid()&&frames[side].rigidMotion&&frames[side].count<=64);
            CHECK(Near(Vec(frames[side].transform.World(frames[side].points[0])),palette[hands[side].wrist].position));
            CHECK(Near(Vec(frames[side].controllerPose.origin),context.tracking.controllers.physical[side].position));
            unsigned sample=1;
            for (size_t i=0;i<binding.count;++i)
                if (i!=size_t(hands[side].wrist)&&(hands[side].mask&(uint64_t{1}<<i)))
                {
                    CHECK(sample<frames[side].count);
                    CHECK(Near(Vec(frames[side].transform.World(frames[side].points[sample++])),palette[i].position));
                }
            CHECK(sample==frames[side].count);
        }
        const auto before=palette;const Vec3 delta[2]={{-.02f,.03f,.01f},{.04f,-.01f,-.02f}};
        CHECK(ApplyContactCorrections(context,binding,authored,delta,palette.data()));
        CHECK(!std::memcmp(&palette[0],&before[0],sizeof(NodeMatrix)));
        CHECK(!std::memcmp(&palette[12],&before[12],sizeof(NodeMatrix)));
        for (unsigned side=0;side<2;++side)
        {
            for (size_t i=0;i<binding.count;++i) if (hands[side].mask&(uint64_t{1}<<i))
            {
                CHECK(Near(palette[i].position,before[i].position+delta[side]));
                CHECK(palette[i].scale==before[i].scale&&Near(palette[i].forward,before[i].forward));
            }
            if (floating)
                for (size_t i=0;i<binding.count;++i)
                    if (binding.armMask[hands[side].anatomy]&(uint64_t{1}<<i))
                        CHECK(Near(palette[i].position,palette[hands[side].wrist].position));
        }
        CHECK(Near(palette[binding.gun].position,before[binding.gun].position+delta[primary]));
        Vec3 invalid[2]={{},{std::numeric_limits<float>::quiet_NaN(),0,0}};const auto corrected=palette;
        CHECK(!ApplyContactCorrections(context,binding,authored,invalid,palette.data()));
        CHECK(!std::memcmp(palette.data(),corrected.data(),sizeof(palette)));
        invalid[1]={.76f,0,0};CHECK(!ApplyContactCorrections(context,binding,authored,invalid,palette.data()));
        CHECK(!std::memcmp(palette.data(),corrected.data(),sizeof(palette)));
    }
    ConfigureRig(context,false,false);
    std::array<NodeMatrix,kFirstPersonMaxNodes> palette{};CHECK(Tracked(context,binding,authored,palette));
    contact_melee::Frame first[2]{};CHECK(BuildContactFrames(context,binding,palette.data(),17,first));
    contact_melee::Motion motion[2];contact_melee::Sweeps sweeps{};
    for (unsigned side=0;side<2;++side) CHECK(motion[side].Advance(first[side],1,sweeps)==contact_melee::AdvanceResult::Seeded);
    // A reload/finger pose change with fixed controllers cannot become a punch.
    ++context.tracking.serial;context.tracking.predictedDisplayTimeNs+=20'000'000;
    palette[4].position.x+=.2f;palette[10].position.z+=.3f;
    contact_melee::Frame animated[2]{};CHECK(BuildContactFrames(context,binding,palette.data(),17,animated));
    for (unsigned side=0;side<2;++side)
    {
        CHECK(motion[side].Advance(animated[side],1,sweeps)==contact_melee::AdvanceResult::Advanced);
        CHECK(sweeps.count==0);
    }
    // Actor locomotion changes the world transform, never physical swing speed.
    ++context.tracking.serial;context.tracking.predictedDisplayTimeNs+=20'000'000;
    context.camera.position.x+=10;
    CHECK(Tracked(context,binding,authored,palette));
    contact_melee::Frame moved[2]{};CHECK(BuildContactFrames(context,binding,palette.data(),17,moved));
    for (unsigned side=0;side<2;++side)
    {
        CHECK(motion[side].Advance(moved[side],1,sweeps)==contact_melee::AdvanceResult::Advanced);
        CHECK(sweeps.count==0);
    }
    ++context.tracking.serial;context.tracking.predictedDisplayTimeNs+=20'000'000;
    context.tracking.controllers.physical[1].position.x+=.1f;
    context.tracking.controllers.primaryAim=context.tracking.controllers.physical[1];
    CHECK(Tracked(context,binding,authored,palette));
    contact_melee::Frame swung[2]{};CHECK(BuildContactFrames(context,binding,palette.data(),17,swung));
    CHECK(motion[0].Advance(swung[0],1,sweeps)==contact_melee::AdvanceResult::Advanced&&sweeps.count==0);
    CHECK(motion[1].Advance(swung[1],1,sweeps)==contact_melee::AdvanceResult::Advanced&&sweeps.count>0);
    CHECK(std::fabs(sweeps.values[0].speedMetresPerSecond-5)<.002f);
    CHECK(motion[1].Advance(swung[1],1,sweeps)==contact_melee::AdvanceResult::Duplicate&&sweeps.count==0);
    const auto oldContext=context;
    for (unsigned change=0;change<5;++change)
    {
        context=oldContext;++context.tracking.serial;context.tracking.predictedDisplayTimeNs+=20'000'000;
        FirstPersonBinding nextBinding=binding;
        if (change==0) ++context.referenceRevision;
        if (change==1) ++context.rendererEpoch;
        if (change==2) ++nextBinding.graph;
        if (change==3) context.tracking.controllers.handAlignment=true;
        if (change==4) context.unitsPerMeter=.5f;
        contact_melee::Frame next[2]{};CHECK(BuildContactFrames(context,nextBinding,palette.data(),17,next));
        contact_melee::Motion independent;CHECK(independent.Advance(swung[1],1,sweeps)==contact_melee::AdvanceResult::Seeded);
        CHECK(independent.Advance(next[1],1,sweeps)==contact_melee::AdvanceResult::Seeded&&sweeps.count==0);
    }
    context=oldContext;
    contact_melee::Frame rejected[2]={first[0],first[1]};const auto oldFirst=first[0];
    context.tracking.controllers.physical[1].valid=false;
    CHECK(!BuildContactFrames(context,binding,palette.data(),17,rejected));
    CHECK(!std::memcmp(&rejected[0],&oldFirst,sizeof(oldFirst)));
    context=oldContext;context.reference.generation++;
    CHECK(!BuildContactFrames(context,binding,palette.data(),17,rejected));
    context=oldContext;context.tracking.predictedDisplayTimeNs=0;
    CHECK(!BuildContactFrames(context,binding,palette.data(),17,rejected));
    context=oldContext;FirstPersonBinding malformed=binding;malformed.leftMask|=binding.gunMask;
    CHECK(!BuildContactFrames(context,malformed,palette.data(),17,rejected));
    CHECK(!BuildContactFrames(context,binding,palette.data(),UINT32_MAX,rejected));
    const auto validPalette=palette;palette[binding.gun].position.x+=30;
    CHECK(!BuildContactFrames(context,binding,palette.data(),17,rejected));palette=validPalette;
    // Nonpositional tracking uses the head as the position reference, exactly
    // matching CE's production controller-to-world matrix in either mode.
    for (bool positional:{false,true}) for (float scale:{.5f,1.0f,2.0f})
    {
        context=oldContext;context.positional=positional;context.unitsPerMeter=scale;
        context.reference.orientation={0,.258819045f,0,.965925826f};
        contact_melee::TrackingToWorld transform{};CHECK(BuildContactTransform(context,transform));
        NodeMatrix expected{};
        CHECK(BuildControllerMatrix(context.camera,context.tracking,context.reference,
            context.tracking.controllers.physical[0],scale,positional,expected));
        const auto actual=transform.World(ContactPoint(context.tracking.controllers.physical[0].position));
        CHECK(Near(Vec(actual),expected.position));
    }
    return 0;
}
