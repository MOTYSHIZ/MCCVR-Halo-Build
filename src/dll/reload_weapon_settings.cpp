#include "reload_weapon_settings.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace reload_weapon_settings
{
namespace
{
constexpr wchar_t kFileName[] = L"halomccvr_reloads.txt";

struct Change
{
    enum Kind : int { Enabled, Handle, ClearHandle } kind = Enabled;
    char name[reload_weapon_file::kNameChars]{};
    bool enabled = false;
    float handle[3]{};
};

std::wstring g_path;
SRWLOCK g_lock = SRWLOCK_INIT;            // the published table
reload_weapon_file::Table g_table{};
SRWLOCK g_changesLock = SRWLOCK_INIT;     // queued changes
Change g_changes[16]{};
int g_changeCount = 0;
FILETIME g_seen{};
bool g_seenValid = false;

bool Copy(char (&out)[reload_weapon_file::kNameChars], const char* name) noexcept
{
    if (!name) return false;
    const size_t n = std::strlen(name);
    if (!n || n >= sizeof(out)) return false;
    std::memcpy(out, name, n + 1);
    return true;
}

void Queue(const Change& c) noexcept
{
    AcquireSRWLockExclusive(&g_changesLock);
    if (g_changeCount < int(std::size(g_changes))) g_changes[g_changeCount++] = c;
    ReleaseSRWLockExclusive(&g_changesLock);
}

bool FileTime(FILETIME& out)
{
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(g_path.c_str(), GetFileExInfoStandard, &data)) return false;
    out = data.ftLastWriteTime;
    return true;
}

bool ReadFileText(std::string& text)
{
    const HANDLE f = CreateFileW(g_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(f, &size) && size.QuadPart >= 0 && size.QuadPart < (1 << 20);
    if (ok)
    {
        text.assign(size_t(size.QuadPart), '\0');
        DWORD got = 0;
        ok = size.QuadPart == 0 || (ReadFile(f, text.data(), DWORD(text.size()), &got, nullptr) && got == text.size());
    }
    CloseHandle(f);
    return ok;
}

bool WriteFileText(const char* text, size_t size)
{
    const std::wstring temp = g_path + L".tmp";
    const HANDLE f = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0;
    const bool ok = WriteFile(f, text, DWORD(size), &wrote, nullptr) && wrote == size;
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

void Poll(char* line, size_t lineSize) noexcept
{
    if (line && lineSize) line[0] = 0;
    if (g_path.empty() || !line || !lineSize) return;
    try
    {
        // The file changed (or appeared, or went): read it again.
        FILETIME now{};
        const bool exists = FileTime(now);
        if (exists != g_seenValid || (exists && CompareFileTime(&now, &g_seen) != 0))
        {
            reload_weapon_file::Table table{};
            if (exists)
            {
                std::string text;
                if (ReadFileText(text)) table = reload_weapon_file::Parse(text.c_str());
            }
            AcquireSRWLockExclusive(&g_lock);
            g_table = table;
            ReleaseSRWLockExclusive(&g_lock);
            g_seen = now;
            g_seenValid = exists;
            if (exists)
                snprintf(line, lineSize, "Manual reload: halomccvr_reloads.txt read: %d gun line(s)%s", table.count,
                         table.errors ? " (some fields were not understood: see the file's header)" : "");
            return;
        }
        // Changes from the menu and the calibrations.
        Change changes[16]{};
        int count = 0;
        AcquireSRWLockExclusive(&g_changesLock);
        count = g_changeCount;
        std::memcpy(changes, g_changes, sizeof(Change) * size_t(count));
        g_changeCount = 0;
        ReleaseSRWLockExclusive(&g_changesLock);
        if (!count) return;
        reload_weapon_file::Table table{};
        AcquireSRWLockShared(&g_lock);
        table = g_table;
        ReleaseSRWLockShared(&g_lock);
        for (int i = 0; i < count; ++i)
        {
            auto* e = reload_weapon_file::FindOrAdd(table, changes[i].name);
            if (!e) continue;
            if (changes[i].kind == Change::Enabled) e->enabled = changes[i].enabled ? 1 : 0;
            else if (changes[i].kind == Change::Handle)
            {
                e->handleSet = true;
                for (int a = 0; a < 3; ++a) e->handle[a] = changes[i].handle[a];
            }
            else e->handleSet = false;
        }
        std::vector<char> text(16384);
        const size_t size = reload_weapon_file::Format(table, text.data(), text.size());
        AcquireSRWLockExclusive(&g_lock);
        g_table = table;
        ReleaseSRWLockExclusive(&g_lock);
        if (WriteFileText(text.data(), size))
        {
            (void)FileTime(g_seen);
            g_seenValid = true;
            snprintf(line, lineSize, "Manual reload: halomccvr_reloads.txt saved (%d change(s), %d gun line(s))", count,
                     table.count);
        }
        else
            snprintf(line, lineSize, "Manual reload: could not write halomccvr_reloads.txt (error %lu); the change "
                     "holds until the game closes", GetLastError());
    }
    catch (...)
    {
        snprintf(line, lineSize, "Manual reload: halomccvr_reloads.txt could not be handled (out of memory)");
    }
}

bool Get(const char* name, reload_weapon_file::Entry& out) noexcept
{
    if (!name) return false;
    AcquireSRWLockShared(&g_lock);
    const auto* e = reload_weapon_file::Find(g_table, name);
    if (e) out = *e;
    ReleaseSRWLockShared(&g_lock);
    return e != nullptr;
}

void SetEnabled(const char* name, bool enabled) noexcept
{
    Change c{};
    c.kind = Change::Enabled;
    c.enabled = enabled;
    if (Copy(c.name, name)) Queue(c);
}
void SetHandle(const char* name, const float cm[3]) noexcept
{
    Change c{};
    c.kind = Change::Handle;
    if (!cm || !Copy(c.name, name)) return;
    for (int a = 0; a < 3; ++a) c.handle[a] = cm[a];
    Queue(c);
}
void ClearHandle(const char* name) noexcept
{
    Change c{};
    c.kind = Change::ClearHandle;
    if (Copy(c.name, name)) Queue(c);
}
} // namespace reload_weapon_settings
