// A PLAYER build reads every key a player build writes.
//
// The settings file is written from the whole of the settings, and read by a chain of key tests of which a part
// is compiled into development builds only. A key that is written always and read inside that part is saved,
// then called unknown at the next start: the setting works for the session it is changed in and is forgotten.
// This target is compiled as a player build (no HALOMCCVR_DEV_DIAGNOSTICS), so that part is not in it.
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../src/common/config.h"

namespace
{
std::vector<std::string> g_unknownKeys;
int failures = 0;
void Check(bool ok, const char* what)
{
    if (!ok)
    {
        ++failures;
        std::printf("FAIL: %s\n", what);
    }
}
} // namespace

void Logf(const char* format, ...)
{
    char line[512];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    if (std::strstr(line, "unknown key"))
        g_unknownKeys.emplace_back(line);
}

int main()
{
#if HALOMCCVR_DEV_DIAGNOSTICS
    std::printf("FAIL: this target must be compiled as a player build\n");
    return 1;
#else
    wchar_t folder[MAX_PATH]{}, path[MAX_PATH]{};
    GetTempPathW(MAX_PATH, folder);
    swprintf_s(path, L"%shalomccvr-player-roundtrip-%lu.cfg", folder, GetCurrentProcessId());
    DeleteFileW(path);

    // The first load of a file that is not there leaves the built-in settings; the save writes all of them.
    g_config = Config{};
    ConfigLoad(path);
    g_config.mono_rendering = true;
    g_config.hook_cost_frames = 503;
    g_config.host_ui_panel = false;
    g_config.host_ui_panel_width_m = 3.3f;
    g_config.host_ui_panel_distance_m = 1.7f;
    g_config.aim_frame_zoom = true;
    g_config.aim_frame_size_deg[1] = 70.0f;
    g_config.aim_frame_strength[0] = 1.5f;
    g_config.aim_frame_move_s = 0.3f;
    ConfigSave();

    g_unknownKeys.clear();
    g_config = Config{};
    ConfigLoad(path);
    DeleteFileW(path);

    for (const std::string& line : g_unknownKeys)
        std::printf("FAIL: written by a player build and not read by one: %s\n", line.c_str());
    failures += static_cast<int>(g_unknownKeys.size());
    Check(g_config.mono_rendering, "mono_rendering is remembered");
    Check(g_config.hook_cost_frames == 503, "hook_cost_frames is remembered");
    Check(!g_config.host_ui_panel, "host_ui_panel is remembered");
    Check(g_config.host_ui_panel_width_m > 3.29f && g_config.host_ui_panel_width_m < 3.31f,
          "host_ui_panel_width_m is remembered");
    Check(g_config.host_ui_panel_distance_m > 1.69f && g_config.host_ui_panel_distance_m < 1.71f,
          "host_ui_panel_distance_m is remembered");
    Check(g_config.aim_frame_zoom, "aim_frame_zoom is remembered");
    Check(g_config.aim_frame_size_deg[1] > 69.9f && g_config.aim_frame_size_deg[1] < 70.1f,
          "aim_frame_size_deg_2 is remembered");
    Check(g_config.aim_frame_strength[0] > 1.49f && g_config.aim_frame_strength[0] < 1.51f,
          "aim_frame_strength_1 is remembered");
    Check(g_config.aim_frame_move_s > 0.29f && g_config.aim_frame_move_s < 0.31f,
          "aim_frame_move_s is remembered");

    if (failures)
        std::printf("%d check(s) failed\n", failures);
    else
        std::printf("config player round trip: all checks passed\n");
    return failures ? 1 : 0;
#endif
}
