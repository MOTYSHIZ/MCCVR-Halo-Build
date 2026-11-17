#pragma once
// halomccvr_reloads.txt beside the settings (reload_weapon_file.h): read and written by the log worker (Poll), read
// by the VR thread (Get) and changed by the menu and the in-headset calibrations (Set*), which only queue a change.
#include "../common/reload_weapon_file.h"

namespace reload_weapon_settings
{
void SetFolder(const wchar_t* folder);
// The worker: reload the file when it changed, apply queued changes and write it. A line for the log, or empty.
void Poll(char* line, size_t lineSize) noexcept;
// Any thread: the gun's line, if the file has one.
bool Get(const char* name, reload_weapon_file::Entry& out) noexcept;
// Any thread: queue a change to a gun's line (applied and saved by the next Poll).
void SetEnabled(const char* name, bool enabled) noexcept;
void SetHandle(const char* name, const float cm[3]) noexcept;
void ClearHandle(const char* name) noexcept;
void SetPouch(const char* name, float cm) noexcept;      // this gun's reach at the pouch
void ClearPouch(const char* name) noexcept;              // back to the global "Magazine grab radius"
void SetSeat(const char* name, float cm) noexcept;       // this gun's receiver reach (how close to the well's line)
void ClearSeat(const char* name) noexcept;               // back to the built-in 10 cm
} // namespace reload_weapon_settings
