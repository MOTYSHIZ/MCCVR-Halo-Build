#pragma once
// Support-hand tracks: short, data-driven paths for the support hand, each key
// placed relative to the held gun. The arm solver keeps solving the same
// two-bone arm; a playing track only moves the support hand's TARGET, blended
// from the controller onto the path and back. Shared by Halo 3 and ODST (and by
// Manual Reloading later), modelled on the Halo: Campaign Evolved VR mod's
// weapon-relative support-hand anchors (docs/PLAYER-IK-PARITY-GAP-2026-11-13.md).
//
// Pure and allocation-free. Parsing runs off the render path; Evaluate and
// Resolve are small enough for the palette hook.
//
// Bases are column-major 3x3 (element (row r, column c) at [c*3+r]), the same
// layout BoneMatrix::rotation uses; a frame's columns are forward, left, up.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hand_track
{
enum class Frame : uint8_t
{
    Weapon,     // the gun: origin at the gun hand's wrist
    Authored,   // the game's own animated support hand on that gun
    Controller, // the player's real support hand
};

// Held hand shapes a key may ask for. The consumer decides what each means on
// its own rig; None leaves the fingers as they are.
enum class Shape : uint8_t { None, Rest, Index, Fist, ThumbsUp, Point, PointDown, Ok, Magazine, Handle, Count };
inline constexpr const char* kShapeNames[unsigned(Shape::Count)]{
    "none","rest","index","fist","thumbsup","point","pointdown","ok","magazine","handle"};

inline constexpr unsigned kMaxKeys=16;
inline constexpr unsigned kMaxTracks=48;
inline constexpr unsigned kNameLength=40;
inline constexpr float kMaxTime=10.0f;     // seconds
inline constexpr float kMaxOffset=1.0f;    // metres from the frame origin
inline constexpr float kMaxBlend=2.0f;     // seconds
inline constexpr float kMaxAngle=180.0f;   // degrees

struct Key
{
    float time{};          // seconds from the start of the track
    Frame frame{Frame::Authored};
    float offset[3]{};     // metres along the gun's forward, left, up
    float rotation[3]{};   // degrees: yaw (+ turns left), pitch (+ tips up), roll (+ clockwise from behind)
    float weight{1.0f};    // 0 leaves the hand on the controller, 1 puts it on this key
    Shape shape{Shape::None};
};

struct Track
{
    char name[kNameLength]{};
    char weapon[kNameLength]{}; // "*" any weapon, else part of the catalogue name ("assault_rifle")
    float blendIn{0.10f}, blendOut{0.15f};
    bool smooth{true};          // ease between keys instead of a straight constant-speed move
    unsigned keyCount{};
    Key keys[kMaxKeys]{};
};

struct Library
{
    unsigned count{};
    Track tracks[kMaxTracks]{};
};

// ---------------------------------------------------------------------------
// Lookup

inline bool Wildcard(const char* pattern) noexcept { return pattern[0]=='*'&&pattern[1]==0; }

// The track named `name` for this weapon: the longest weapon pattern contained
// in the weapon's catalogue name wins; a "*" track serves every other weapon.
inline const Track* Find(const Library& library,const char* name,const char* weaponName) noexcept
{
    if(!name) return nullptr;
    const Track* best=nullptr;size_t bestLength=0;const Track* fallback=nullptr;
    for(unsigned i=0;i<library.count&&i<kMaxTracks;++i)
    {
        const Track& track=library.tracks[i];
        if(std::strcmp(track.name,name)!=0) continue;
        if(Wildcard(track.weapon)) { if(!fallback) fallback=&track; continue; }
        const size_t length=std::strlen(track.weapon);
        if(length&&weaponName&&std::strstr(weaponName,track.weapon)&&length>bestLength)
        { best=&track;bestLength=length; }
    }
    return best?best:fallback;
}

// ---------------------------------------------------------------------------
// Sampling

inline float Clamp01(float v) noexcept { return v<0?0:(v>1?1:v); }
inline float Smooth(float u) noexcept { u=Clamp01(u);return u*u*(3-2*u); }

struct Sampled
{
    bool active{};
    bool finished{};
    Key a{}, b{};
    float u{};      // 0 at a, 1 at b
    float weight{}; // overall pull from the controller onto the path
};

inline float Duration(const Track& track) noexcept
{ return track.keyCount?track.keys[track.keyCount-1].time:0.0f; }

// Where the track is `seconds` after it started. Finished once the blend out
// has completed; inactive before the start and for an empty track.
inline Sampled Evaluate(const Track& track,float seconds) noexcept
{
    Sampled out{};
    if(!track.keyCount||track.keyCount>kMaxKeys||!std::isfinite(seconds)||seconds<0) return out;
    const float end=Duration(track);
    if(seconds>=end+track.blendOut) { out.finished=true;return out; }
    const float in=track.blendIn>0?Smooth(seconds/track.blendIn):1.0f;
    const float fade=seconds<=end?1.0f:(track.blendOut>0?1.0f-Smooth((seconds-end)/track.blendOut):0.0f);
    unsigned i=0;
    while(i+1<track.keyCount&&track.keys[i+1].time<=seconds) ++i;
    out.a=track.keys[i];
    if(i+1<track.keyCount&&seconds>=track.keys[0].time)
    {
        out.b=track.keys[i+1];
        const float span=out.b.time-out.a.time;
        const float raw=span>1e-6f?(seconds-out.a.time)/span:1.0f;
        out.u=track.smooth?Smooth(raw):Clamp01(raw);
    }
    else { out.b=out.a;out.u=0; }
    const float keyWeight=out.a.weight+(out.b.weight-out.a.weight)*out.u;
    out.weight=Clamp01(in*fade*keyWeight);
    out.active=true;
    return out;
}

// ---------------------------------------------------------------------------
// Poses

struct Pose { float basis[9]{1,0,0,0,1,0,0,0,1}; float position[3]{}; };

inline bool Finite(const Pose& p) noexcept
{
    for(float v:p.basis) if(!std::isfinite(v)) return false;
    for(float v:p.position) if(!std::isfinite(v)) return false;
    return true;
}
inline void Multiply(const float a[9],const float b[9],float out[9]) noexcept
{
    float r[9];
    for(int c=0;c<3;++c) for(int row=0;row<3;++row)
        r[c*3+row]=a[0*3+row]*b[c*3+0]+a[1*3+row]*b[c*3+1]+a[2*3+row]*b[c*3+2];
    std::memcpy(out,r,sizeof(r));
}
inline void Transpose(const float a[9],float out[9]) noexcept
{
    float r[9];
    for(int c=0;c<3;++c) for(int row=0;row<3;++row) r[c*3+row]=a[row*3+c];
    std::memcpy(out,r,sizeof(r));
}
// Rotation by `angle` radians about unit `axis` (right-handed).
inline void AxisAngle(const float axis[3],float angle,float out[9]) noexcept
{
    const float x=axis[0],y=axis[1],z=axis[2],c=std::cos(angle),s=std::sin(angle),t=1-c;
    const float m[9]{t*x*x+c,t*x*y+s*z,t*x*z-s*y, t*x*y-s*z,t*y*y+c,t*y*z+s*x, t*x*z+s*y,t*y*z-s*x,t*z*z+c};
    std::memcpy(out,m,sizeof(m));
}

struct Quat { float x{},y{},z{},w{1}; };
inline bool ToQuat(const float m[9],Quat& q) noexcept
{
    // R(r,c)=m[c*3+r]
    const float m00=m[0],m11=m[4],m22=m[8];
    const float m01=m[3],m10=m[1],m02=m[6],m20=m[2],m12=m[7],m21=m[5];
    const float trace=m00+m11+m22;
    if(trace>0) { const float s=std::sqrt(trace+1)*2;q={(m21-m12)/s,(m02-m20)/s,(m10-m01)/s,.25f*s}; }
    else if(m00>m11&&m00>m22) { const float s=std::sqrt(1+m00-m11-m22)*2;q={.25f*s,(m01+m10)/s,(m02+m20)/s,(m21-m12)/s}; }
    else if(m11>m22) { const float s=std::sqrt(1+m11-m00-m22)*2;q={(m01+m10)/s,.25f*s,(m12+m21)/s,(m02-m20)/s}; }
    else { const float s=std::sqrt(1+m22-m00-m11)*2;q={(m02+m20)/s,(m12+m21)/s,.25f*s,(m10-m01)/s}; }
    const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    if(!std::isfinite(n)||n<1e-6f) return false;
    q.x/=n;q.y/=n;q.z/=n;q.w/=n;return true;
}
inline void FromQuat(const Quat& q,float m[9]) noexcept
{
    const float x=q.x,y=q.y,z=q.z,w=q.w;
    m[0]=1-2*(y*y+z*z); m[1]=2*(x*y+z*w);   m[2]=2*(x*z-y*w);
    m[3]=2*(x*y-z*w);   m[4]=1-2*(x*x+z*z); m[5]=2*(y*z+x*w);
    m[6]=2*(x*z+y*w);   m[7]=2*(y*z-x*w);   m[8]=1-2*(x*x+y*y);
}
inline Quat Slerp(Quat a,Quat b,float u) noexcept
{
    float d=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
    if(d<0) { b={-b.x,-b.y,-b.z,-b.w};d=-d; }
    float wa,wb;
    if(d>0.9995f) { wa=1-u;wb=u; }
    else { const float th=std::acos(d),s=std::sin(th);wa=std::sin((1-u)*th)/s;wb=std::sin(u*th)/s; }
    Quat q{a.x*wa+b.x*wb,a.y*wa+b.y*wb,a.z*wa+b.z*wb,a.w*wa+b.w*wb};
    const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    if(n>1e-6f) { q.x/=n;q.y/=n;q.z/=n;q.w/=n; }
    return q;
}

// Straight-line position, shortest-arc rotation.
inline bool Blend(const Pose& a,const Pose& b,float u,Pose& out) noexcept
{
    u=Clamp01(u);
    Quat qa{},qb{};
    if(!ToQuat(a.basis,qa)||!ToQuat(b.basis,qb)) return false;
    Pose r{};
    FromQuat(Slerp(qa,qb,u),r.basis);
    for(int i=0;i<3;++i) r.position[i]=a.position[i]+(b.position[i]-a.position[i])*u;
    if(!Finite(r)) return false;
    out=r;return true;
}

// The frames a key can be placed in, for this frame of the game.
struct Frames
{
    Pose gun;              // origin at the gun-hand wrist; columns forward/left/up of the gun
    Pose authored;         // the game's own support-hand wrist on the gun
    Pose controller;       // the support hand's ordinary controller target
    bool authoredValid{};
    bool controllerValid{};
    float unitsPerMetre{1.0f}; // world units per metre
};

// One key in world space. A Weapon key keeps the authored hand's orientation
// (a sensible grip) at a point fixed to the gun.
inline bool ResolveKey(const Key& key,const Frames& frames,Pose& out) noexcept
{
    const Pose* base=nullptr;
    switch(key.frame)
    {
    case Frame::Weapon: base=frames.authoredValid?&frames.authored:(frames.controllerValid?&frames.controller:nullptr);break;
    case Frame::Authored: base=frames.authoredValid?&frames.authored:nullptr;break;
    case Frame::Controller: base=frames.controllerValid?&frames.controller:nullptr;break;
    }
    if(!base||!Finite(frames.gun)||!std::isfinite(frames.unitsPerMetre)||frames.unitsPerMetre<=0) return false;
    const float* G=frames.gun.basis;
    const float deg=0.01745329252f;
    const float up[3]{G[6],G[7],G[8]};
    const float right[3]{-G[3],-G[4],-G[5]};
    const float forward[3]{G[0],G[1],G[2]};
    float yaw[9],pitch[9],roll[9],trim[9];
    AxisAngle(up,key.rotation[0]*deg,yaw);
    AxisAngle(right,key.rotation[1]*deg,pitch);
    AxisAngle(forward,key.rotation[2]*deg,roll);
    Multiply(yaw,pitch,trim);Multiply(trim,roll,trim);
    Pose r{};
    Multiply(trim,base->basis,r.basis);
    const float* origin=key.frame==Frame::Weapon?frames.gun.position:base->position;
    for(int row=0;row<3;++row)
        r.position[row]=origin[row]+(G[row]*key.offset[0]+G[3+row]*key.offset[1]+G[6+row]*key.offset[2])*frames.unitsPerMetre;
    if(!Finite(r)) return false;
    out=r;return true;
}

// The support hand's target: the controller target moved `weight` of the way
// onto the track's current point. False leaves the caller's target alone.
inline bool Target(const Sampled& sample,const Frames& frames,Pose& out) noexcept
{
    if(!sample.active||!frames.controllerValid||sample.weight<=0) return false;
    Pose a{},b{},onPath{};
    if(!ResolveKey(sample.a,frames,a)||!ResolveKey(sample.b,frames,b)||!Blend(a,b,sample.u,onPath)) return false;
    return Blend(frames.controller,onPath,sample.weight,out);
}

// ---------------------------------------------------------------------------
// Text format. One item per line; '#' starts a comment.
//
//   track <name> <weapon or *>
//     blend_in <seconds>        (optional, default 0.10)
//     blend_out <seconds>       (optional, default 0.15)
//     smooth <0|1>              (optional, default 1)
//     key <time> <weapon|authored|controller> <fwd> <left> <up> <yaw> <pitch> <roll> <weight> [shape]
//   end
//
// A malformed track is dropped whole and reported; the rest still load.

struct ParseReport
{
    unsigned tracks{}, droppedTracks{}, errors{};
    unsigned firstErrorLine{};
    char firstError[96]{};
};

namespace detail
{
inline void Error(ParseReport& report,unsigned line,const char* what) noexcept
{
    if(!report.errors++) { report.firstErrorLine=line;std::snprintf(report.firstError,sizeof(report.firstError),"%s",what); }
}
// Splits a line in place into up to `max` whitespace-separated tokens.
inline unsigned Tokens(char* line,char* tokens[],unsigned max) noexcept
{
    unsigned n=0;char* p=line;
    while(*p&&n<max)
    {
        while(*p==' '||*p=='\t'||*p=='\r') ++p;
        if(!*p||*p=='#') break;
        tokens[n++]=p;
        while(*p&&*p!=' '&&*p!='\t'&&*p!='\r'&&*p!='#') ++p;
        if(*p=='#') { *p=0;break; }
        if(*p) *p++=0;
    }
    return n;
}
inline bool Number(const char* text,float& out) noexcept
{
    char* end=nullptr;const float v=std::strtof(text,&end);
    if(end==text||*end||!std::isfinite(v)) return false;
    out=v;return true;
}
inline bool Copy(char (&dst)[kNameLength],const char* src) noexcept
{
    const size_t n=std::strlen(src);
    if(!n||n>=kNameLength) return false;
    std::memcpy(dst,src,n+1);return true;
}
inline bool InRange(float v,float lo,float hi) noexcept { return v>=lo&&v<=hi; }
}

// Parses `text` (NUL-terminated, modified in place) into `out`. Never fails as
// a whole: bad tracks are left out and counted.
inline ParseReport Parse(char* text,Library& out) noexcept
{
    ParseReport report{};
    out.count=0;
    Track current{};bool open=false,currentBad=false;unsigned openLine=0;
    unsigned lineNumber=0;
    char* cursor=text;
    while(cursor&&*cursor)
    {
        ++lineNumber;
        char* line=cursor;
        char* newline=std::strchr(cursor,'\n');
        if(newline) { *newline=0;cursor=newline+1; } else cursor=nullptr;
        char* t[16];const unsigned n=detail::Tokens(line,t,16);
        if(!n) continue;
        if(std::strcmp(t[0],"track")==0)
        {
            if(open) { detail::Error(report,lineNumber,"track without end");++report.droppedTracks; }
            current=Track{};open=true;currentBad=false;openLine=lineNumber;
            if(n!=3||!detail::Copy(current.name,t[1])||!detail::Copy(current.weapon,t[2]))
            { detail::Error(report,lineNumber,"track needs a name and a weapon (or *)");currentBad=true; }
            continue;
        }
        if(!open) { detail::Error(report,lineNumber,"line outside a track");continue; }
        if(std::strcmp(t[0],"end")==0)
        {
            open=false;
            if(!current.keyCount&&!currentBad) { detail::Error(report,openLine,"track has no keys");currentBad=true; }
            if(!currentBad&&out.count>=kMaxTracks) { detail::Error(report,lineNumber,"too many tracks");currentBad=true; }
            if(currentBad) { ++report.droppedTracks;continue; }
            out.tracks[out.count++]=current;++report.tracks;
            continue;
        }
        if(currentBad) continue;
        float v=0;
        if(std::strcmp(t[0],"blend_in")==0||std::strcmp(t[0],"blend_out")==0)
        {
            if(n!=2||!detail::Number(t[1],v)||!detail::InRange(v,0,kMaxBlend))
            { detail::Error(report,lineNumber,"blend time must be 0 to 2 seconds");currentBad=true;continue; }
            (t[0][6]=='i'?current.blendIn:current.blendOut)=v;
            continue;
        }
        if(std::strcmp(t[0],"smooth")==0)
        {
            if(n!=2||!detail::Number(t[1],v)||(v!=0&&v!=1))
            { detail::Error(report,lineNumber,"smooth must be 0 or 1");currentBad=true;continue; }
            current.smooth=v!=0;
            continue;
        }
        if(std::strcmp(t[0],"key")==0)
        {
            Key key{};
            if(n<10||n>11) { detail::Error(report,lineNumber,"key needs 9 values (and an optional shape)");currentBad=true;continue; }
            if(current.keyCount>=kMaxKeys) { detail::Error(report,lineNumber,"too many keys (16 per track)");currentBad=true;continue; }
            if(std::strcmp(t[2],"weapon")==0) key.frame=Frame::Weapon;
            else if(std::strcmp(t[2],"authored")==0) key.frame=Frame::Authored;
            else if(std::strcmp(t[2],"controller")==0) key.frame=Frame::Controller;
            else { detail::Error(report,lineNumber,"frame must be weapon, authored or controller");currentBad=true;continue; }
            float values[8]{};bool ok=detail::Number(t[1],key.time)&&detail::InRange(key.time,0,kMaxTime);
            for(unsigned i=0;ok&&i<7;++i) ok=detail::Number(t[3+i],values[i]);
            for(unsigned i=0;ok&&i<3;++i) ok=detail::InRange(values[i],-kMaxOffset,kMaxOffset);
            for(unsigned i=3;ok&&i<6;++i) ok=detail::InRange(values[i],-kMaxAngle,kMaxAngle);
            ok=ok&&detail::InRange(values[6],0,1);
            if(!ok) { detail::Error(report,lineNumber,"key value missing or out of range");currentBad=true;continue; }
            if(current.keyCount&&key.time<current.keys[current.keyCount-1].time)
            { detail::Error(report,lineNumber,"key times must not go backwards");currentBad=true;continue; }
            for(int i=0;i<3;++i) { key.offset[i]=values[i];key.rotation[i]=values[3+i]; }
            key.weight=values[6];
            if(n==11)
            {
                unsigned s=0;
                while(s<unsigned(Shape::Count)&&std::strcmp(kShapeNames[s],t[10])!=0) ++s;
                if(s==unsigned(Shape::Count)) { detail::Error(report,lineNumber,"unknown hand shape");currentBad=true;continue; }
                key.shape=Shape(s);
            }
            current.keys[current.keyCount++]=key;
            continue;
        }
        detail::Error(report,lineNumber,"unknown line in a track");currentBad=true;
    }
    if(open) { detail::Error(report,openLine,"track without end");++report.droppedTracks; }
    return report;
}

// The built-in tracks, written to the tracks file the first time and used
// whenever the file cannot be read. `reload` on any weapon puts the support
// hand on the game's own animated hand for the reload: the motion along the
// gun the game already authored for every weapon.
inline constexpr const char* kDefaultText=
"# Halo MCC VR support-hand tracks (Halo 3 and ODST).\n"
"# A track moves the support hand along the held gun for a moment, then hands\n"
"# it back to your controller. Delete this file to get the defaults back; the\n"
"# mod reloads it by itself a couple of seconds after you save it.\n"
"#\n"
"#   track <name> <weapon or *>   weapon = part of the gun's name, e.g. assault_rifle\n"
"#     blend_in <seconds>         time to move off the controller (default 0.10)\n"
"#     blend_out <seconds>        time to return to the controller (default 0.15)\n"
"#     smooth <0|1>               ease between keys (default 1)\n"
"#     key <time> <frame> <fwd> <left> <up> <yaw> <pitch> <roll> <weight> [shape]\n"
"#   end\n"
"#\n"
"# frame:  weapon     = fixed to the gun, measured from the gun hand's wrist\n"
"#         authored   = the game's own animated support hand on that gun\n"
"#         controller = your real support hand\n"
"# fwd/left/up are metres along the gun; yaw/pitch/roll are degrees;\n"
"# weight 0 = stay on the controller, 1 = fully on this key.\n"
"# shape (optional): rest index fist thumbsup point pointdown ok magazine handle\n"
"#\n"
"# Tracks the mod plays: reload (after a gesture reload), preview (F1 button).\n"
"\n"
"track reload *\n"
"  blend_in 0.12\n"
"  blend_out 0.20\n"
"  key 0.00 authored 0 0 0  0 0 0  1\n"
"  key 1.40 authored 0 0 0  0 0 0  1\n"
"end\n"
"\n"
"track preview *\n"
"  blend_in 0.15\n"
"  blend_out 0.25\n"
"  key 0.00 authored 0 0 0  0 0 0  1\n"
"  key 0.60 authored 0 0 -0.08  0 0 0  1\n"
"  key 1.20 authored 0 0 0  0 0 0  1\n"
"  key 1.60 authored 0 0 0  0 0 0  1\n"
"end\n";
}
