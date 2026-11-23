#pragma once
// ODST'S TUTORIAL TEXT IN VR WORDS (docs/ODST-PROMPT-TEXT-EVIDENCE-2026-11-20.md).
//
// ODST's tutorial lines ("Press [VISR] to activate your VISR", "[A] Continue") come from the scenario's cutscene titles
// (chud_show_cinematic_title). Their button pictures are private-use characters (U+E451 accept, U+E452 cancel, U+E448
// the flashlight action, U+E50E the map's zoom, ...; the editing kit's glyph table names each). The game turns each into
// a gamepad picture, or MCC into a keyboard key while it thinks the player uses a keyboard. In VR neither is the
// control the player holds.
//
// So the text is rewritten before the chud keeps it: each English line that names a control is said again in the
// player's own VR controls, read from VR Mappings ("Squeeze the left grip or right grip to check INTEL data"). The
// chud's copy is the only thing that changes: presentation only, nothing in the game's state is written.
//
// WHERE (MCC, halo3odst.dll). chud_show_cinematic_title's implementation looks the title's string up into a 256
// character buffer on its stack and hands that buffer to the chud's text setter (user, text, seconds), which copies at
// most 255 characters. Two signatures, each unique in the module's code, must agree: the implementation's call site
// (its call of the setter, read from the rel32) and the setter's own body. Nothing is a recorded address.
//
// WHAT. Only English lines of the forms the tutorials use are rewritten: "Press|Use <control> [or <control>] to|for ...",
// and a choice line "<control> Continue" / "<control> Repeat <control> Decline". Any other line that holds a button
// picture, a picture this file does not know, or a control the player has left unbound keeps the game's own text whole.
//
// Engine-free and allocation-free: the DLL runs it on the game's script thread, the tests on fixtures.
#include "drive_resolve.h"
#include "vr_action_mapping.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

