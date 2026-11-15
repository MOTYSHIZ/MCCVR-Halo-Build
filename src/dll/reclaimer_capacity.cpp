#include "reclaimer_capacity.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <string>
#include <vector>
#include <windows.h>

#include "../common/log.h"

namespace reclaimer_capacity
{
namespace
{
std::atomic<uint32_t> g_statedFpNodes{0};   // the client's fp_nodes, 0 until read

struct LogFile
{
    uint64_t written = 0;
    std::wstring path;
    std::string name;   // narrow, for the version and the log line
};

std::string Narrow(const std::wstring& text)
{
    std::string out;
    for (const wchar_t c : text)
        out.push_back(c < 0x80 ? static_cast<char>(c) : '?');
    return out;
}

// The client's logs, newest first.
std::vector<LogFile> ClientLogs(const std::wstring& folder)
{
    std::vector<LogFile> logs;
    WIN32_FIND_DATAW entry{};
    const HANDLE find = FindFirstFileW((folder + L"\\client*.log").c_str(), &entry);
    if (find == INVALID_HANDLE_VALUE)
        return logs;
    unsigned seen = 0;
    do
    {
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        LogFile log;
        log.written = (uint64_t(entry.ftLastWriteTime.dwHighDateTime) << 32) | entry.ftLastWriteTime.dwLowDateTime;
        log.path = folder + L"\\" + entry.cFileName;
        log.name = Narrow(entry.cFileName);
        logs.push_back(std::move(log));
    } while (++seen < 256 && FindNextFileW(find, &entry));
    FindClose(find);
    std::sort(logs.begin(), logs.end(), [](const LogFile& a, const LogFile& b) { return a.written > b.written; });
    return logs;
}

// The start of a log the client still has open for writing (its list is near the top).
std::string LoadHead(const std::wstring& path)
{
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return {};
    std::string text(512 * 1024, '\0');
    DWORD got = 0;
    if (!ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &got, nullptr))
        got = 0;
    CloseHandle(file);
    text.resize(got);
    return text;
}

std::string Hex(uint64_t value)
{
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

void Report(const LogFile& current, const Manifest& manifest, const LogFile* previous, const Manifest* older)
{
    const std::string version = VersionFromLogName(current.name);
    std::string stated;
    for (const Entry& entry : manifest.entries)
        if (ModDependsOn(entry.name) && IsSize(entry))
            stated += " " + entry.name + " " +
                (entry.summary ? std::to_string(entry.host) : Hex(entry.host) + " (retail " + Hex(entry.retail) + ")");
    LOG("RECLAIMER capacity: client %s lists %zu sizes in its log (%s); the ones this mod's reads depend on:%s",
        version.empty() ? "(version not in the log's name)" : version.c_str(), manifest.entries.size(),
        current.name.c_str(), stated.empty() ? " none listed" : stated.c_str());
    if (!previous || !older)
    {
        LOG("RECLAIMER capacity: no log of another client version to compare with");
        return;
    }
    const std::string olderVersion = VersionFromLogName(previous->name);
    const std::vector<Change> changes = Changes(*older, manifest);
    if (changes.empty())
    {
        LOG("RECLAIMER capacity: no size changed since client %s", olderVersion.c_str());
        return;
    }
    std::string depended, others;
    size_t otherCount = 0;
    for (const Change& change : changes)
    {
        const std::string item = " " + change.name + " " + (change.before ? Hex(change.was) : std::string("(new)")) +
            " -> " + (change.after ? Hex(change.now) : std::string("(gone)"));
        if (change.depended)
            depended += item;
        else if (++otherCount <= 24)
            others += item;
    }
    if (otherCount > 24)
        others += " and " + std::to_string(otherCount - 24) + " more";
    LOG("RECLAIMER capacity: since client %s, %zu size(s) changed. %s%s%s%s", olderVersion.c_str(), changes.size(),
        depended.empty() ? "None this mod's reads depend on" : "THIS MOD'S READS DEPEND ON:",
        depended.c_str(), others.empty() ? "" : "; others:", others.c_str());
}
} // namespace

void Poll()
{
    static bool done = false;
    static ULONGLONG nextMs = 0, firstMs = 0;
    if (done)
        return;
    const ULONGLONG now = GetTickCount64();
    if (now < nextMs)
        return;
    nextMs = now + 1000;
    if (!firstMs)
        firstMs = now;
    if (now - firstMs > 60000)
    {
        LOG("RECLAIMER capacity: no list of sizes in this launch's client log after a minute; nothing compared");
        done = true;
        return;
    }
    wchar_t buffer[1024]{};
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, 1024) || !buffer[0])
    {
        done = true;
        return;
    }
    const std::vector<LogFile> logs = ClientLogs(std::wstring(buffer) + L"\\ProjectReclaimer\\Logs");
    // This launch's log: the newest, written since this process started.
    FILETIME created{}, exited{}, kernel{}, user{};
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    const uint64_t started = (uint64_t(created.dwHighDateTime) << 32) | created.dwLowDateTime;
    if (logs.empty() || logs.front().written < started)
        return;
    const Manifest manifest = Parse(LoadHead(logs.front().path));
    if (manifest.Empty())
        return;
    done = true;
    if (const Entry* fp = manifest.Find("fp_nodes"))
        if (fp->host > 64 && fp->host <= 1024)
            g_statedFpNodes.store(static_cast<uint32_t>(fp->host), std::memory_order_release);

    // The newest log of another (named) version that holds a list.
    const std::string version = VersionFromLogName(logs.front().name);
    for (size_t i = 1; i < logs.size() && i < 64; ++i)
    {
        const std::string other = VersionFromLogName(logs[i].name);
        if (other.empty() || other == version)
            continue;
        const Manifest older = Parse(LoadHead(logs[i].path));
        if (older.Empty())
            continue;
        Report(logs.front(), manifest, &logs[i], &older);
        return;
    }
    Report(logs.front(), manifest, nullptr, nullptr);
}

NodeCandidates FpNodes()
{
    return FpNodeCandidates(uint64_t{g_statedFpNodes.load(std::memory_order_acquire)});
}
} // namespace reclaimer_capacity
