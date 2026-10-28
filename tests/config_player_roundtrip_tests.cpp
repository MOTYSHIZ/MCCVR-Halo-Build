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
#include "../src/common/weapon_alignment.h"

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
    for (int profile=0;profile<kTitleProfileCount;++profile) {
        const auto baseline=weapon_alignment::Baseline(profile);
        const auto& saved=g_config.title_profiles[profile];
        Check(saved.gun_forward_m==baseline.forward && saved.gun_right_m==baseline.right &&
            saved.gun_up_m==baseline.up,"new config uses per-title gun baselines");
        const auto left=weapon_alignment::kLeftOffsets[profile];
        Check(saved.left_gun_forward_m==left.forward && saved.left_gun_right_m==left.right &&
            saved.left_gun_up_m==left.up,"new config uses independent left baselines");
    }
    g_config.left_gun_forward_m=.11f;g_config.left_gun_right_m=-.07f;g_config.left_gun_up_m=.03f;
    g_config.gun_forward_m=-.22f;g_config.gun_right_m=.08f;g_config.gun_up_m=-.09f;
    g_config.mono_rendering = true;
    ConfigSave();

    g_unknownKeys.clear();
    g_config = Config{};
    ConfigLoad(path);
    DeleteFileW(path);

    for (const std::string& line : g_unknownKeys)
        std::printf("FAIL: written by a player build and not read by one: %s\n", line.c_str());
    failures += static_cast<int>(g_unknownKeys.size());
    Check(g_config.mono_rendering, "mono_rendering is remembered");
    Check(g_config.left_gun_forward_m==.11f && g_config.left_gun_right_m==-.07f && g_config.left_gun_up_m==.03f,
        "left offsets survive save/reload");
    Check(g_config.gun_forward_m==-.22f && g_config.gun_right_m==.08f && g_config.gun_up_m==-.09f,
        "right offsets remain independent after save/reload");

    // Upgrade once, including explicit title overrides; later edits survive reload.
    FILE* legacy=nullptr;
    _wfopen_s(&legacy,path,L"wt");
    Check(legacy!=nullptr,"legacy fixture opens");
    if (legacy) {
        std::fputs("left_hand_forward_m = 0.12\nhalo3_left_hand_forward_m = 0.2\n",legacy);
        std::fclose(legacy);
        ConfigLoad(path);
        Check(g_config.left_hand_forward_m==0,"legacy shared offset resets");
        for (const auto& profile:g_config.title_profiles)
            Check(profile.left_hand_forward_m==0,"legacy title offset resets");
        g_config.left_hand_forward_m=0.075f;
        ConfigSave();
        ConfigLoad(path);
        Check(g_config.left_hand_forward_m==0.075f,"post-upgrade edit survives reload");
        DeleteFileW(path);
    }

    // Retired shared and per-title offsets are cleared and remain inert on reload.
    FILE* retired=nullptr;
    _wfopen_s(&retired,path,L"wt");
    Check(retired!=nullptr,"retired hand fixture opens");
    if (retired) {
        std::fputs("left_hand_forward_reset_version = 1\nleft_hand_mesh_x_m = .1\n"
            "right_hand_mesh_z_m = -.1\nhalo4_left_hand_mesh_y_m = .15\n"
            "halo4_right_hand_mesh_x_m = -.12\nhalo4_left_gun_right_m = .07\n",retired);
        std::fclose(retired);
        for (int pass=0;pass<2;++pass) {
            ConfigLoad(path);
            Config_ApplyTitleProfile(3);
            Check(g_config.left_hand_mesh_x_m==0 && g_config.left_hand_mesh_y_m==0 &&
                g_config.left_hand_mesh_z_m==0 && g_config.right_hand_mesh_x_m==0 &&
                g_config.right_hand_mesh_y_m==0 && g_config.right_hand_mesh_z_m==0,
                "retired visual hand offsets stay zero after title switch and reload");
            Check(g_config.left_gun_right_m==.07f,"independent gun offset survives retired-hand cleanup");
            for (const auto& profile:g_config.title_profiles)
                Check(profile.left_hand_mesh_x_m==0 && profile.left_hand_mesh_y_m==0 &&
                    profile.left_hand_mesh_z_m==0 && profile.right_hand_mesh_x_m==0 &&
                    profile.right_hand_mesh_y_m==0 && profile.right_hand_mesh_z_m==0,
                    "all title profiles discard retired hand offsets");
            Config_ApplyTitleProfile(-1);
        }
        DeleteFileW(path);
    }

    if (failures)
        std::printf("%d check(s) failed\n", failures);
    else
        std::printf("config player round trip: all checks passed\n");
    return failures ? 1 : 0;
#endif
}
