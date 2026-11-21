// ODST's tutorial text in VR words (src/common/odst_prompt_text.h): the English lines of Mombasa Streets' tutorials as
// the kit writes them (H3ODSTEK tags\ui\chapter_titles\atlas\h100.multilingual_unicode_string_list), rewritten with the
// default VR Mappings and with others; lines the rewrite must leave alone.
#include "../src/common/odst_prompt_text.h"

#include <cstdio>
#include <cwchar>

namespace
{
int g_failures = 0;
void Print(const wchar_t* text)
{
    for (const wchar_t* p = text; *p; ++p)
    {
        if (*p == L'\r') std::printf("\\r");
        else if (*p == L'\n') std::printf("\\n");
        else if (*p >= 0x20 && *p < 0x7F) std::printf("%c", static_cast<char>(*p));
        else std::printf("<U+%04X>", static_cast<unsigned>(*p));
    }
}
void Expect(const wchar_t* in, const odst_prompt_text::Controls& c, const wchar_t* want, int line)
{
    wchar_t out[odst_prompt_text::kMaxChars + 1]{};
    const unsigned n = odst_prompt_text::Rewrite(in, c, out);
    const bool ok = want ? n == std::wcslen(want) && std::wcscmp(out, want) == 0 : n == 0;
    std::printf("%s ", ok ? "ok  " : "FAIL");
    Print(in);
    std::printf("\n     -> ");
    if (n) Print(out); else std::printf("(the game's own text)");
    if (!ok)
    {
        std::printf("\n     wanted ");
        if (want) Print(want); else std::printf("(the game's own text)");
        std::printf("  [line %d]", line);
        ++g_failures;
    }
    std::printf("\n");
}
#define EXPECT(in, c, want) Expect(in, c, want, __LINE__)

odst_prompt_text::Controls Defaults()
{
    using namespace vr_mapping;
    odst_prompt_text::Controls c{};
    c.visr = Input{kNoneHeld, DpadUp};   // ODST's flashlight row by default (kDefaults)
    c.padX = Input{kNoneHeld, X};
    c.padY = Input{kNoneHeld, Y};
    return c;
}
} // namespace

