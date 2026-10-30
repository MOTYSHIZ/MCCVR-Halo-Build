#pragma once

#include <cstdint>

// WHICH PRESENT CARRIES THE GAME'S PICTURE (d3d11_hook.cpp, IsGameSwapchain).
//
// MCC presents one swapchain. A host can present others beside it: Project Reclaimer has been seen presenting a
// 1x1 swapchain from a thread of its own (its OpenGL screens going out through DXGI) for half a minute before the
// game's swapchain existed, and then both, from two threads. Everything the mod does at a present belongs to the
// game's swapchain alone: the headset's frame, the session it is made on, the host screen's tracking, a resize's
// release of the mod's views. Done for another swapchain it uses one device's immediate context from a second
// thread and copies between two devices, which gets the device removed.
//
// So a present is the game's only when its swapchain belongs to a D3D11 device; in a host that draws the game at
// its own size, when its buffers are bigger than a token surface; and, once the headset's session exists, when
// its device is the session's. Pure logic, unit-tested offline.
namespace present_owner
{
// Smaller than this across or down, a host's swapchain is a token surface, not a picture of the game.
constexpr uint32_t kTokenSurface = 64;

enum class Verdict : uint8_t
{
    Game = 0,
    NotD3D11,     // its device is not a D3D11 device
    TokenSurface, // a host's swapchain too small to be the game's picture
    OtherDevice,  // made on another device than the headset session's
};

constexpr bool IsTokenSurface(uint32_t width, uint32_t height)
{
    return width < kTokenSurface || height < kTokenSurface;
}

// `described`: the swapchain's size was read. `device` / `sessionDevice` are identities (0 = none yet).
constexpr Verdict Classify(bool d3d11, bool hostOwnsRenderSize, bool described, uint32_t width, uint32_t height,
                           uintptr_t device, uintptr_t sessionDevice)
{
    if (!d3d11 || !device)
        return Verdict::NotD3D11;
    if (hostOwnsRenderSize && described && IsTokenSurface(width, height))
        return Verdict::TokenSurface;
    if (sessionDevice && device != sessionDevice)
        return Verdict::OtherDevice;
    return Verdict::Game;
}

constexpr const char* Describe(Verdict verdict)
{
    switch (verdict)
    {
    case Verdict::Game: return "the game's picture";
    case Verdict::NotD3D11: return "not on a D3D11 device";
    case Verdict::TokenSurface: return "too small to be the game's picture (a host's own surface)";
    case Verdict::OtherDevice: return "on another device than the headset session's";
    }
    return "unknown";
}
} // namespace present_owner
