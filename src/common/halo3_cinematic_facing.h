#pragma once

#include "runtime_types.h"
#include <cstdint>

// Automatic yaw alignment requires an identified authored shot. Cortana's
// effect flag alone is not a camera cut. Unknown reads are not cinematic exits.
// This state belongs only to the Halo 3 camera thread and its current generation.
struct Halo3CinematicFacing
{
    uint32_t generation{};
    bool inShot{};
    int32_t scene{-1}, shot{-1};

    bool Observe(uint32_t currentGeneration, CinematicControlState control,
        int32_t currentScene, int32_t currentShot) noexcept
    {
        if (generation != currentGeneration)
        {
            *this = {};
            generation = currentGeneration;
        }
        if (!generation || control == CinematicControlState::Unknown)
            return false;
        if (control == CinematicControlState::AuthoredLocked)
        {
            if (currentScene < 0 || currentShot < 0)
                return false;
            const bool boundary = !inShot || scene != currentScene || shot != currentShot;
            inShot = true;
            scene = currentScene;
            shot = currentShot;
            return boundary;
        }
        if (control != CinematicControlState::PlayerControlled)
            return false;
        const bool exit = inShot;
        inShot = false;
        scene = shot = -1;
        return exit;
    }
};