int main()
{
    using namespace vr_mapping;
    const auto c = Defaults();
    // Mombasa Streets' look training.
    EXPECT(L"Use  \xE454  to look around the pod", c, L"Aim with your gun hand to look around the pod");
    EXPECT(L"Do you like these settings?\r\n   \xE451  Accept Settings    \xE452  Invert Camera", c,
           L"Do you like these settings?\r\nPress A to accept settings, press B to invert camera");
    EXPECT(L"Your aiming has been set to Normal\r\n \xE451  Continue", c,
           L"Your aiming has been set to Normal\r\nPress A to continue");
    EXPECT(L"Settings can be changed from the PAUSE Menu\r\n \xE451  Continue", c,
           L"Settings can be changed from the PAUSE Menu\r\nPress A to continue");
    // VISR training.
    EXPECT(L"Press \xE448  to activate your VISR", c,
           L"Hold the left controller by your head and push its stick up\r\nto activate your VISR");
    EXPECT(L"Would you like to repeat VISR training?\r\n   \xE503 Repeat    \xE452  Decline", c,
           L"Would you like to repeat VISR training?\r\nPress Y to repeat, press B to decline");
    // The VISR database (the map) training.
    EXPECT(L"Press  \xE44C  to activate VISR database", c,
           L"Hold the left controller by your head and click its stick to\r\nactivate VISR database");
    EXPECT(L"Use \xE50D  to move map", c, L"Push the left stick to move map");
    EXPECT(L"Use \xE50E  to zoom map", c, L"Push the right stick up or down to zoom map");
    EXPECT(L"Use  \xE501  or  \xE502  to switch between NAV beacons", c,
           L"Hold the left controller by your head and push its stick up\r\nor down to switch between NAV beacons");
    EXPECT(L"Press  \xE505  to place a waypoint\r\n \xE451  Continue", c,
           L"Press A to place a waypoint\r\nPress A to continue");
    EXPECT(L"Press  \xE506  to cycle waypoint selection", c, L"Press Y to cycle waypoint selection");
    EXPECT(L"Press  \xE507  to remove waypoint", c, L"Press X to remove waypoint");
    EXPECT(L"Press  \xE511  or  \xE512  to check INTEL data", c,
           L"Squeeze the left or right grip to check INTEL data");
    EXPECT(L"Use  \xE50F  or  \xE510  to select data block", c, L"Push the right stick left or right to select data block");
    EXPECT(L"Press  \xE509  to play COMM data and  \xE50C  to stop", c, L"Press A to play COMM data and press X to stop");
    EXPECT(L"Press  \xE44C  or  \xE452  to deactivate VISR database", c,
           L"Hold the left controller by your head and click its stick or\r\npress B to deactivate VISR database");
    EXPECT(L"Would you like to perform TRAINING?\r\n   \xE503  Accept    \xE452  Decline", c,
           L"Would you like to perform TRAINING?\r\nPress Y to accept, press B to decline");
    EXPECT(L"Press  \xE501  or  \xE502  for assistance", c,
           L"Hold the left controller by your head and push its stick up\r\nor down for assistance");

    // Left alone: other languages, no picture, a picture this file does not know.
    EXPECT(L"Sehen Sie sich mit \xE454 um", c, nullptr);
    EXPECT(L"La resistencia se regenera con el tiempo.\r\n\xE451 Continuar", c, nullptr);
    EXPECT(L"Look settings assigned", c, nullptr);
    EXPECT(L"Press \xE443 to jump", c, nullptr);

    // The player's own rows: confirm on the right trigger, the pages on a chord, zoom on the grips.
    {
        auto mine = c;
        mine.odst[PromptConfirm] = {Input{kNoneHeld, PrimaryTrigger}, Input{}, Input{}, Input{}};
        mine.odst[MapPreviousPage] = {Input{{SupportGrip, Unbound}, X}, Input{}, Input{}, Input{}};
        mine.odst[MapNextPage] = {Input{{SupportGrip, Unbound}, Y}, Input{}, Input{}, Input{}};
        mine.odst[MapZoomIn] = {Input{kNoneHeld, PrimaryGrip}, Input{}, Input{}, Input{}};
        mine.odst[MapZoomOut] = {Input{kNoneHeld, SupportGrip}, Input{}, Input{}, Input{}};
        mine.visr = Input{kNoneHeld, RightStickUp};
        EXPECT(L"New NAV data!\r\n \xE451  Continue", mine, L"New NAV data!\r\nPull the right trigger to continue");
        EXPECT(L"Press  \xE511  or  \xE512  to check INTEL data", mine,
               L"Hold the left grip and press X or hold the left grip and\r\npress Y to check INTEL data");
        EXPECT(L"Use \xE50E  to zoom map", mine, L"Squeeze the right or left grip to zoom map");
        EXPECT(L"Press \xE448  to activate your VISR", mine, L"Push the right stick up to activate your VISR");
        // Left-handed: the gun hand's trigger is the left one.
        mine.leftHanded = true;
        EXPECT(L"New NAV data!\r\n \xE451  Continue", mine, L"New NAV data!\r\nPull the left trigger to continue");
        // An unbound row keeps the game's own line.
        mine.odst[PromptReject] = {Input{}, Input{}, Input{}, Input{}};
        EXPECT(L"Would you like to repeat HEALTH training?\r\n   \xE503  Repeat    \xE452  Decline", mine, nullptr);
    }
    // The D-pad gesture on the right hand.
    {
        auto right = c;
        right.dpadRightHand = true;
        EXPECT(L"Press  \xE44C  to open VISR database", right,
               L"Hold the right controller by your head and click its stick\r\nto open VISR database");
    }
    // Unpublished controls (Controls{} defaults: nothing read yet) keep the game's own text where they are needed.
    EXPECT(L"Press \xE448  to activate your VISR", odst_prompt_text::Controls{}, nullptr);

    // The wrap never leaves a line over the width, and the length stays within the chud's copy.
    {
        wchar_t out[odst_prompt_text::kMaxChars + 1]{};
        const unsigned n = odst_prompt_text::Rewrite(
            L"Use  \xE501  or  \xE502  to switch between NAV beacons and to choose the beacon you want to follow", c, out);
        unsigned run = 0, longest = 0;
        for (unsigned i = 0; i < n; ++i) { run = out[i] == L'\n' ? 0 : run + 1; longest = run > longest ? run : longest; }
        const bool ok = n && longest <= odst_prompt_text::kWrapWidth + 2;
        std::printf("%s wrap: longest line %u characters\n", ok ? "ok  " : "FAIL", longest);
        g_failures += ok ? 0 : 1;
    }
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}
