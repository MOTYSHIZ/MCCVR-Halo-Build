#include "reload_weapon_file.h"
#include <cstdio>

namespace
{
unsigned checks = 0, failures = 0;
void Check(bool value, const char* name)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL: %s\n", name); }
}
bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }
} // namespace

int main()
{
    using namespace reload_weapon_file;
    const char* text =
        "# comment\r\n"
        "\r\n"
        "fp_assault_rifle enabled=1 grab=12.5 handle=1,-2,0.5 depth=9 travel=7\r\n"
        "fp_smg\tenabled=0\n"
        "fp_magnum grab=999 bogus=3\n"
        "fp_sniper_rifle handle=1,2\n";
    const Table t = Parse(text);
    Check(t.count == 4, "four guns");
    const Entry* ar = Find(t, "fp_assault_rifle");
    Check(ar && ar->enabled == 1 && Near(ar->grab, 12.5f) && ar->handleSet && Near(ar->handle[1], -2.f) &&
              Near(ar->handle[2], 0.5f) && Near(ar->depth, 9.f) && Near(ar->travel, 7.f),
          "every field of a full line");
    const Entry* smg = Find(t, "fp_smg");
    Check(smg && smg->enabled == 0 && std::isnan(smg->grab) && !smg->handleSet, "a tab-separated line, one field");
    const Entry* magnum = Find(t, "fp_magnum");
    Check(magnum && std::isnan(magnum->grab), "a value out of range is not taken");
    const Entry* sniper = Find(t, "fp_sniper_rifle");
    Check(sniper && !sniper->handleSet, "a handle with two numbers is not taken");
    Check(t.errors == 2 && t.firstErrorLine == 5, "the bad lines are counted, the first one named");

    char out[2048];
    Format(t, out, sizeof(out));
    const Table again = Parse(out);
    const Entry* ar2 = Find(again, "fp_assault_rifle");
    Check(again.count == 4 && again.errors == 0 && ar2 && Near(ar2->grab, 12.5f) && Near(ar2->handle[0], 1.f),
          "written back, it reads the same");

    Table added{};
    Entry* e = FindOrAdd(added, "fp_shotgun");
    Check(e && added.count == 1 && FindOrAdd(added, "fp_shotgun") == e, "one entry per name");
    Check(!FindOrAdd(added, "a_name_far_too_long_to_be_any_kit_model_name_at_all"), "a name too long is refused");
    std::printf("Reload weapon file: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
