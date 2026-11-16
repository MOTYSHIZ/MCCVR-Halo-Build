#pragma once
// halomccvr_reloads.txt: the manual reload's per-gun settings (Halo 3 / ODST), one line per first-person gun, named
// as the official kits name its model. Campaign Evolved's per-weapon file (vr-game-conversion
// references/manual-reload.md section 3) in this mod's simple line format:
//
//   fp_assault_rifle enabled=1 grab=10 handle=0,0,0 depth=8 travel=6
//
//   enabled  1/0: the gun reloads by hand (0: the game's way). A gun with no line uses its built-in setting.
//   grab     cm: how close the support hand must be to the rack part to take it.
//   handle   cm back,up,side: where the rack part is taken, from the part (set in the headset: "Set the handle
//            grab spot to my hand").
//   depth    cm: how far below the well a magazine is taken onto its way in (when no reload animation is learned).
//   travel   cm: the rack's stroke (when no reload animation is learned).
//
// A field left out keeps the built-in value. Unknown names and fields are kept as they are in the file but ignored.
// Pure: no allocation.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace reload_weapon_file
{
inline constexpr int kMaxEntries = 32;
inline constexpr int kNameChars = 40;

struct Entry
{
    char name[kNameChars]{};
    int enabled = -1;               // -1: not set
    float grab = NAN, depth = NAN, travel = NAN;   // cm; NaN: not set
    bool handleSet = false;
    float handle[3]{};              // cm back, up, side
};

struct Table
{
    int count = 0;
    Entry entries[kMaxEntries]{};
    int errors = 0, firstErrorLine = 0;
};

inline const Entry* Find(const Table& t, const char* name) noexcept
{
    if (!name) return nullptr;
    for (int i = 0; i < t.count; ++i)
        if (std::strcmp(t.entries[i].name, name) == 0) return &t.entries[i];
    return nullptr;
}
inline Entry* FindOrAdd(Table& t, const char* name) noexcept
{
    if (!name || !*name || std::strlen(name) >= size_t(kNameChars)) return nullptr;
    for (int i = 0; i < t.count; ++i)
        if (std::strcmp(t.entries[i].name, name) == 0) return &t.entries[i];
    if (t.count >= kMaxEntries) return nullptr;
    Entry& e = t.entries[t.count++];
    e = Entry{};
    std::memcpy(e.name, name, std::strlen(name) + 1);
    return &e;
}

inline bool Number(const char* text, float lo, float hi, float& out) noexcept
{
    char* end = nullptr;
    const float v = std::strtof(text, &end);
    if (end == text || !std::isfinite(v) || v < lo || v > hi) return false;
    out = v;
    return true;
}

// Parses a whole file. Lines starting with '#' and blank lines are skipped.
inline Table Parse(const char* text) noexcept
{
    Table t{};
    if (!text) return t;
    int lineNo = 0;
    const char* p = text;
    while (*p)
    {
        ++lineNo;
        char line[256]{};
        size_t n = 0;
        while (*p && *p != '\n') { if (n + 1 < sizeof(line) && *p != '\r') line[n++] = *p; ++p; }
        if (*p == '\n') ++p;
        char* s = line;
        while (*s == ' ' || *s == '\t') ++s;
        if (!*s || *s == '#') continue;
        // Split on blanks in place.
        char* tokens[16]{};
        int count = 0;
        for (char* c = s; *c && count < 16;)
        {
            while (*c == ' ' || *c == '\t') *c++ = 0;
            if (!*c) break;
            tokens[count++] = c;
            while (*c && *c != ' ' && *c != '\t') ++c;
        }
        Entry* e = FindOrAdd(t, tokens[0]);
        bool bad = e == nullptr;
        for (int k = 1; e && k < count; ++k)
        {
            char* tok = tokens[k];
            char* eq = std::strchr(tok, '=');
            if (!eq) { bad = true; continue; }
            *eq = 0;
            const char* key = tok;
            const char* val = eq + 1;
            float v = 0;
            if (!std::strcmp(key, "enabled") && (val[0] == '0' || val[0] == '1') && !val[1]) e->enabled = val[0] - '0';
            else if (!std::strcmp(key, "grab") && Number(val, 2, 40, v)) e->grab = v;
            else if (!std::strcmp(key, "depth") && Number(val, 1, 40, v)) e->depth = v;
            else if (!std::strcmp(key, "travel") && Number(val, 1, 20, v)) e->travel = v;
            else if (!std::strcmp(key, "handle"))
            {
                float h[3]{};
                char buf[64]{};
                for (size_t i = 0; i + 1 < sizeof(buf) && val[i]; ++i) buf[i] = val[i];
                char* a = buf;
                bool ok = true;
                for (int i = 0; i < 3 && ok; ++i)
                {
                    char* comma = std::strchr(a, ',');
                    if (comma) *comma = 0;
                    ok = Number(a, -30, 30, h[i]) && (i == 2 ? !comma : comma != nullptr);
                    a = comma ? comma + 1 : a;
                }
                if (ok) { e->handleSet = true; for (int i = 0; i < 3; ++i) e->handle[i] = h[i]; }
                else bad = true;
            }
            else bad = true;
        }
        if (bad) { if (!t.errors) t.firstErrorLine = lineNo; ++t.errors; }
    }
    return t;
}

// The whole file, written back (the header, then one line per entry with the fields it sets).
inline size_t Format(const Table& t, char* out, size_t size) noexcept
{
    if (!out || !size) return 0;
    size_t at = 0;
    auto put = [&](const char* fmt, auto... args) {
        if (at >= size) return;
        const int n = std::snprintf(out + at, size - at, fmt, args...);
        if (n > 0) at += size_t(n) < size - at ? size_t(n) : size - at - 1;
    };
    put("# Manual reload, Halo 3 / ODST: one line per first-person gun (the kit's model name). Fields left out keep\n");
    put("# the built-in value. enabled=1/0 (by hand or the game's way), grab=cm (reach to take the handle),\n");
    put("# handle=back,up,side cm (where the handle is taken, set in the headset), depth=cm, travel=cm.\n");
    put("# Delete this file to go back to the built-in settings.\n");
    for (int i = 0; i < t.count; ++i)
    {
        const Entry& e = t.entries[i];
        put("%s", e.name);
        if (e.enabled >= 0) put(" enabled=%d", e.enabled);
        if (std::isfinite(e.grab)) put(" grab=%.1f", double(e.grab));
        if (e.handleSet) put(" handle=%.1f,%.1f,%.1f", double(e.handle[0]), double(e.handle[1]), double(e.handle[2]));
        if (std::isfinite(e.depth)) put(" depth=%.1f", double(e.depth));
        if (std::isfinite(e.travel)) put(" travel=%.1f", double(e.travel));
        put("\n");
    }
    return at;
}
} // namespace reload_weapon_file
