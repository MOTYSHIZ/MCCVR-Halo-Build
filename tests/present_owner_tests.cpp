// Which present carries the game's picture (src/common/present_owner_logic.h): a D3D11 swapchain, not a host's
// token surface, and on the headset session's device once there is one.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include "../src/common/present_owner_logic.h"

using namespace present_owner;

static unsigned g_checks = 0;
static void Check(bool condition, const char* message)
{
    ++g_checks;
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

int main()
{
    constexpr uintptr_t kGameDevice = 0x1000, kOtherDevice = 0x2000;

    // MCC: one swapchain, before and after the session.
    Check(Classify(true, false, true, 2912, 2100, kGameDevice, 0) == Verdict::Game, "MCC's swapchain before the session");
    Check(Classify(true, false, true, 2912, 2100, kGameDevice, kGameDevice) == Verdict::Game,
          "MCC's swapchain on the session's device");
    // MCC never has a host's token rule: a small swapchain there stays the game's.
    Check(Classify(true, false, true, 1, 1, kGameDevice, 0) == Verdict::Game, "no token rule outside a host");

    // Project Reclaimer: the 1x1 surface that presents first is never the game's, before or after the session.
    Check(Classify(true, true, true, 1, 1, kOtherDevice, 0) == Verdict::TokenSurface, "a host's 1x1 surface");
    Check(Classify(true, true, true, 1, 1, kGameDevice, kGameDevice) == Verdict::TokenSurface,
          "a host's 1x1 surface on the session's own device");
    Check(Classify(true, true, true, 63, 2100, kGameDevice, 0) == Verdict::TokenSurface, "63 across is a token");
    Check(Classify(true, true, true, 2912, 63, kGameDevice, 0) == Verdict::TokenSurface, "63 down is a token");
    Check(Classify(true, true, true, 64, 64, kGameDevice, 0) == Verdict::Game, "64 by 64 is a picture");
    Check(Classify(true, true, true, 2912, 2100, kGameDevice, 0) == Verdict::Game, "the game's swapchain, first");
    // An unread size is not taken for a token: the device rule still applies.
    Check(Classify(true, true, false, 0, 0, kGameDevice, 0) == Verdict::Game, "an unread size is not a token");

    // Another device than the session's is never the game's, in any host.
    Check(Classify(true, true, true, 2912, 2100, kOtherDevice, kGameDevice) == Verdict::OtherDevice,
          "a host's picture on another device");
    Check(Classify(true, false, true, 2912, 2100, kOtherDevice, kGameDevice) == Verdict::OtherDevice,
          "MCC's picture on another device");

    // Not D3D11.
    Check(Classify(false, false, true, 2912, 2100, kGameDevice, 0) == Verdict::NotD3D11, "not a D3D11 swapchain");
    Check(Classify(true, false, true, 2912, 2100, 0, 0) == Verdict::NotD3D11, "no device at all");

    Check(IsTokenSurface(1, 1) && !IsTokenSurface(kTokenSurface, kTokenSurface), "the token bound");
    for (Verdict verdict : {Verdict::Game, Verdict::NotD3D11, Verdict::TokenSurface, Verdict::OtherDevice})
        Check(Describe(verdict) && std::strcmp(Describe(verdict), "unknown") != 0, "every verdict is described");

    std::printf("present_owner: %u checks passed\n", g_checks);
    return 0;
}
