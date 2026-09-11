#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "runtime_types.h"

// Roomscale has its own positive native on-foot camera proof. The historical
// shared-feature gate is H3-only and is not this feature's admission policy.
inline bool RoomscaleGameplayEligible(GameTitle title, RuntimeMode mode) noexcept
{
    return mode == RuntimeMode::Gameplay &&
        (title == GameTitle::Halo2 || title == GameTitle::Halo3 ||
         title == GameTitle::Halo3ODST || title == GameTitle::HaloReach ||
         title == GameTitle::Halo4);
}

// Native locomotion owns collision, steps, ground support and networking.
// Consume only observed motion toward our request from the tracking reference:
// body position + remaining tracked lean then represents one physical step.
struct RoomscaleFollow
{
    bool seeded=false, commanded=false;
    uint32_t generation=0;
    uint64_t time=0;
    float body[2]{}, expectedReference[2]{}, initialLean[2]{};
    float headForward[2]{}, worldForward[2]{}, error[2]{};

    bool Update(uint32_t epoch,uint64_t now,bool enabled,bool manualMove,
        const float position[3],const float head[3],float reference[3],
        float hx,float hz,float wx,float wy,float scale,float& moveX,float& moveY) noexcept
    {
        moveX=moveY=0;
        const float values[]{position[0],position[1],head[0],head[2],reference[0],
            reference[2],hx,hz,wx,wy,scale};
        for (float v:values) if (!std::isfinite(v)) { seeded=false; return false; }
        const float hl=std::hypot(hx,hz),wl=std::hypot(wx,wy);
        if (!enabled || !epoch || !now || scale<=0 || hl<0.001f || wl<0.001f)
        { seeded=false; commanded=false; return false; }
        hx/=hl; hz/=hl; wx/=wl; wy/=wl;
        const bool reset=!seeded || generation!=epoch || now<time || now-time>250 ||
            std::fabs(reference[0]-expectedReference[0])>0.0001f ||
            std::fabs(reference[2]-expectedReference[1])>0.0001f ||
            std::hypot(position[0]-body[0],position[1]-body[1])/scale>0.35f;
        if (reset || manualMove)
        {
            initialLean[0]=head[0]-reference[0];
            initialLean[1]=head[2]-reference[2];
            commanded=false; seeded=true;
        }
        else if (commanded && !manualMove)
        {
            const float dx=(position[0]-body[0])/scale;
            const float dy=(position[1]-body[1])/scale;
            const float forward=dx*worldForward[0]+dy*worldForward[1];
            const float right=dx*worldForward[1]-dy*worldForward[0];
            const float tx=forward*headForward[0]-right*headForward[1];
            const float tz=forward*headForward[1]+right*headForward[0];
            const float length=std::hypot(error[0],error[1]);
            const float distance=std::hypot(tx,tz);
            // Reject teleports and unrelated sideways movement. Include a
            // bounded native stopping step so it does not move the view twice.
            if (length>0.001f && distance<=length+0.05f &&
                tx*error[0]+tz*error[1]>0)
            { reference[0]+=tx; reference[2]+=tz; }
        }
        error[0]=head[0]-reference[0]-initialLean[0];
        error[1]=head[2]-reference[2]-initialLean[1];
        const float distance=std::hypot(error[0],error[1]);
        if (distance>1.2f)
        {
            // A tracking jump must not become a long unattended walk.
            initialLean[0]=head[0]-reference[0];
            initialLean[1]=head[2]-reference[2];
            error[0]=error[1]=0;
        }
        else if (!manualMove && distance>0.02f)
        {
            const float magnitude=std::min(0.8f,distance*3.0f);
            moveX=(-error[0]*hz+error[1]*hx)/distance*magnitude;
            moveY=(error[0]*hx+error[1]*hz)/distance*magnitude;
        }
        commanded=moveX!=0 || moveY!=0;
        generation=epoch; time=now;
        body[0]=position[0]; body[1]=position[1];
        expectedReference[0]=reference[0]; expectedReference[1]=reference[2];
        headForward[0]=hx; headForward[1]=hz;
        worldForward[0]=wx; worldForward[1]=wy;
        return true;
    }
};
