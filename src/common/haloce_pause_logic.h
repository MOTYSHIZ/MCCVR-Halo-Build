#pragma once
#include <cstdint>

namespace halo_ce
{
enum class PauseRequest { None, Enter, Exit };

// Match Halo 3's native-state correction delay. Observe the requested target,
// not the fading presentation: repeatedly requesting the same target would
// restart the comfort fade indefinitely. Unknown samples never imply resume.
struct NativePausePresentation
{
    uint32_t generation{};
    uint64_t mismatchSince{};
    bool mismatchPending{},mismatchValue{};

    PauseRequest Observe(uint32_t currentGeneration,bool known,bool paused,
        bool targetPaused,uint64_t now) noexcept
    {
        if (generation!=currentGeneration)
        { *this={};generation=currentGeneration; }
        if (!currentGeneration||!known||paused==targetPaused)
        { mismatchPending=false;return PauseRequest::None; }
        if (!mismatchPending||mismatchValue!=paused||now<mismatchSince)
        {
            mismatchPending=true;mismatchValue=paused;mismatchSince=now;
            return PauseRequest::None;
        }
        if (now-mismatchSince<50) return PauseRequest::None;
        mismatchPending=false;
        return paused?PauseRequest::Enter:PauseRequest::Exit;
    }
};

// Recent CE eye ownership protects ordinary missed frames from flashing a
// flat image. A completed pause transition explicitly needs the native screen
// and must not wait for that gameplay ownership timeout.
inline bool AllowStockScreen(bool sharedAllows,bool ceOwned,bool paused) noexcept
{ return sharedAllows&&(!ceOwned||paused); }
}
