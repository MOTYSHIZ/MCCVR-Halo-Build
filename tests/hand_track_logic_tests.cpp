#include <cmath>
#include <cstdio>
#include <cstring>
#include "hand_track_logic.h"

namespace {
using namespace hand_track;
unsigned checks=0,failures=0;
void Check(bool value,const char* name){++checks;if(!value){++failures;std::printf("FAIL: %s\n",name);}}
bool Near(float a,float b,float e=1e-4f){return std::fabs(a-b)<=e;}
bool NearPose(const Pose& a,const Pose& b,float e=1e-4f)
{
    for(int i=0;i<9;++i)if(!Near(a.basis[i],b.basis[i],e))return false;
    for(int i=0;i<3;++i)if(!Near(a.position[i],b.position[i],e))return false;
    return true;
}
Library Load(const char* text,ParseReport* report=nullptr)
{
    static char buffer[8192];
    std::snprintf(buffer,sizeof(buffer),"%s",text);
    static Library library;library=Library{};
    const auto r=Parse(buffer,library);
    if(report)*report=r;
    return library;
}
void RotZ(float deg,float m[9]){const float z[3]{0,0,1};AxisAngle(z,deg*0.01745329252f,m);}

void Defaults()
{
    ParseReport report{};
    const Library library=Load(kDefaultText,&report);
    Check(report.errors==0,"built-in tracks parse without errors");
    Check(library.count==2&&report.tracks==2,"built-in reload and preview tracks");
    const Track* reload=Find(library,"reload","fp_assault_rifle");
    Check(reload&&reload->keyCount==2&&reload->keys[0].frame==Frame::Authored,"reload follows the game's own hand");
    Check(Find(library,"preview",nullptr)!=nullptr,"preview serves an unknown weapon");
    Check(Find(library,"missing","fp_assault_rifle")==nullptr,"absent track name finds nothing");
}

void WeaponMatching()
{
    const Library library=Load(
        "track reload *\nkey 0 authored 0 0 0 0 0 0 1\nend\n"
        "track reload rifle\nkey 0 weapon 0.1 0 0 0 0 0 1\nend\n"
        "track reload assault_rifle\nkey 0 weapon 0.2 0 0 0 0 0 1\nend\n");
    Check(library.count==3,"three reload tracks");
    const Track* ar=Find(library,"reload","fp_assault_rifle");
    Check(ar&&std::strcmp(ar->weapon,"assault_rifle")==0,"longest matching weapon pattern wins");
    const Track* br=Find(library,"reload","fp_battle_rifle");
    Check(br&&std::strcmp(br->weapon,"rifle")==0,"shorter pattern serves other rifles");
    const Track* magnum=Find(library,"reload","fp_magnum");
    Check(magnum&&Wildcard(magnum->weapon),"wildcard serves every other weapon");
}

void ParseErrors()
{
    ParseReport report{};
    Library library=Load(
        "track good *\nkey 0 authored 0 0 0 0 0 0 1\nend\n"
        "track badframe *\nkey 0 sideways 0 0 0 0 0 0 1\nend\n"
        "track backwards *\nkey 1 authored 0 0 0 0 0 0 1\nkey 0.5 authored 0 0 0 0 0 0 1\nend\n"
        "track far *\nkey 0 weapon 3 0 0 0 0 0 1\nend\n"
        "track heavy *\nkey 0 weapon 0 0 0 0 0 0 1.5\nend\n"
        "track empty *\nend\n"
        "track shaped * # comment\nkey 0 authored 0 0 0 0 0 0 1 magazine # trailing\nend\n"
        "track badshape *\nkey 0 authored 0 0 0 0 0 0 1 claw\nend\n"
        "stray line\n"
        "track open *\nkey 0 authored 0 0 0 0 0 0 1\n",&report);
    Check(library.count==2,"only the good and shaped tracks survive");
    Check(report.droppedTracks==7,"each malformed track is dropped whole");
    Check(report.errors>=8&&report.firstErrorLine==5,"first error names its line");
    const Track* shaped=Find(library,"shaped",nullptr);
    Check(shaped&&shaped->keys[0].shape==Shape::Magazine,"optional hand shape parsed past comments");
    report={};
    library=Load("track t *\nblend_in 3\nkey 0 authored 0 0 0 0 0 0 1\nend\n",&report);
    Check(library.count==0&&report.errors==1,"blend time over two seconds rejected");
    library=Load("track t *\nblend_in 0\nblend_out 0.5\nsmooth 0\nkey 0 authored 0 0 0 0 0 0 1\nend\n",&report);
    Check(library.count==1&&library.tracks[0].blendIn==0&&Near(library.tracks[0].blendOut,0.5f)&&!library.tracks[0].smooth,
        "blend and smooth settings parsed");
    char longName[128];std::snprintf(longName,sizeof(longName),"track %s *\nkey 0 authored 0 0 0 0 0 0 1\nend\n",
        "a_name_that_is_much_longer_than_forty_characters_long");
    library=Load(longName,&report);
    Check(library.count==0,"over-long names are rejected, not truncated");
}

void Sampling()
{
    Track track{};
    track.blendIn=0.2f;track.blendOut=0.4f;track.smooth=false;track.keyCount=3;
    track.keys[0]={0.0f,Frame::Weapon,{0,0,0},{},1};
    track.keys[1]={1.0f,Frame::Weapon,{0.1f,0,0},{},1};
    track.keys[2]={2.0f,Frame::Weapon,{0.1f,0,0},{},0.5f};
    Check(!Evaluate(track,-0.1f).active,"before the start is inactive");
    Sampled s=Evaluate(track,0.0f);
    Check(s.active&&s.weight==0,"weight starts at zero and blends in");
    s=Evaluate(track,0.1f);
    Check(s.weight>0&&s.weight<1,"half way through blend in");
    s=Evaluate(track,0.5f);
    Check(Near(s.weight,1)&&Near(s.u,0.5f)&&s.a.time==0&&s.b.time==1,"linear midpoint between first keys");
    s=Evaluate(track,1.5f);
    Check(Near(s.weight,0.75f),"key weights interpolate");
    s=Evaluate(track,2.0f);
    Check(s.active&&Near(s.weight,0.5f)&&s.a.time==2&&s.b.time==2,"last key holds");
    s=Evaluate(track,2.2f);
    Check(s.active&&s.weight<0.5f&&s.weight>0,"blending out after the last key");
    s=Evaluate(track,2.4f);
    Check(!s.active&&s.finished,"finished once blended out");
    track.smooth=true;
    s=Evaluate(track,0.25f);
    Check(Near(s.u,Smooth(0.25f)),"smooth tracks ease between keys");
    Track late=track;late.keys[0].time=0.5f;
    s=Evaluate(late,0.25f);
    Check(s.active&&s.a.time==0.5f&&s.b.time==0.5f,"before a late first key holds that key");
    Track empty{};
    Check(!Evaluate(empty,0).active,"empty track never plays");
    Check(!Evaluate(track,std::nanf("")).active,"non-finite time never plays");
}

Frames MakeFrames()
{
    Frames f{};
    f.unitsPerMetre=0.33f;
    RotZ(90,f.gun.basis); // gun forward points along world +Y
    f.gun.position[0]=10;f.gun.position[1]=20;f.gun.position[2]=5;
    f.authored.position[0]=10;f.authored.position[1]=20.1f;f.authored.position[2]=4.98f;
    RotZ(80,f.authored.basis);f.authoredValid=true;
    f.controller.position[0]=9.8f;f.controller.position[1]=19.9f;f.controller.position[2]=4.9f;
    f.controllerValid=true;
    return f;
}

void Resolving()
{
    const Frames f=MakeFrames();
    Key key{};key.frame=Frame::Authored;
    Pose out{};
    Check(ResolveKey(key,f,out)&&NearPose(out,f.authored),"zero authored key is the game's own hand");
    key.offset[0]=1;
    Check(ResolveKey(key,f,out)&&Near(out.position[0],10)&&Near(out.position[1],20.1f+0.33f),
        "forward offset follows the gun barrel in world units");
    key.offset[0]=0;key.offset[1]=1;
    Check(ResolveKey(key,f,out)&&Near(out.position[0],10-0.33f),"left offset is the gun's left");
    key.offset[1]=0;key.offset[2]=-1;
    Check(ResolveKey(key,f,out)&&Near(out.position[2],4.98f-0.33f),"up offset is the gun's up");
    key=Key{};key.frame=Frame::Weapon;key.offset[0]=0.1f;
    Check(ResolveKey(key,f,out)&&Near(out.position[1],20+0.033f)&&Near(out.position[0],10),
        "weapon key is measured from the gun wrist");
    float expected[9];std::memcpy(expected,f.authored.basis,sizeof(expected));
    bool same=true;for(int i=0;i<9;++i)same=same&&Near(out.basis[i],expected[i]);
    Check(same,"weapon key keeps the game's grip orientation");
    key=Key{};key.frame=Frame::Controller;key.offset[2]=0.05f;
    Check(ResolveKey(key,f,out)&&Near(out.position[2],4.9f+0.0165f),"controller key moves relative to the real hand");
    key=Key{};key.frame=Frame::Authored;key.rotation[0]=10;
    Check(ResolveKey(key,f,out),"yaw key resolves");
    float yawed[9];RotZ(90,yawed);
    Check(Near(out.basis[0],yawed[0],1e-3f)&&Near(out.basis[1],yawed[1],1e-3f),"yaw turns the hand about the gun's up");
    key=Key{};key.frame=Frame::Authored;key.rotation[1]=90;
    Check(ResolveKey(key,f,out),"pitch key resolves");
    // Gun-up pitch tips the hand's forward toward world up around the gun's right.
    Frames level=f;RotZ(0,level.authored.basis);RotZ(0,level.gun.basis);
    Check(ResolveKey(key,level,out)&&Near(out.basis[2],1,1e-3f),"positive pitch tips forward up");
    key=Key{};key.frame=Frame::Authored;key.rotation[2]=90;
    Check(ResolveKey(key,level,out)&&Near(out.basis[6+0],0,1e-3f)&&Near(out.basis[6+1],-1,1e-3f),
        "positive roll turns the top to the right");
    Frames noAuthored=f;noAuthored.authoredValid=false;
    key=Key{};key.frame=Frame::Authored;
    Check(!ResolveKey(key,noAuthored,out),"authored key needs the game's hand");
    key.frame=Frame::Weapon;
    Check(ResolveKey(key,noAuthored,out),"weapon key falls back to the controller grip orientation");
    Frames bad=f;bad.unitsPerMetre=0;
    Check(!ResolveKey(key,bad,out),"invalid world scale rejected");
}

void Targets()
{
    const Frames f=MakeFrames();
    Sampled s{};
    Pose out{};
    Check(!Target(s,f,out),"inactive sample leaves the target alone");
    s.active=true;s.weight=0;s.a.frame=Frame::Authored;s.b=s.a;
    Check(!Target(s,f,out),"zero weight leaves the target alone");
    s.weight=1;
    Check(Target(s,f,out)&&NearPose(out,f.authored,1e-4f),"full weight puts the hand on the track");
    s.weight=0.5f;
    Check(Target(s,f,out)&&Near(out.position[1],(19.9f+20.1f)/2)&&Near(out.position[2],(4.9f+4.98f)/2),
        "half weight is half way from the controller");
    s.weight=1;s.b.frame=Frame::Weapon;s.b.offset[0]=0.2f;s.u=0.5f;
    Pose a{},b{};ResolveKey(s.a,f,a);ResolveKey(s.b,f,b);
    Check(Target(s,f,out)&&Near(out.position[1],(a.position[1]+b.position[1])/2),"keys in different frames blend in world space");
    Frames noController=f;noController.controllerValid=false;
    Check(!Target(s,f.controllerValid?noController:f,out),"no controller target, no track");
    Quat q{};float m[9];RotZ(37,m);
    Check(ToQuat(m,q),"rotation converts to a quaternion");
    float back[9];FromQuat(q,back);bool same=true;for(int i=0;i<9;++i)same=same&&Near(back[i],m[i]);
    Check(same,"quaternion round trip");
    float flip[9]{-1,0,0,0,-1,0,0,0,1};
    Check(ToQuat(flip,q)&&Near(std::fabs(q.z),1),"half-turn rotation converts");
}
}

int main()
{
    Defaults();WeaponMatching();ParseErrors();Sampling();Resolving();Targets();
    std::printf("hand_track_logic_tests: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
