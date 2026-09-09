#pragma once
#include <cstdint>

// Worker-side retirement only. Never authorizes installation or stereo: the
// title's normal load gate and fresh-camera debounce must prove those again.
constexpr bool CameraNeedsRecovery(uint64_t now, uint64_t installedAt,
    uint64_t lastCamera, bool installed) noexcept
{
    const uint64_t reference = lastCamera > installedAt ? lastCamera : installedAt;
    return installed && reference && now >= reference && now - reference >= 2000;
}
