#pragma once
// A FUNCTION ANOTHER PROGRAM HAS DETOURED FIRST. A host that loads the game's own module may hook a function the mod
// hooks too: Project Reclaimer's client replaces the first 14 bytes of Halo 3's part draw with an absolute jump into
// its own executable (`FF 25 00 00 00 00` and the 8-byte target). The mod's signature, which starts at the function's
// entry, then matches nothing, and the feature is left off. The same function can still be found by everything after
// those 14 bytes, provided what the 14 bytes were is known -- all literal in the signature -- and enough follows them
// to stay unique. The mod's hook then goes in front of the other program's detour, which still runs after it.
//
// Pure: no engine, no allocation beyond the string returned.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace foreign_detour
{
inline constexpr size_t kAbsoluteJumpBytes = 14; // FF 25 00 00 00 00, then the 8-byte target
inline constexpr size_t kMinimumTailBytes = 16;  // what must follow the jump for the match to mean anything

// `pattern` (space-separated hex bytes, "??" a wildcard) with its first 14 bytes replaced by an absolute jump to
// anywhere. Empty when those 14 bytes are not all literal in `pattern`, or fewer than kMinimumTailBytes follow them.
inline std::string JumpedPattern(std::string_view pattern)
{
    std::string tail;
    size_t token = 0, literalHead = 0, tailTokens = 0;
    size_t at = 0;
    while (at < pattern.size())
    {
        while (at < pattern.size() && pattern[at] == ' ')
            ++at;
        if (at >= pattern.size())
            break;
        size_t end = pattern.find(' ', at);
        if (end == std::string_view::npos)
            end = pattern.size();
        const std::string_view byte = pattern.substr(at, end - at);
        at = end;
        if (token < kAbsoluteJumpBytes)
        {
            if (byte.size() == 2 && byte != "??")
                ++literalHead;
        }
        else
        {
            tail += ' ';
            tail += byte;
            ++tailTokens;
        }
        ++token;
    }
    if (literalHead != kAbsoluteJumpBytes || tailTokens < kMinimumTailBytes)
        return {};
    return "FF 25 00 00 00 00 ?? ?? ?? ?? ?? ?? ?? ??" + tail;
}

// The jump's target, from the 14 bytes at a match of JumpedPattern.
inline uint64_t JumpTarget(const uint8_t* bytes)
{
    uint64_t target = 0;
    std::memcpy(&target, bytes + 6, sizeof(target));
    return target;
}
} // namespace foreign_detour
