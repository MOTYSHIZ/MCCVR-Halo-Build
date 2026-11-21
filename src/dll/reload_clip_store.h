#pragma once
// The guns' learned reload animations (reload_clip_logic.h): recorded by the first-person palette hook, kept for
// the session and in halomccvr_reload_clips.bin beside the settings, so each gun learns its reload once.
//
// Threads: the palette hook (one render thread) records and reads; the log worker loads and saves. Each slot has
// its own state word: a slot being recorded belongs to the hook, a complete slot never changes until the menu
// forgets it, and the worker only reads complete slots. No allocation on the hook's side.
#include "../common/reload_clip_logic.h"
#include "../common/runtime_types.h"
#include <cstdint>

namespace reload_clip_store
{
struct Learned
{
    const reload_clip::Clip* clip = nullptr;
    reload_clip::Segments segments{};
    int magazineNode = -1, rackNode = -1;
};

void SetFolder(const wchar_t* folder);

// The hook: start recording a reload of this gun, unless one of this kind is already learned (or no slot is free).
reload_clip::Clip* BeginRecording(GameTitle title, uint64_t identity, reload_clip::Kind kind) noexcept;
// The hook: the reload ended. A complete one (the game loaded rounds) is analysed and kept; else the slot is freed.
void FinishRecording(reload_clip::Clip* clip, bool complete) noexcept;
// The hook: a learned clip of this gun and kind (complete slots only).
bool Find(GameTitle title, uint64_t identity, reload_clip::Kind kind, Learned& out) noexcept;

// The worker: load the file once, then write it whenever a new clip was learned. Returns a line for the log
// when something happened (empty otherwise).
void Tick(char* line, size_t lineSize) noexcept;
// The menu: forget every learned clip (the file too); each gun learns again on its next reload.
void ForgetAll() noexcept;
// The menu: forget one gun's learned clips (both kinds), so a reload learned badly is learned again.
void Forget(GameTitle title, uint64_t identity) noexcept;
uint32_t LearnedCount() noexcept;
// Clips learned from a reload in this session (not loaded from the file), and the gun of the last one: the worker
// tells the player when a gun has learned its reload. Any thread.
uint32_t LearnedThisSession(uint64_t& lastIdentity) noexcept;
} // namespace reload_clip_store
