#include "hand_tracks.h"
#include "../common/config.h"
#include "../common/log.h"
#include "../common/weapon_model_catalog.h"
#include <windows.h>
#include <atomic>
#include <cstring>
#include <string>

namespace
{
constexpr wchar_t kFileName[] = L"halomccvr_hand_tracks.txt";
constexpr uint64_t kCheckIntervalMs = 2000;
constexpr DWORD kMaxFileBytes = 256 * 1024;
constexpr unsigned kSlots = 4;

// --- library (written by the worker, read by the VR thread) ---------------
std::wstring g_path;
hand_track::Library g_libraries[2];
hand_track::Library g_parsed;          // worker scratch
std::atomic<unsigned> g_activeLibrary{0};
std::atomic<uint32_t> g_libraryVersion{0}; // odd while a library is being written
std::atomic<bool> g_libraryReady{false};
uint64_t g_nextCheckMs = 0;
bool g_fileSeen = false;
FILETIME g_lastWrite{};
uint64_t g_lastSize = ~0ull;
bool g_createTried = false;
bool g_builtInLoaded = false;

// --- status (menu) ---------------------------------------------------------
SRWLOCK g_statusLock = SRWLOCK_INIT;
HandTracksStatus g_status{};
unsigned g_loggedPlays = 0;

// --- playback (VR thread owns these) --------------------------------------
std::atomic<uint32_t> g_request{0};
hand_track::Track g_slots[kSlots];
hand_track::PoseTable g_slotPoses[kSlots];
unsigned g_slotNext = 0, g_playSlot = 0;
bool g_playing = false;
uint64_t g_startMs = 0;
std::atomic<unsigned> g_plays{0};
// bit 63 active, bits 32..33 slot, low 32 elapsed seconds as float bits.
std::atomic<uint64_t> g_published{0};
std::atomic<bool> g_playingFlag{false};
std::atomic<unsigned> g_appliedSolves{0};
bool g_loggedPlaying = false;

void Publish(hand_track::Library& source, bool fromFile, const hand_track::ParseReport& report)
{
    const unsigned target = 1u - g_activeLibrary.load(std::memory_order_acquire);
    g_libraryVersion.fetch_add(1, std::memory_order_acq_rel);
    g_libraries[target] = source;
    g_activeLibrary.store(target, std::memory_order_release);
    g_libraryVersion.fetch_add(1, std::memory_order_acq_rel);
    g_libraryReady.store(true, std::memory_order_release);
    AcquireSRWLockExclusive(&g_statusLock);
    g_status.loaded = true;
    g_status.fromFile = fromFile;
    g_status.tracks = report.tracks;
    g_status.dropped = report.droppedTracks;
    g_status.errors = report.errors;
    g_status.firstErrorLine = report.firstErrorLine;
    memcpy(g_status.firstError, report.firstError, sizeof(g_status.firstError));
    ReleaseSRWLockExclusive(&g_statusLock);
}

void LoadBuiltIn()
{
    static char text[8192];
    const size_t n = strlen(hand_track::kDefaultText);
    if (n >= sizeof(text)) return;
    memcpy(text, hand_track::kDefaultText, n + 1);
    const auto report = hand_track::Parse(text, g_parsed);
    Publish(g_parsed, false, report);
    g_builtInLoaded = true;
    LOG("Hand tracks: using %u built-in support-hand tracks", report.tracks);
}

void WriteDefaultFile()
{
    if (g_createTried || g_path.empty()) return;
    g_createTried = true;
    const HANDLE file = CreateFileW(g_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    const DWORD size = DWORD(strlen(hand_track::kDefaultText));
    const BOOL ok = WriteFile(file, hand_track::kDefaultText, size, &written, nullptr);
    CloseHandle(file);
    LOG("Hand tracks: %s %ls with the built-in tracks", ok && written == size ? "wrote" : "could not fully write",
        kFileName);
}

bool LoadFile(uint64_t size)
{
    if (size == 0 || size > kMaxFileBytes) return false;
    const HANDLE file = CreateFileW(g_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    std::string text(size_t(size) + 1, '\0');
    DWORD read = 0;
    const BOOL ok = ReadFile(file, text.data(), DWORD(size), &read, nullptr);
    CloseHandle(file);
    if (!ok) return false;
    text.resize(read);
    text.push_back('\0');
    const auto report = hand_track::Parse(text.data(), g_parsed);
    Publish(g_parsed, true, report);
    if (report.errors)
        LOG("Hand tracks: loaded %u tracks from %ls; %u dropped, %u problems (first at line %u: %s)",
            report.tracks, kFileName, report.droppedTracks, report.errors, report.firstErrorLine,
            report.firstError);
    else
        LOG("Hand tracks: loaded %u tracks from %ls", report.tracks, kFileName);
    return true;
}

const char* WeaponName(GameTitle title, uint64_t identity)
{
    const auto* model = weapon_model::Find(title, identity);
    return model ? model->name : nullptr;
}

bool Start(const char* name, GameTitle title, uint64_t identity, uint64_t nowMs)
{
    if (!g_libraryReady.load(std::memory_order_acquire)) return false;
    const uint32_t before = g_libraryVersion.load(std::memory_order_acquire);
    if (before & 1u) return false;
    const auto& library = g_libraries[g_activeLibrary.load(std::memory_order_acquire)];
    const char* weapon = WeaponName(title, identity);
    const hand_track::Track* track = hand_track::Find(library, name, weapon);
    if (!track) return false;
    const unsigned slot = g_slotNext;
    g_slots[slot] = *track;
    g_slotPoses[slot] = library.poses;
    if (g_libraryVersion.load(std::memory_order_acquire) != before) return false;
    g_slotNext = (g_slotNext + 1) % kSlots;
    g_playSlot = slot;
    g_startMs = nowMs;
    g_playing = true;
    AcquireSRWLockExclusive(&g_statusLock);
    memcpy(g_status.playingName, g_slots[slot].name, sizeof(g_status.playingName));
    if (weapon)
        strncpy_s(g_status.playingWeapon, weapon, _TRUNCATE);
    else
        strcpy_s(g_status.playingWeapon, "unknown weapon");
    ReleaseSRWLockExclusive(&g_statusLock);
    g_plays.fetch_add(1, std::memory_order_release);
    return true;
}

void StopPlaying()
{
    g_playing = false;
    g_published.store(0, std::memory_order_release);
    g_playingFlag.store(false, std::memory_order_relaxed);
}
}

void HandTracks_SetFolder(const wchar_t* folder)
{
    if (!folder) return;
    g_path = folder;
    if (!g_path.empty() && g_path.back() != L'\\' && g_path.back() != L'/') g_path += L'\\';
    g_path += kFileName;
}

void HandTracks_Poll(uint64_t nowMs)
{
    const unsigned plays = g_plays.load(std::memory_order_acquire);
    if (plays != g_loggedPlays)
    {
        g_loggedPlays = plays;
        HandTracksStatus status{};
        HandTracks_GetStatus(status);
        LOG("Hand tracks: playing '%s' on the support hand (%s)", status.playingName, status.playingWeapon);
        g_appliedSolves.store(0, std::memory_order_relaxed);
        g_loggedPlaying = true;
    }
    else if (g_loggedPlaying && !g_playingFlag.load(std::memory_order_relaxed))
    {
        g_loggedPlaying = false;
        // Zero means the arm solver never took the track (arm IK off, a two-handed
        // grip, or no first-person arms this frame).
        LOG("Hand tracks: finished; the support-hand target followed the track in %u arm solves",
            g_appliedSolves.exchange(0, std::memory_order_relaxed));
    }
    if (nowMs < g_nextCheckMs) return;
    g_nextCheckMs = nowMs + kCheckIntervalMs;
    if (g_path.empty())
    {
        if (!g_builtInLoaded) LoadBuiltIn();
        return;
    }
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(g_path.c_str(), GetFileExInfoStandard, &data))
    {
        if (g_fileSeen)
            LOG("Hand tracks: %ls was removed; using the built-in tracks", kFileName);
        g_fileSeen = false;
        g_lastSize = ~0ull;
        if (!g_builtInLoaded || g_status.fromFile) LoadBuiltIn();
        WriteDefaultFile();
        return;
    }
    const uint64_t size = (uint64_t(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    if (g_fileSeen && size == g_lastSize && CompareFileTime(&data.ftLastWriteTime, &g_lastWrite) == 0) return;
    g_fileSeen = true;
    g_lastSize = size;
    g_lastWrite = data.ftLastWriteTime;
    if (!LoadFile(size))
    {
        LOG("Hand tracks: could not read %ls (%llu bytes); keeping the tracks already in use", kFileName,
            static_cast<unsigned long long>(size));
        if (!g_libraryReady.load(std::memory_order_acquire)) LoadBuiltIn();
    }
    g_createTried = false;
}

void HandTracks_Request(HandTrackRequest request)
{
    g_request.store(uint32_t(request), std::memory_order_release);
}

void HandTracks_Update(uint64_t nowMs, GameTitle title, uint64_t weaponIdentity, bool dualWield)
{
    const auto request = HandTrackRequest(g_request.exchange(0, std::memory_order_acq_rel));
    const bool allowed = g_config.support_hand_tracks && !dualWield &&
        (title == GameTitle::Halo3 || title == GameTitle::Halo3ODST);
    if (!allowed)
    {
        if (g_playing || g_published.load(std::memory_order_relaxed)) StopPlaying();
        return;
    }
    if (request == HandTrackRequest::Reload)
        Start("reload", title, weaponIdentity, nowMs);
    else if (request == HandTrackRequest::Preview)
        Start("preview", title, weaponIdentity, nowMs);
    if (!g_playing)
    {
        if (g_published.load(std::memory_order_relaxed)) StopPlaying();
        return;
    }
    const float elapsed = float(nowMs - g_startMs) / 1000.0f;
    if (hand_track::Evaluate(g_slots[g_playSlot], elapsed).finished)
    {
        StopPlaying();
        return;
    }
    uint32_t bits = 0;
    memcpy(&bits, &elapsed, sizeof(bits));
    g_published.store((1ull << 63) | (uint64_t(g_playSlot & 3u) << 32) | bits, std::memory_order_release);
    g_playingFlag.store(true, std::memory_order_relaxed);
}

bool HandTracks_Current(hand_track::Sampled& out)
{
    const uint64_t value = g_published.load(std::memory_order_acquire);
    if (!(value >> 63)) return false;
    const unsigned slot = unsigned(value >> 32) & 3u;
    const uint32_t bits = uint32_t(value);
    float elapsed = 0;
    memcpy(&elapsed, &bits, sizeof(elapsed));
    out = hand_track::Evaluate(g_slots[slot], elapsed);
    return out.active;
}

bool HandTracks_CurrentCurls(const float controller[hand_track::kFingers], float out[hand_track::kFingers])
{
    const uint64_t value = g_published.load(std::memory_order_acquire);
    if (!(value >> 63)) return false;
    const unsigned slot = unsigned(value >> 32) & 3u;
    const uint32_t bits = uint32_t(value);
    float elapsed = 0;
    memcpy(&elapsed, &bits, sizeof(elapsed));
    const auto sample = hand_track::Evaluate(g_slots[slot], elapsed);
    return hand_track::Curls(g_slotPoses[slot], sample, controller, out);
}

void HandTracks_NoteApplied()
{
    g_appliedSolves.fetch_add(1, std::memory_order_relaxed);
}

void HandTracks_GetStatus(HandTracksStatus& out)
{
    AcquireSRWLockShared(&g_statusLock);
    out = g_status;
    ReleaseSRWLockShared(&g_statusLock);
    out.playing = g_playingFlag.load(std::memory_order_relaxed);
    out.plays = g_plays.load(std::memory_order_relaxed);
}
