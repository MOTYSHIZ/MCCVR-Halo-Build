#pragma once
#include "../common/runtime_types.h"
#include <cstdint>
#include <memory>
#include <vector>

struct NativeSubtitleImage
{
    static constexpr uint32_t width = 2048, height = 512;
    GameTitle title = GameTitle::None;
    uint32_t generation = 0;
    uint64_t expiresAtMs = 0, revision = 0;
    bool theatre = false;
    std::shared_ptr<const std::vector<uint8_t>> rgba;
};

// Worker only: install the independent native observer, retire stale captions,
// and rasterize changed text. No work is added to title camera ownership.
void NativeSubtitles_Poll(bool levelRunning);
std::shared_ptr<const NativeSubtitleImage> NativeSubtitles_Read();
void NativeSubtitles_SetReachSources(uint32_t generation,
    uintptr_t gameplayReturn, uintptr_t theatreReturn) noexcept;

// THE NOTICE: a short line of the mod's own for the headset (the vehicle camera in use changed), shown for
// `seconds` in a panel of its own, apart from the captions: it never replaces dialogue and is not subject to the
// caption settings. Any thread; the text is copied and the newest replaces the last. The worker's poll rasterizes
// it, and NativeNotice_Read gives the render thread the image until it expires.
//
// It is drawn in the style of Halo 3's HUD (user 2026-11-21: the game's own blue messages are too short for
// instructions, so this panel is the mod's primary message for things like the reload calibrations): the text in the
// HUD's light blue with a soft glow, on a dark see-through bar with bracket corners, under an optional small heading.
// A warning is drawn in amber. Showing the same heading, text and tone again only extends the time it stays (no new
// picture is drawn), so a caller may repeat it.
enum class NoticeTone : uint8_t { Info = 0, Warning = 1 };
void NativeNotice_Show(const wchar_t* text, float seconds) noexcept;
// Returns the notice's number, for NativeNotice_Hide.
uint64_t NativeNotice_Show(const wchar_t* heading, const wchar_t* text, float seconds, NoticeTone tone) noexcept;
// Takes this notice away now, unless another has replaced it (an instruction whose step was cancelled).
void NativeNotice_Hide(uint64_t notice) noexcept;
std::shared_ptr<const NativeSubtitleImage> NativeNotice_Read();
// THE LABELS: short words for waypoints of the mod's own (the manual reload's prompts: "PULL" at the charging handle,
// "MAG" at the pouch), each in a slot of its own so switching between them never waits on the rasterizer. A label
// stays until changed (an empty text clears it); the worker rasterizes it, the render thread places it.
inline constexpr int kNativeLabelSlots = 4;   // PULL, MAG, WAIT, SHELL
void NativeLabel_Set(int slot, const wchar_t* text, float opacity) noexcept;
std::shared_ptr<const NativeSubtitleImage> NativeLabel_Read(int slot);

// Bounded, nonblocking ingress for independently verified title sources.
// Copies UTF-16 immediately; never retains an engine-owned string pointer.
void NativeSubtitles_Capture(GameTitle title, uint32_t generation,
    const wchar_t* text, float seconds, bool theatre,
    uint8_t channel=0, bool refreshed=false) noexcept;
