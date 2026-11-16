// HaloMCCVR.log keeps the previous launch as .prev even when another process (MCC's crash reporter, which
// once inherited the log handle) still holds the old log open; and the new log is opened so that it can be
// renamed while it is written and is not inherited by child processes.
#include <windows.h>
#include <cstdio>
#include <string>
#include "../src/common/log.h"

static int failures = 0;
static void Check(bool ok, const char* what)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++failures;
}

static std::string ReadAll(const std::wstring& path)
{
    std::string text;
    if (FILE* f = _wfopen(path.c_str(), L"rb"))
    {
        char buffer[512];
        size_t n = 0;
        while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0)
            text.append(buffer, n);
        fclose(f);
    }
    return text;
}

int main()
{
    wchar_t temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp);
    const std::wstring dir = std::wstring(temp) + L"halomccvr_log_rotation_test\\";
    CreateDirectoryW(dir.c_str(), nullptr);
    const std::wstring log = dir + L"test.log", prev = log + L".prev", moved = dir + L"moved.log";
    DeleteFileW(log.c_str());
    DeleteFileW(prev.c_str());
    DeleteFileW(moved.c_str());

    // The previous run's log, still held by another handle that does not share delete (as the crash
    // reporter's inherited handle did), so the rename to .prev must fail.
    HANDLE old = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(old != INVALID_HANDLE_VALUE, "fixture: the previous log is created and held without delete sharing");
    DWORD written = 0;
    WriteFile(old, "previous run\r\n", 14, &written, nullptr);

    LogInit(log.c_str());
    Logf("this run %d", 7);
    Check(ReadAll(prev) == "previous run\r\n", "a held previous log is still kept as .prev (copied when it cannot be renamed)");
    Check(ReadAll(log).find("] this run 7\r\n") != std::string::npos, "the new log is written fresh");
    CloseHandle(old);

    // The new log shares delete: it can be renamed while this process writes it.
    Check(MoveFileExW(log.c_str(), moved.c_str(), 0) != 0, "the open log can be renamed (delete sharing)");
    Logf("after the rename");
    Check(ReadAll(moved).find("after the rename") != std::string::npos, "writing continues into the renamed file");

    // Another evidence file (the native fault log) is archived into Logs\ under its own name, named for the
    // time on its first line, and only the newest `keep` of that name stay; other names are left alone.
    {
        const std::wstring logs = dir + L"Logs\\", faults = dir + L"faults.log";
        WIN32_FIND_DATAW found{};
        HANDLE search = FindFirstFileW((logs + L"*").c_str(), &found);
        if (search != INVALID_HANDLE_VALUE)
        {
            do
                if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    DeleteFileW((logs + found.cFileName).c_str());
            while (FindNextFileW(search, &found));
            FindClose(search);
        }
        auto write = [&](const char* text) {
            if (FILE* f = _wfopen(faults.c_str(), L"wb"))
            {
                std::fputs(text, f);
                std::fclose(f);
            }
        };
        auto count = [&](const wchar_t* pattern) {
            int n = 0;
            WIN32_FIND_DATAW entry{};
            const HANDLE h = FindFirstFileW((logs + pattern).c_str(), &entry);
            if (h == INVALID_HANDLE_VALUE)
                return 0;
            do
                ++n;
            while (FindNextFileW(h, &entry));
            FindClose(h);
            return n;
        };
        const char* runs[] = {"[00:00:01.000] run a\r\n", "[00:00:02.000] run b\r\n", "[00:00:03.000] run c\r\n"};
        for (const char* run : runs)
        {
            write(run);
            LogArchivePrevious(faults.c_str(), 2);
        }
        Check(count(L"faults *.log") == 2, "only the newest two fault logs are kept");
        Check(count(L"faults * 00-00-03.log") == 1 && count(L"faults * 00-00-01.log") == 0,
              "each copy is named for the time on its first line, and the oldest went");
        Check(count(L"test *.log") == 0 && count(L"moved *.log") == 0, "no other name is archived or pruned");
        DeleteFileW(faults.c_str());
    }

    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
