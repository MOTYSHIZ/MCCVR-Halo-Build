#pragma once

// Simple thread-safe file logger. Everything the mod does gets written to
// halo3xr.log next to the DLL so users can send us the file when something
// goes wrong on a machine we can't see.

// `keepRuns` > 0: the log a run before left is first copied into a Logs folder beside it, named for when that run
// started, and only the newest `keepRuns` such copies are kept (log_archive_logic.h).
void LogInit(const wchar_t* filePath, unsigned keepRuns = 0);
// The same for another evidence file a run writes (HaloMCCVR-native-faults.log): the copy its previous run left goes
// into the Logs folder beside it, named for when that run started (its first line's time, else its last write), and
// the newest `keepRuns` copies of that name are kept. Call before the file is replaced. Best effort.
void LogArchivePrevious(const wchar_t* filePath, unsigned keepRuns);
// Directory of the log file (trailing separator included), for evidence
// files written next to it. Empty until LogInit ran.
const wchar_t* LogDirectory();
void Logf(const char* fmt, ...);

#define LOG(...) Logf(__VA_ARGS__)
