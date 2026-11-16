#include "reload_clip_store.h"

#include "../common/manual_reload_parts.h"

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace reload_clip_store
{
namespace
{
constexpr wchar_t kFileName[] = L"halomccvr_reload_clips.bin";
constexpr uint32_t kMagic = 0x4352484Du;   // "MHRC"
constexpr uint32_t kVersion = 1;
constexpr int kSlots = 16;

enum State : int { Free = 0, Recording = 1, Complete = 2, Loading = 3 };

struct Slot
{
    std::atomic<int> state{Free};
    std::atomic<uint32_t> sequence{0};   // bumped whenever the slot is claimed (a reader checks it did not move)
    std::atomic<bool> saved{false};      // written to the file (the hook clears it on a new recording)
    std::atomic<uint32_t> reported{0};   // the sequence whose clip the log has described
    GameTitle title{};
    reload_clip::Clip clip{};
    reload_clip::Segments segments{};
    int magazineNode = -1, rackNode = -1;
};
Slot g_slots[kSlots];
std::wstring g_path;
std::atomic<bool> g_loaded{false};
std::atomic<bool> g_forget{false};
std::atomic<uint32_t> g_learned{0};

void Analyze(Slot& s) noexcept
{
    const auto* parts = manual_reload::FindParts(s.title, s.clip.identity);
    s.magazineNode = parts ? parts->magazineNode : -1;
    s.rackNode = parts ? parts->rackNode : -1;
    s.segments = reload_clip::Analyze(s.clip, s.magazineNode, s.rackNode);
}

void CountLearned() noexcept
{
    uint32_t n = 0;
    for (const Slot& s : g_slots) if (s.state.load(std::memory_order_acquire) == Complete) ++n;
    g_learned.store(n, std::memory_order_relaxed);
}

bool ReadAll(std::vector<uint8_t>& bytes)
{
    const HANDLE f = CreateFileW(g_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(f, &size) && size.QuadPart > 0 && size.QuadPart < (64ll << 20);
    if (ok)
    {
        bytes.resize(size_t(size.QuadPart));
        DWORD got = 0;
        ok = ReadFile(f, bytes.data(), DWORD(bytes.size()), &got, nullptr) && got == bytes.size();
    }
    CloseHandle(f);
    return ok;
}

// Loads every clip in the file into free slots. Returns how many.
int Load(char* why, size_t whySize)
{
    std::vector<uint8_t> bytes;
    if (!ReadAll(bytes)) { snprintf(why, whySize, "no file yet"); return 0; }
    size_t at = 0;
    auto take = [&](void* out, size_t n) {
        if (at + n > bytes.size()) return false;
        std::memcpy(out, bytes.data() + at, n);
        at += n;
        return true;
    };
    uint32_t magic = 0, version = 0, count = 0;
    if (!take(&magic, 4) || !take(&version, 4) || !take(&count, 4) || magic != kMagic || version != kVersion ||
        count > uint32_t(kSlots))
    {
        snprintf(why, whySize, "not a clip file of version %u: ignored", kVersion);
        return 0;
    }
    int loaded = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        uint8_t title = 0, kind = 0;
        uint64_t identity = 0;
        int32_t nodes = 0, frames = 0;
        if (!take(&title, 1) || !take(&kind, 1) || !take(&identity, 8) || !take(&nodes, 4) || !take(&frames, 4) ||
            nodes <= 0 || nodes > reload_clip::kMaxNodes || frames < 2 || frames > reload_clip::kMaxFrames || kind > 1)
        {
            snprintf(why, whySize, "clip %u is damaged: the rest ignored", i);
            break;
        }
        Slot* slot = nullptr;
        for (Slot& s : g_slots)
        {
            int expected = Free;
            if (s.state.compare_exchange_strong(expected, Loading, std::memory_order_acq_rel)) { slot = &s; break; }
        }
        if (!slot) break;
        slot->sequence.fetch_add(1, std::memory_order_acq_rel);
        slot->clip = reload_clip::Clip{};
        slot->clip.identity = identity;
        slot->clip.title = title;
        slot->clip.kind = reload_clip::Kind(kind);
        slot->clip.nodes = nodes;
        slot->clip.frames = frames;
        bool ok = take(slot->clip.time, sizeof(float) * size_t(frames));
        for (int f = 0; ok && f < frames; ++f)
            ok = take(slot->clip.bone[f], sizeof(reload_clip::Bone) * size_t(nodes));
        if (!ok)
        {
            slot->state.store(Free, std::memory_order_release);
            snprintf(why, whySize, "clip %u is cut short: the rest ignored", i);
            break;
        }
        slot->title = GameTitle(title);
        Analyze(*slot);
        slot->saved = true;
        slot->state.store(Complete, std::memory_order_release);
        ++loaded;
    }
    return loaded;
}

bool Save(int& written)
{
    // Copy each complete slot out (checking it did not change meanwhile), then write the file whole.
    std::vector<uint8_t> out;
    auto put = [&](const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        out.insert(out.end(), b, b + n);
    };
    const uint32_t magic = kMagic, version = kVersion;
    uint32_t count = 0;
    put(&magic, 4); put(&version, 4); put(&count, 4);
    auto copy = std::make_unique<reload_clip::Clip>();
    written = 0;
    for (Slot& s : g_slots)
    {
        if (s.state.load(std::memory_order_acquire) != Complete) continue;
        const uint32_t before = s.sequence.load(std::memory_order_acquire);
        *copy = s.clip;
        if (s.state.load(std::memory_order_acquire) != Complete || s.sequence.load(std::memory_order_acquire) != before)
            continue;
        const uint8_t title = copy->title, kind = uint8_t(copy->kind);
        const int32_t nodes = copy->nodes, frames = copy->frames;
        put(&title, 1); put(&kind, 1); put(&copy->identity, 8); put(&nodes, 4); put(&frames, 4);
        put(copy->time, sizeof(float) * size_t(frames));
        for (int f = 0; f < frames; ++f) put(copy->bone[f], sizeof(reload_clip::Bone) * size_t(nodes));
        s.saved = true;
        ++count;
    }
    std::memcpy(out.data() + 8, &count, 4);
    written = int(count);
    const std::wstring temp = g_path + L".tmp";
    const HANDLE f = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0;
    const bool ok = WriteFile(f, out.data(), DWORD(out.size()), &wrote, nullptr) && wrote == out.size();
    CloseHandle(f);
    return ok && MoveFileExW(temp.c_str(), g_path.c_str(), MOVEFILE_REPLACE_EXISTING);
}
} // namespace

void SetFolder(const wchar_t* folder)
{
    g_path = folder ? folder : L"";
    if (!g_path.empty() && g_path.back() != L'\\' && g_path.back() != L'/') g_path += L'\\';
    g_path += kFileName;
}

reload_clip::Clip* BeginRecording(GameTitle title, uint64_t identity, reload_clip::Kind kind) noexcept
{
    for (const Slot& s : g_slots)
    {
        const int state = s.state.load(std::memory_order_acquire);
        if ((state == Complete || state == Recording) && s.clip.identity == identity && s.clip.kind == kind &&
            s.title == title) return nullptr;
    }
    for (Slot& s : g_slots)
    {
        int expected = Free;
        if (!s.state.compare_exchange_strong(expected, Recording, std::memory_order_acq_rel)) continue;
        s.sequence.fetch_add(1, std::memory_order_acq_rel);
        s.saved = false;
        s.title = title;
        s.clip.identity = identity;
        s.clip.title = uint8_t(title);
        s.clip.kind = kind;
        s.clip.nodes = 0;
        s.clip.frames = 0;
        return &s.clip;
    }
    return nullptr;
}

void FinishRecording(reload_clip::Clip* clip, bool complete) noexcept
{
    for (Slot& s : g_slots)
    {
        if (&s.clip != clip || s.state.load(std::memory_order_acquire) != Recording) continue;
        if (complete && clip->frames >= 4)
        {
            Analyze(s);
            // A clip that shows neither the magazine's way in nor a rack stroke teaches nothing.
            if (s.segments.magazine || s.segments.rack)
            {
                s.state.store(Complete, std::memory_order_release);
                CountLearned();
                return;
            }
        }
        s.state.store(Free, std::memory_order_release);
        return;
    }
}

bool Find(GameTitle title, uint64_t identity, reload_clip::Kind kind, Learned& out) noexcept
{
    for (const Slot& s : g_slots)
        if (s.state.load(std::memory_order_acquire) == Complete && s.clip.identity == identity &&
            s.clip.kind == kind && s.title == title)
        {
            out.clip = &s.clip;
            out.segments = s.segments;
            out.magazineNode = s.magazineNode;
            out.rackNode = s.rackNode;
            return true;
        }
    return false;
}

void Tick(char* line, size_t lineSize) noexcept
{
    if (line && lineSize) line[0] = 0;
    if (g_path.empty() || !line || !lineSize) return;
    try
    {
        if (!g_loaded.exchange(true))
        {
            char why[96]{};
            const int n = Load(why, sizeof(why));
            CountLearned();
            snprintf(line, lineSize, "Reload animations: %d learned clip(s) loaded from halomccvr_reload_clips.bin%s%s",
                     n, why[0] ? " -- " : "", why);
            return;
        }
        if (g_forget.exchange(false))
        {
            for (Slot& s : g_slots)
            {
                int expected = Complete;
                s.state.compare_exchange_strong(expected, Free, std::memory_order_acq_rel);
            }
            CountLearned();
            DeleteFileW(g_path.c_str());
            snprintf(line, lineSize, "Reload animations: forgotten; each gun learns its reload again");
            return;
        }
        for (Slot& s : g_slots)
        {
            // Each clip learned is described once: when its magazine leaves and comes back, and its rack stroke.
            if (s.state.load(std::memory_order_acquire) != Complete) continue;
            const uint32_t sequence = s.sequence.load(std::memory_order_acquire);
            if (s.reported.exchange(sequence) == sequence) continue;
            const auto& g = s.segments;
            snprintf(line, lineSize,
                     "Reload animation learned: model=%016llX %s reload, %d frames over %.2f s, %d nodes; magazine %s "
                     "(out %.2f-%.2f s, in %.2f-%.2f s from %.4f units, axis %.2f %.2f %.2f); rack %s (%.2f-%.2f s, "
                     "%.4f units, back %.2f %.2f %.2f); magazine node %d, rack node %d",
                     static_cast<unsigned long long>(s.clip.identity),
                     s.clip.kind == reload_clip::Kind::Empty ? "empty" : "full", s.clip.frames,
                     s.clip.frames ? s.clip.time[s.clip.frames - 1] : 0.f, s.clip.nodes, g.magazine ? "yes" : "no",
                     g.outStart, g.outEnd, g.in, g.seat, g.window, g.approach.x, g.approach.y, g.approach.z,
                     g.rack ? "yes" : "no", g.rackStart, g.rackFull, g.rackTravel, g.rackBack.x, g.rackBack.y,
                     g.rackBack.z, s.magazineNode, s.rackNode);
            return;
        }
        bool unsaved = false;
        for (const Slot& s : g_slots)
            if (s.state.load(std::memory_order_acquire) == Complete && !s.saved) unsaved = true;
        if (!unsaved) return;
        int written = 0;
        if (Save(written))
            snprintf(line, lineSize, "Reload animations: %d learned clip(s) saved", written);
        else
        {
            const DWORD error = GetLastError();
            // Not retried every tick: the next clip learned tries again.
            for (Slot& s : g_slots) s.saved = true;
            snprintf(line, lineSize, "Reload animations: could not write halomccvr_reload_clips.bin (error %lu)", error);
        }
    }
    catch (...)
    {
        snprintf(line, lineSize, "Reload animations: the file could not be handled (out of memory)");
    }
}

void ForgetAll() noexcept { g_forget.store(true, std::memory_order_release); }
uint32_t LearnedCount() noexcept { return g_learned.load(std::memory_order_relaxed); }
} // namespace reload_clip_store