namespace odst_prompt_text
{
// ---------------------------------------------------------------------------------------------------------- where
// The implementation's call of the setter: mov ecx,[rsi+STRINGS]; movsx rax,di; lea rdi,[rax+rax*4]; cmp ecx,-1; je;
// mov edx,[rbx+rdi*8]; lea r8,[rsp+20h]; mov word [rsp+20h],r10w; call LOOKUP; movss xmm2,[rbx+rdi*8+SECONDS];
// lea rdx,[rsp+20h]; mov ecx,ebp; call SETTER.
inline constexpr char kTitleCall[] =
    "8B 8E ?? ?? ?? ?? 48 0F BF C7 48 8D 3C 80 83 F9 FF 74 ?? 8B 14 FB 4C 8D 44 24 20 66 44 89 54 24 20 E8 ?? ?? ?? ?? "
    "F3 0F 10 54 FB ?? 48 8D 54 24 20 8B CD E8";
inline constexpr uint32_t kTitleCallSetter = 0x34, kTitleCallNext = 0x38;
// The setter: the user's chud block (0x3B88 bytes each), the expiry time from `seconds`, then a copy of at most 0xFF
// characters (mov r8d,0xFF; call copy) and a terminator.
inline constexpr char kSetter[] =
    "40 53 48 83 EC 20 44 8B 05 ?? ?? ?? ?? 33 DB 65 48 8B 04 25 58 00 00 00 41 B9 ?? ?? ?? ?? 4E 8B 14 C0 48 63 C1 "
    "4C 69 C0 ?? ?? ?? ?? 4F 8B 0C 11 48 85 D2 74 ?? B9 ?? ?? ?? ?? 4A 8B 0C 11 0F BF 41 04 66 0F 6E C0 0F 5B C0 F3 0F "
    "59 C2 F3 0F 58 05 ?? ?? ?? ?? F3 0F 2C C0 03 41 0C 49 8D 89 ?? ?? ?? ?? 49 03 C8 43 89 84 01 ?? ?? ?? ?? 41 B8 FF "
    "00 00 00 E8";
// The setter keeps at most this many characters (its copy's limit, in kSetter); the rewrite stays within it.
inline constexpr unsigned kMaxChars = 0xFF;

struct Where
{
    bool ok = false;
    uint32_t setterRva = 0;      // the chud's text setter (user, text, seconds)
    uint32_t titleCallRva = 0;   // chud_show_cinematic_title's call of it
    char missing[192]{};         // what was not found or did not agree, "; " apart (empty when ok)
};

namespace detail
{
inline void Note(Where& w, const char* what)
{
    const size_t used = std::strlen(w.missing);
    std::snprintf(w.missing + used, sizeof(w.missing) - used, "%s%s", used ? "; " : "", what);
}
inline uint32_t Unique(const drive_resolve::Image& im, const char* text, const char* name, Where& w)
{
    drive_resolve::Pattern p{};
    if (!drive_resolve::Compile(text, p))
    {
        char line[96];
        std::snprintf(line, sizeof(line), "%s (bad signature)", name);
        Note(w, line);
        return 0;
    }
    const auto hits = drive_resolve::CountExecutable(im, p);
    if (hits.count == 1)
        return hits.first;
    char line[96];
    std::snprintf(line, sizeof(line), "%s (%s)", name, hits.count ? "several matches" : "not found");
    Note(w, line);
    return 0;
}
} // namespace detail

inline Where Resolve(const drive_resolve::Image& im)
{
    using namespace detail;
    Where w{};
    const uint32_t call = Unique(im, kTitleCall, "chud_show_cinematic_title's call of the chud's text setter", w);
    const uint32_t setter = Unique(im, kSetter, "the chud's text setter", w);
    if (call && setter)
    {
        uint8_t rel[4]{};
        uint32_t disp = 0;
        if (!drive_resolve::ReadOriginal(im, call + kTitleCallSetter, rel, 4))
            Note(w, "the call's operand");
        else
        {
            std::memcpy(&disp, rel, 4);
            const uint32_t target = call + kTitleCallNext + disp;   // wraps as the CPU's addition does
            if (target != setter)
                Note(w, "chud_show_cinematic_title calls another function than the setter");
            else if (drive_resolve::FunctionStartOf(im, setter) != setter)
                Note(w, "the setter is not a function's start (.pdata)");
            else
            {
                w.setterRva = setter;
                w.titleCallRva = call;
            }
        }
    }
    w.ok = !w.missing[0] && w.setterRva;
    if (!w.ok && !w.missing[0])
        Note(w, "an operand");
    return w;
}

// The HUD message text for a player (user < 4, string id, a 256-character buffer): the user's chud definition's
// message list (records of 0x1FC bytes, its tag at +0x184), the string lookup into the buffer, then the in-place
// post-pass over the buffer with its size (mov r8d,0x100; mov rdx,rsi; mov ecx,ebx; call). ODST's "Press [Back] to
// open VISR NAV data" and the like come through it (H3ODSTEK tags\ui\hud\hud_messages).
inline constexpr char kHudMessage[] =
    "83 F9 04 0F 8D ?? ?? ?? ?? 48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B 05 ?? ?? ?? ?? "
    "49 8B F0 8B EA 8B D9 48 85 C0 74 ?? 83 78 04 00 75 ?? 33 FF EB ?? 8B 48 04 48 8B 05 ?? ?? ?? ?? 48 8D 3C 88 8B CB "
    "E8 ?? ?? ?? ?? 48 63 C8 48 69 C1 FC 01 00 00 8B 8C 38 84 01 00 00 83 F9 FF 74 ?? 4C 8B C6 8B D5 E8 ?? ?? ?? ?? "
    "84 C0 74 ?? 41 B8 00 01 00 00 48 8B D6 8B CB E8";
// Its buffer: the post-pass's size, in characters; the rewrite (at most kMaxChars and a terminator) fits.
inline constexpr unsigned kHudMessageChars = 0x100;
struct HudWhere
{
    uint32_t rva = 0;
    char missing[96]{};
};
inline HudWhere ResolveHudMessage(const drive_resolve::Image& im)
{
    HudWhere h{};
    Where w{};
    const uint32_t at = detail::Unique(im, kHudMessage, "the HUD message text function", w);
    if (at && drive_resolve::FunctionStartOf(im, at) != at)
        detail::Note(w, "the HUD message text function is not a function's start (.pdata)");
    else if (at)
        h.rva = at;
    std::snprintf(h.missing, sizeof(h.missing), "%s", w.missing);
    return h;
}

// ------------------------------------------------------------------------------------------------------- the glyphs
// ODST's button pictures (the editing kit's glyph table, H3ODSTEK atlas_tag_test.exe): the actions each stands for.
namespace glyph
{
inline constexpr wchar_t kFlashlight = 0xE448, kBack = 0xE44C, kAccept = 0xE451, kCancel = 0xE452, kMove = 0xE453,
                         kLook = 0xE454;
inline constexpr wchar_t kWaypoint = 0xE501, kAltWaypoint = 0xE502, kRetrain = 0xE503, kPlaceWaypoint = 0xE505,
                         kNextWaypoint = 0xE506, kRemoveWaypoint = 0xE507, kPdaClose = 0xE508, kCommPlay = 0xE509,
                         kCommAll = 0xE50A, kCommPause = 0xE50B, kCommStop = 0xE50C, kMoveMap = 0xE50D,
                         kZoomMap = 0xE50E, kPrevRecord = 0xE50F, kNextRecord = 0xE510, kPrevScreen = 0xE511,
                         kNextScreen = 0xE512;
// MCC's own gamepad pictures, written in place of an action's picture by its glyph callback (+0x323DCC: the engine's
// buttons 10-15 and 0-1 as U+E100..E107, A B X Y LB RB LT RT; 2-5 the D-pad as E108..E10B; 6 Start E10C; 7 Back E10D)
// and the map's resolver (+0x323BB8: E110 and E111 the move and look sticks). The HUD's prompts arrive with them.
inline constexpr wchar_t kPadA = 0xE100, kPadRT = 0xE107, kPadUp = 0xE108, kPadDown = 0xE109, kPadLeft = 0xE10A,
                         kPadRight = 0xE10B, kPadBack = 0xE10D, kPadMoveStick = 0xE110, kPadLookStick = 0xE111;
inline constexpr unsigned kPadButtons = 8;   // U+E100..E107
// The kit's action pictures (the glyph table's button_jump ... button_use_equipment), as Project Reclaimer's HUD prompts
// arrive with them ("Hold <U+E45E> to swap for <weapon>"). E445 (action / reload), E45D (generic action) and E45E (pick
// up the primary weapon) are the use / pick-up prompts' button.
inline constexpr wchar_t kJump = 0xE443, kSwitchGrenade = 0xE444, kActionReload = 0xE445, kSwitchWeapon = 0xE446,
                         kMeleeAttack = 0xE447, kThrowGrenade = 0xE449, kFire = 0xE44A, kCrouch = 0xE44D,
                         kScopeZoom = 0xE44E, kActionGeneric = 0xE45D, kActionWeaponPrimary = 0xE45E,
                         kUseEquipment = 0xE461;
// The action row each of those names (vr_mapping::Count: not one of them).
inline constexpr vr_mapping::Action ActionOf(wchar_t g) noexcept
{
    switch (g)
    {
    case kJump: return vr_mapping::Jump;
    case kSwitchGrenade: return vr_mapping::SwitchGrenade;
    case kActionReload: case kActionGeneric: case kActionWeaponPrimary: return vr_mapping::Interact;
    case kSwitchWeapon: return vr_mapping::SwitchWeapon;
    case kMeleeAttack: return vr_mapping::Melee;
    case kThrowGrenade: return vr_mapping::Grenade;
    case kFire: return vr_mapping::Fire;
    case kCrouch: return vr_mapping::Crouch;
    case kScopeZoom: return vr_mapping::Zoom;
    case kUseEquipment: return vr_mapping::Equipment;
    default: return vr_mapping::Count;
    }
}
inline constexpr bool Private(wchar_t c) noexcept { return c >= 0xE000 && c <= 0xF8FF; }
// A picture of a control this file says in words. Any other picture (a weapon's, an item's) is the line's own.
inline constexpr bool Known(wchar_t g) noexcept
{
    return g == kFlashlight || g == kBack || (g >= kAccept && g <= kLook) || (g >= kWaypoint && g <= kRetrain) ||
           (g >= kPlaceWaypoint && g <= kNextScreen) || (g >= kPadA && g <= kPadRight) || g == kPadBack ||
           g == kPadMoveStick || g == kPadLookStick || ActionOf(g) != vr_mapping::Count;
}
} // namespace glyph
// The game's layout bit for each of U+E100..E107 (XInput's A B X Y LB RB; the triggers as the layout's bits 16, 17).
inline constexpr uint32_t kPadTransport[glyph::kPadButtons]{0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200,
                                                            1u << 16, 1u << 17};
inline constexpr unsigned kPadX = 2, kPadY = 3;

// -------------------------------------------------------------------------------------------- the player's controls
// What the text names, as the player has it. The prompt and map rows are VR Mappings' ODST rows; the rest is what
// presses the game's own button for it (the input thread reads the rows and the game's layout; ControlForAction,
// ControlForButton).
struct Controls
{
    vr_mapping::OdstBindings odst = vr_mapping::kOdstDefaults;
    vr_mapping::Input visr{};    // ODST's VISR: the flashlight action's row
    // What presses each gamepad button U+E100..E107 names (kPadTransport): X is the map's remove waypoint and COMM
    // stop, Y next waypoint, play all and repeat training; the HUD's prompts name any of them.
    vr_mapping::Input pad[glyph::kPadButtons]{};
    vr_mapping::Input action[vr_mapping::Count]{};   // each action row's control (the kit's action pictures)
    bool leftHanded = false;     // the gun hand is the left: its trigger and grip are the left ones
    bool dpadRightHand = false;  // the D-pad gesture's controller is the right one
};

// A row's control: its own (Automatic read as the game's layout has it) with its held-first controls, else its first
// further input. Unbound when the row has none.
inline vr_mapping::Input ControlForAction(vr_mapping::Action action, const vr_mapping::Overrides& effective,
                                          const vr_mapping::Inputs* rows) noexcept
{
    vr_mapping::Input in{};
    if (action >= vr_mapping::Count)
        return in;
    in.press = vr_mapping::Resolve(action, effective[action]);
    if (rows)
        in.held = (*rows)[action].held;
    if (vr_mapping::ListedSource(in.press))
        return in;
    in = vr_mapping::Input{};
    if (rows)
        for (const auto& more : (*rows)[action].more)
            if (vr_mapping::ListedSource(more.press))
                return more;
    return in;
}
// What presses a gamepad button (`button`, a layout bit): the row whose action the game's layout puts there, the
// `preferred` action first where several share it (use / pick up: the HUD's prompts are its), else the first. While the
// layout is unread, the action `fallback` holds it (ODST's default layout; Count: none).
inline vr_mapping::Input ControlForButton(uint32_t button, vr_mapping::Action fallback,
                                          const vr_mapping::Overrides& effective, const vr_mapping::Transports& layout,
                                          bool layoutKnown, const vr_mapping::Inputs* rows,
                                          vr_mapping::Action preferred = vr_mapping::Count) noexcept
{
    if (layoutKnown)
    {
        if (preferred < vr_mapping::Count && (layout[preferred] & button))
        {
            const auto in = ControlForAction(preferred, effective, rows);
            if (vr_mapping::ListedSource(in.press))
                return in;
        }
        for (unsigned a = 0; a < vr_mapping::Count; ++a)
            if (layout[a] & button)
            {
                const auto in = ControlForAction(static_cast<vr_mapping::Action>(a), effective, rows);
                if (vr_mapping::ListedSource(in.press))
                    return in;
            }
        return vr_mapping::Input{};
    }
    return ControlForAction(fallback, effective, rows);
}

// ------------------------------------------------------------------------------------------------------ the words
// One control said as an instruction: [held first][lead]verb object[ dir] -- "hold the left grip and pull the left
// trigger", "hold the left controller by your head and push its stick up".
struct Phrase
{
    bool ok = false;
    const wchar_t* held[vr_mapping::kHeld]{};   // objects held first ("the left grip")
    const wchar_t* lead = L"";                  // the D-pad gesture's "hold ... by your head and "
    const wchar_t* verb = L"";
    const wchar_t* object = L"";
    const wchar_t* dir = L"";
};

namespace detail
{
inline const wchar_t* GestureLead(const Controls& c) noexcept
{
    return c.dpadRightHand ? L"hold the right controller by your head and " : L"hold the left controller by your head and ";
}
// The gesture's stick: the left controller's, whichever controller is at the head (input.cpp's dpadMode reads moveX/Y
// and the left click).
inline const wchar_t* GestureStick(const Controls& c) noexcept
{
    return c.dpadRightHand ? L"the left stick" : L"its stick";
}
// The words for one control. False for one no row may list.
inline bool Words(int source, const Controls& c, const wchar_t*& verb, const wchar_t*& object, const wchar_t*& dir,
                  const wchar_t*& lead) noexcept
{
    using namespace vr_mapping;
    const bool left = c.leftHanded;
    lead = L"";
    dir = L"";
    switch (source)
    {
    case PrimaryTrigger: verb = L"pull"; object = left ? L"the left trigger" : L"the right trigger"; return true;
    case SupportTrigger: verb = L"pull"; object = left ? L"the right trigger" : L"the left trigger"; return true;
    case PrimaryGrip: verb = L"squeeze"; object = left ? L"the left grip" : L"the right grip"; return true;
    case SupportGrip: verb = L"squeeze"; object = left ? L"the right grip" : L"the left grip"; return true;
    case A: verb = L"press"; object = L"A"; return true;
    case B: verb = L"press"; object = L"B"; return true;
    case X: verb = L"press"; object = L"X"; return true;
    case Y: verb = L"press"; object = L"Y"; return true;
    case LeftClick: verb = L"click"; object = L"the left stick"; return true;
    case RightClick: verb = L"click"; object = L"the right stick"; return true;
    case DpadUp: case DpadDown: case DpadLeft: case DpadRight:
        lead = GestureLead(c);
        verb = L"push";
        object = GestureStick(c);
        dir = source == DpadUp ? L"up" : source == DpadDown ? L"down" : source == DpadLeft ? L"left" : L"right";
        return true;
    case HandOnGun: verb = L"put"; object = L"both hands on the gun"; return true;
    case RightStickUp: case RightStickDown: case RightStickLeft: case RightStickRight:
        verb = L"push";
        object = L"the right stick";
        dir = source == RightStickUp ? L"up" : source == RightStickDown ? L"down" : source == RightStickLeft ? L"left"
                                                                                                           : L"right";
        return true;
    default: return false;
    }
}
} // namespace detail

inline Phrase PhraseFor(const vr_mapping::Input& in, const Controls& c) noexcept
{
    Phrase p{};
    if (!vr_mapping::OdstListedSource(in.press))
        return p;
    if (!detail::Words(in.press, c, p.verb, p.object, p.dir, p.lead))
        return p;
    unsigned n = 0;
    for (const int held : in.held)
    {
        if (held == vr_mapping::Unbound || held == vr_mapping::Automatic)
            continue;
        const wchar_t *verb = L"", *object = L"", *dir = L"", *lead = L"";
        // A held control is a button, trigger, grip or the hand on the gun: never a direction or the gesture.
        if (!detail::Words(held, c, verb, object, dir, lead) || dir[0] || lead[0])
            return Phrase{};
        p.held[n++] = held == vr_mapping::HandOnGun ? L"both hands on the gun" : object;
    }
    p.ok = true;
    return p;
}
inline Phrase PhraseForRow(const vr_mapping::ExtraRow& row, const Controls& c) noexcept
{
    for (const auto& in : row)
        if (vr_mapping::OdstListedSource(in.press))
            return PhraseFor(in, c);
    return Phrase{};
}
inline Phrase Fixed(const wchar_t* lead, const wchar_t* verb, const wchar_t* object) noexcept
{
    Phrase p{};
    p.ok = true;
    p.lead = lead;
    p.verb = verb;
    p.object = object;
    return p;
}

// What a picture stands for: one phrase, or two said as "one or the other" (the map's zoom).
struct Said
{
    Phrase first, second;
    bool pair = false;
};
inline bool PictureFor(wchar_t g, const Controls& c, Said& out) noexcept
{
    using namespace vr_mapping;
    out = Said{};
    switch (g)
    {
    case glyph::kAccept: case glyph::kPlaceWaypoint: case glyph::kCommPlay: case glyph::kCommPause:
        out.first = PhraseForRow(c.odst[PromptConfirm], c); break;   // the game's A: the map's own, or accept
    case glyph::kCancel: out.first = PhraseForRow(c.odst[PromptReject], c); break;
    // The look tests read the game's look stick, which VR drives from the gun hand's aim (not the headset).
    case glyph::kLook: out.first = Fixed(L"", L"aim", L"with your gun hand"); break;
    case glyph::kMove: case glyph::kMoveMap: out.first = Fixed(L"", L"push", L"the left stick"); break;
    case glyph::kFlashlight: out.first = PhraseFor(c.visr, c); break;
    case glyph::kBack: case glyph::kPdaClose: case glyph::kPadBack:
        out.first = Fixed(detail::GestureLead(c), L"click", detail::GestureStick(c));
        break;
    case glyph::kWaypoint: case glyph::kAltWaypoint:   // the engine's default layout: the D-pad's up and down
        out.first = Fixed(detail::GestureLead(c), L"push", detail::GestureStick(c));
        out.first.dir = g == glyph::kWaypoint ? L"up" : L"down";
        break;
    case glyph::kPadUp: case glyph::kPadDown: case glyph::kPadLeft: case glyph::kPadRight:   // the D-pad itself
        out.first = Fixed(detail::GestureLead(c), L"push", detail::GestureStick(c));
        out.first.dir = g == glyph::kPadUp ? L"up" : g == glyph::kPadDown ? L"down" : g == glyph::kPadLeft ? L"left"
                                                                                                         : L"right";
        break;
    case glyph::kPadMoveStick: out.first = Fixed(L"", L"push", L"the left stick"); break;
    case glyph::kPadLookStick: out.first = Fixed(L"", L"aim", L"with your gun hand"); break;
    case glyph::kRetrain: case glyph::kNextWaypoint: case glyph::kCommAll: out.first = PhraseFor(c.pad[kPadY], c); break;
    case glyph::kRemoveWaypoint: case glyph::kCommStop: out.first = PhraseFor(c.pad[kPadX], c); break;
    case glyph::kZoomMap:
        out.first = PhraseForRow(c.odst[MapZoomIn], c);
        out.second = PhraseForRow(c.odst[MapZoomOut], c);
        out.pair = true;
        break;
    case glyph::kPrevRecord: out.first = PhraseForRow(c.odst[MapUp], c); break;
    case glyph::kNextRecord: out.first = PhraseForRow(c.odst[MapDown], c); break;
    case glyph::kPrevScreen: out.first = PhraseForRow(c.odst[MapPreviousPage], c); break;
    case glyph::kNextScreen: out.first = PhraseForRow(c.odst[MapNextPage], c); break;
    default:
        if (const auto a = glyph::ActionOf(g); a != Count)
        {
            out.first = PhraseFor(c.action[a], c);
            break;
        }
        if (g < glyph::kPadA || g > glyph::kPadRT)
            return false;
        out.first = PhraseFor(c.pad[g - glyph::kPadA], c);   // the row whose action the game puts on that button
        break;
    }
    return out.first.ok && (!out.pair || out.second.ok);
}

// ---------------------------------------------------------------------------------------------------- the writing
namespace detail
{
struct Writer
{
    wchar_t* out;
    unsigned cap;   // characters, not counting the terminator
    unsigned used = 0;
    bool overflow = false;
    bool capital = false;   // the next letter starts a sentence
    void Put(wchar_t ch) noexcept
    {
        if (used >= cap) { overflow = true; return; }
        if (capital && ch >= L'a' && ch <= L'z') ch = static_cast<wchar_t>(ch - L'a' + L'A');
        if (capital && ch != L' ') capital = false;
        out[used++] = ch;
    }
    void Put(const wchar_t* s) noexcept { while (s && *s) Put(*s++); }
    void Put(const wchar_t* s, unsigned n) noexcept { for (unsigned i = 0; i < n && s[i]; ++i) Put(s[i]); }
};
inline bool Same(const wchar_t* a, const wchar_t* b) noexcept { return std::wcscmp(a ? a : L"", b ? b : L"") == 0; }
inline bool SameHold(const Phrase& a, const Phrase& b) noexcept
{
    for (unsigned i = 0; i < vr_mapping::kHeld; ++i)
        if (!Same(a.held[i], b.held[i])) return false;
    return Same(a.lead, b.lead);
}
inline void PutHold(Writer& w, const Phrase& p) noexcept
{
    for (const wchar_t* held : p.held)
        if (held) { w.Put(L"hold "); w.Put(held); w.Put(L" and "); }
    w.Put(p.lead);
}
inline void PutPhrase(Writer& w, const Phrase& p) noexcept
{
    PutHold(w, p);
    w.Put(p.verb);
    w.Put(L' ');
    w.Put(p.object);
    if (p.dir[0]) { w.Put(L' '); w.Put(p.dir); }
}
// "the left grip" and "the right grip": the side and the rest, else false.
inline bool Sided(const wchar_t* object, const wchar_t*& side, unsigned& sideLength, const wchar_t*& rest) noexcept
{
    if (std::wcsncmp(object, L"the left ", 9) == 0) { side = L"left"; sideLength = 4; rest = object + 9; return true; }
    if (std::wcsncmp(object, L"the right ", 10) == 0) { side = L"right"; sideLength = 5; rest = object + 10; return true; }
    return false;
}
// Two controls as one or the other, shared words said once: "push the right stick up or down", "squeeze the left or
// right grip", else "press A or press B".
inline void PutEither(Writer& w, const Phrase& a, const Phrase& b) noexcept
{
    if (SameHold(a, b) && Same(a.verb, b.verb))
    {
        if (Same(a.object, b.object))
        {
            PutHold(w, a);
            w.Put(a.verb);
            w.Put(L' ');
            w.Put(a.object);
            if (a.dir[0] && b.dir[0] && !Same(a.dir, b.dir))
            {
                w.Put(L' '); w.Put(a.dir); w.Put(L" or "); w.Put(b.dir);
            }
            else if (a.dir[0]) { w.Put(L' '); w.Put(a.dir); }
            return;
        }
        const wchar_t *sideA = nullptr, *sideB = nullptr, *restA = nullptr, *restB = nullptr;
        unsigned lengthA = 0, lengthB = 0;
        if (!a.dir[0] && !b.dir[0] && Sided(a.object, sideA, lengthA, restA) &&
            Sided(b.object, sideB, lengthB, restB) && Same(restA, restB) && !Same(sideA, sideB))
        {
            PutHold(w, a);
            w.Put(a.verb);
            w.Put(L" the ");
            w.Put(sideA);
            w.Put(L" or ");
            w.Put(sideB);
            w.Put(L' ');
            w.Put(restA);
            return;
        }
    }
    PutPhrase(w, a);
    w.Put(L" or ");
    PutPhrase(w, b);
}
inline void PutSaid(Writer& w, const Said& s) noexcept
{
    if (s.pair) PutEither(w, s.first, s.second);
    else PutPhrase(w, s.first);
}

inline bool Space(wchar_t c) noexcept { return c == L' ' || c == L'\t'; }
inline const wchar_t* SkipSpace(const wchar_t* p, const wchar_t* end) noexcept
{
    while (p < end && Space(*p)) ++p;
    return p;
}
inline bool StartsWith(const wchar_t* p, const wchar_t* end, const wchar_t* word) noexcept
{
    const size_t n = std::wcslen(word);
    return static_cast<size_t>(end - p) >= n && std::wcsncmp(p, word, n) == 0;
}

// A choice line: "<picture> Label [<picture> Label]", the labels the English tutorials use. Each becomes "<control> to
// label", apart by ", ". False if the line is not one.
inline bool ChoiceLine(Writer& w, const wchar_t* p, const wchar_t* end, const Controls& c) noexcept
{
    static constexpr const wchar_t* kLabels[]{L"Accept Settings", L"Invert Camera", L"Continue", L"Accept", L"Decline",
                                              L"Repeat"};
    p = SkipSpace(p, end);
    if (p >= end || !glyph::Private(*p))
        return false;
    bool first = true;
    while (p < end)
    {
        Said said{};
        if (!glyph::Private(*p) || !PictureFor(*p, c, said))
            return false;
        p = SkipSpace(p + 1, end);
        const wchar_t* label = nullptr;
        for (const wchar_t* known : kLabels)
            if (StartsWith(p, end, known))
            {
                const wchar_t* after = p + std::wcslen(known);
                if (after == end || Space(*after)) { label = known; break; }
            }
        if (!label)
            return false;
        if (!first) w.Put(L", ");
        w.capital = first;
        PutSaid(w, said);
        w.Put(L" to ");
        for (const wchar_t* l = label; *l; ++l)
            w.Put(*l >= L'A' && *l <= L'Z' ? static_cast<wchar_t>(*l - L'A' + L'a') : *l);
        p = SkipSpace(p + std::wcslen(label), end);
        first = false;
    }
    return true;
}
// The rest of a sentence, each picture in it said in words ("... and <picture> to stop" -> "... and press X to stop"),
// runs of spaces as one. False on a picture it cannot say.
inline bool Rest(Writer& w, const wchar_t* p, const wchar_t* end, const Controls& c) noexcept
{
    bool space = false;
    for (; p < end; ++p)
    {
        if (Space(*p)) { space = true; continue; }
        if (space && w.used) w.Put(L' ');
        space = false;
        if (glyph::Known(*p))
        {
            Said said{};
            if (!PictureFor(*p, c, said))
                return false;
            PutSaid(w, said);
        }
        else
            w.Put(*p);
    }
    return true;
}
// A control held down: "hold B", "pull and hold the right trigger". False for a direction or the D-pad gesture.
inline bool HeldDown(Phrase& p) noexcept
{
    if (!p.ok || p.lead[0] || p.dir[0])
        return false;
    if (Same(p.verb, L"press")) p.verb = L"hold";
    else if (Same(p.verb, L"pull")) p.verb = L"pull and hold";
    else if (Same(p.verb, L"squeeze")) p.verb = L"squeeze and hold";
    else if (Same(p.verb, L"click")) p.verb = L"click and hold";
    else return false;
    return true;
}
// "Press|Use|Hold <picture> [or <picture>] to|for ...": the controls said in words, then the rest. False if the line is
// not one. "Hold" is the HUD's prompts (use / pick up, "Hold <RB> to swap for <weapon>").
inline bool SentenceLine(Writer& w, const wchar_t* p, const wchar_t* end, const Controls& c) noexcept
{
    p = SkipSpace(p, end);
    bool hold = false;
    if (StartsWith(p, end, L"Press ")) p += 6;
    else if (StartsWith(p, end, L"Use ")) p += 4;
    else if (StartsWith(p, end, L"Hold ")) { p += 5; hold = true; }
    else return false;
    p = SkipSpace(p, end);
    Said one{}, two{};
    if (p >= end || !glyph::Private(*p) || !PictureFor(*p, c, one))
        return false;
    p = SkipSpace(p + 1, end);
    bool either = false;
    if (StartsWith(p, end, L"or "))
    {
        p = SkipSpace(p + 3, end);
        if (p >= end || !glyph::Private(*p) || !PictureFor(*p, c, two) || one.pair || two.pair)
            return false;
        p = SkipSpace(p + 1, end);
        either = true;
    }
    if (!StartsWith(p, end, L"to ") && !StartsWith(p, end, L"for "))
        return false;
    if (hold && (one.pair || !HeldDown(one.first) || (either && !HeldDown(two.first))))
        return false;
    w.capital = true;
    if (either) PutEither(w, one.first, two.first);
    else PutSaid(w, one);
    w.Put(L' ');
    return Rest(w, p, end, c);
}
// Breaks a line longer than `width` at its last space within it (the chud keeps the stock lines' widths).
inline void Wrap(wchar_t* text, unsigned length, unsigned width) noexcept
{
    unsigned start = 0, lastSpace = ~0u;
    for (unsigned i = 0; i < length; ++i)
    {
        if (text[i] == L'\n') { start = i + 1; lastSpace = ~0u; continue; }
        if (text[i] == L' ') lastSpace = i;
        if (i - start >= width && lastSpace != ~0u && lastSpace > start)
        {
            text[lastSpace] = L'\n';
            start = lastSpace + 1;
            lastSpace = ~0u;
        }
    }
}
} // namespace detail

// ODST's English tutorial lines that name a control, as the kit writes them (H3ODSTEK
// tags\ui\chapter_titles\atlas\h100.multilingual_unicode_string_list, English is every 11th string). Project Reclaimer
// puts its own button pictures, or a keyboard key's name in brackets ("[Z]"), in place of ODST's before the line reaches
// the mod; such a line is matched to its kit line by its words, and the kit line is said.
namespace detail
{
inline constexpr const wchar_t* kKitLines[] = {
    // The HUD's VISR prompts (tags\ui\hud\hud_messages).
    L"Press \xE44C" L" to open VISR NAV data",
    L"Press \xE44C" L" to open VISR COMM data",
    L"Press \xE44C" L" to open VISR INTEL data",
    L"Press \xE44C" L" to open VISR database",
    L"Press \xE44C" L" to activate Visual Intelligence System",
    // The tutorials' titles (tags\ui\chapter_titles\atlas\h100).
    L"Use  \xE454" L"  to look around the pod",
    L"Do you like these settings?\r\n   \xE451" L"  Accept Settings    \xE452" L"  Invert Camera",
    L"Your STAMINA will regenerate over time\r\n \xE451" L"  Continue",
    L"Avoid combat to regain STAMINA\r\n \xE451" L"  Continue",
    L"Would you like to repeat HEALTH training?\r\n   \xE503" L"  Repeat    \xE452" L"  Decline",
    L"Find a medical kiosk to restore HEALTH\r\n \xE451" L"  Continue",
    L"Press \xE448" L"  to activate your VISR",
    L"Your VISR will help with target identification\r\n \xE451" L"  Continue",
    L"Friendly units appear GREEN\r\n \xE451" L"  Continue",
    L"Enemy units appear RED\r\n \xE451" L"  Continue",
    L"Equipment and unlocked doors appear BLUE\r\n \xE451" L"  Continue",
    L"Important objects appear YELLOW\r\n \xE451" L"  Continue",
    L"Use your VISR to conduct a detailed search\r\n \xE451" L"  Continue",
    L"Would you like to repeat VISR training?\r\n   \xE503" L" Repeat    \xE452" L"  Decline",
    L"You are separated from your ODST squad\r\n \xE451" L"  Continue",
    L"Contact city SUPERINTENDENT for assistance\r\n \xE451" L"  Continue",
    L"Press  \xE501" L"  or  \xE502" L"  for assistance",
    L"Press  \xE44C" L"  to activate VISR database",
    L"NAV data will help you find your ODST squad\r\n \xE451" L"  Continue",
    L"The green arrow indicates your current location\r\n \xE451" L"  Continue",
    L"Use \xE50D" L"  to move map",
    L"Use \xE50E" L"  to zoom map",
    L"Use  \xE501" L"  or  \xE502" L"  to switch between NAV beacons",
    L"Press  \xE505" L"  to place a waypoint\r\n \xE451" L"  Continue",
    L"Note the waypoint marker in your compass\r\n \xE451" L"  Continue",
    L"Press  \xE506" L"  to cycle waypoint selection",
    L"New NAV data!\r\n \xE451" L"  Continue",
    L"Use  \xE501" L"  or  \xE502" L"  to select the TAYARI PLAZA beacon",
    L"Note the beacon marker in your compass\r\n \xE451" L"  Continue",
    L"TAYARI PLAZA is the last known location of an ODST squadmate\r\n \xE451" L"  Continue",
    L"Proceed to the beacon and begin your search\r\n \xE451" L"  Continue",
    L"Press  \xE511" L"  or  \xE512" L"  to check INTEL data",
    L"All mission objectives will be stored here\r\n \xE451" L"  Continue",
    L"You have completed your first objective\r\n \xE451" L"  Continue",
    L"Press  \xE44C" L"  to open VISR database",
    L"Press  \xE511" L"  or  \xE512" L"  to check COMM data",
    L"You may find COMM data hidden in the city\r\n \xE451" L"  Continue",
    L"All audio and visual data will be stored here\r\n \xE451" L"  Continue",
    L"Use  \xE50F" L"  or  \xE510" L"  to select data block",
    L"Press  \xE509" L"  to play COMM data and  \xE50C" L"  to stop",
    L"COMM data will play even if you close your VISR database\r\n \xE451" L"  Continue",
    L"Would you like to repeat VISR database training?\r\n   \xE503" L"  Repeat    \xE452" L"  Decline",
    L"Press  \xE44C" L"  or  \xE452" L"  to deactivate VISR database",
    L"You have reached the NAV beacon\r\n \xE451" L"  Continue",
    L"Search building for a clue to your squadmate's location\r\n \xE451" L"  Continue",
    L"UPLIFT RESERVE is the last known location of an ODST squadmate\r\n \xE451" L"  Continue",
    L"Now leaving MOMBASA STREETS\r\n \xE451" L"  Continue",
    L"Superintendent SUPPLY-CACHE unlocked!\r\n \xE451" L"  Continue",
    L"Check NAV data for supply-cache locations\r\n \xE451" L"  Continue",
    L"Your aiming has been set to Normal\r\n \xE451" L"  Continue",
    L"Your aiming has been set to Inverted\r\n \xE451" L"  Continue",
    L"Settings can be changed from the PAUSE Menu\r\n \xE451" L"  Continue",
    L"Your VISR is currently active\r\n \xE451" L"  Continue",
    L"Press  \xE507" L"  to remove waypoint",
    L"New INTEL data!\r\n \xE451" L"  Continue",
    L"Contact city SUPERINTENDENT\r\n \xE451" L"  Continue",
    L"Search TAYARI PLAZA for ODST squadmate\r\n \xE451" L"  Continue",
    L"Objective Completed!\r\n \xE451" L"  Continue",
    L"Would you like to perform TRAINING?\r\n   \xE503" L"  Accept    \xE452" L"  Decline",
};
// Walks a line as words: whitespace runs are one space, a picture or a bracketed key name ("[Z]", "[Left Mouse]") is one
// mark; other characters are themselves.
struct FormCursor
{
    const wchar_t* p;
    static bool White(wchar_t ch) noexcept { return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n'; }
    unsigned Next() noexcept
    {
        if (!*p) return 0;
        if (White(*p)) { while (White(*p)) ++p; return 1; }
        if (glyph::Private(*p)) { ++p; return 2; }
        if (*p == L'[')
        {
            const wchar_t* q = p + 1;
            unsigned n = 0;
            while (*q && *q != L']' && *q != L'\r' && *q != L'\n' && n < 24) { ++q; ++n; }
            if (*q == L']' && n) { p = q + 1; return 2; }
        }
        return 3u + *p++;
    }
};
inline bool SameForm(const wchar_t* a, const wchar_t* b) noexcept
{
    FormCursor x{a}, y{b};
    bool mark = false;
    for (unsigned n = 0; n <= 2 * kMaxChars; ++n)
    {
        const unsigned u = x.Next(), v = y.Next();
        if (u != v) return false;
        mark |= u == 2;
        if (!u) return mark;
    }
    return false;
}
// The kit line `in` is a form of (its pictures ODST's own), or null.
inline const wchar_t* KitLineFor(const wchar_t* in) noexcept
{
    for (const wchar_t* line : kKitLines)
        if (SameForm(in, line)) return line;
    return nullptr;
}
} // namespace detail

// The text for `in` in VR words, into `out` (terminated): its length, or 0 to keep the game's own text (no picture in
// it, a line of another form or language, a picture not known, a control unbound, or too long for the chud). A line
// of a kit line's form (Project Reclaimer's pictures or key names in ODST's places) is said as that kit line.
inline constexpr unsigned kWrapWidth = 60;
inline unsigned Rewrite(const wchar_t* in, const Controls& c, wchar_t (&out)[kMaxChars + 1]) noexcept
{
    using namespace detail;
    out[0] = 0;
    if (!in)
        return 0;
    if (const wchar_t* kit = KitLineFor(in))
        in = kit;
    if (!in)
        return 0;
    const wchar_t* end = in;
    bool pictures = false;
    while (*end && end - in <= static_cast<ptrdiff_t>(kMaxChars))
        pictures |= glyph::Known(*end++);
    if (!pictures || *end)
        return 0;
    // Wrapping inserts line feeds in place of spaces: the lines keep "\r\n" as the stock text writes them.
    Writer w{out, kMaxChars};
    for (const wchar_t* line = in; line < end;)
    {
        const wchar_t* stop = line;
        while (stop < end && *stop != L'\r' && *stop != L'\n') ++stop;
        bool has = false;
        for (const wchar_t* p = line; p < stop; ++p) has |= glyph::Known(*p);
        if (!has)
            w.Put(line, static_cast<unsigned>(stop - line));
        else
        {
            // A form tried and refused leaves nothing behind.
            const unsigned mark = w.used;
            const bool overflow = w.overflow;
            if (!ChoiceLine(w, line, stop, c))
            {
                w.used = mark;
                w.overflow = overflow;
                if (!SentenceLine(w, line, stop, c))
                    return 0;
            }
        }
        w.capital = false;
        line = stop;
        while (line < end && (*line == L'\r' || *line == L'\n')) w.Put(*line++);
    }
    if (w.overflow)
        return 0;
    out[w.used] = 0;
    // Each long line breaks at a space; the break is a line feed after a carriage return, as the stock text's.
    wchar_t wrapped[kMaxChars + 1]{};
    unsigned n = 0;
    {
        Wrap(out, w.used, kWrapWidth);
        for (unsigned i = 0; i < w.used; ++i)
        {
            const bool inserted = out[i] == L'\n' && (i == 0 || out[i - 1] != L'\r');
            if (inserted)
            {
                if (n + 2 > kMaxChars) return 0;
                wrapped[n++] = L'\r';
                wrapped[n++] = L'\n';
            }
            else
            {
                if (n + 1 > kMaxChars) return 0;
                wrapped[n++] = out[i];
            }
        }
    }
    std::memcpy(out, wrapped, n * sizeof(wchar_t));
    out[n] = 0;
    return n;
}
} // namespace odst_prompt_text
