#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include "native_subtitles.h"
#include "sigscan.h"
#include "title_adapter.h"
#include "vr.h"
#include "../common/config.h"
#include "../common/log.h"
#include "../common/subtitle_logic.h"

namespace {
// Martysl1's two independent MCC UnifiedSubtitles callers select the same
// final UTF-16 handoff. Only the observed production caller is caption ingress.
constexpr char kTimedCaller[] =
    "F3 0F 10 46 34 F3 0F 5C 46 30 8B 43 34 4C 8D 4D FF 48 83 7D 17 08 "
    "4C 0F 43 4D FF 4C 8D 43 2C 48 8D 53 1C 48 8D 4B 0C F3 0F 11 44 24 28 "
    "89 44 24 20 E8 ?? ?? ?? ??";
constexpr char kProductionCaller[] =
    "F3 0F 10 45 7F 4D 8B C4 F3 0F 11 44 24 28 49 8B D7 49 8B CE "
    "44 89 6C 24 20 E8 ?? ?? ?? ?? 48 8B 4D EF 48 33 CC E8 ?? ?? ?? ??";
constexpr unsigned char kTargetPrologue[] = {
    0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,
    0x48,0x89,0x78,0x18,0x4C,0x89,0x70,0x20,0x55,0x48,0x8B,0xEC,
    0x48,0x81,0xEC,0x80,0x00,0x00,0x00 };
using Handoff = void(__fastcall*)(const void*, const void*, const void*,
    const wchar_t*, uint32_t, float);
Handoff g_original = nullptr;
uintptr_t g_productionReturn = 0;
bool g_attempted = false;
using InterfaceHandoff = bool(__fastcall*)(void*,const void*,const void*,const void*,
    const wchar_t*,float,uint32_t);
InterfaceHandoff g_originalInterface = nullptr;
#include "native_subtitle_authority.inl"
constexpr char kInterfaceWrapper[] =
    "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 40 B1 79 49 8B D9 49 8B F8 "
    "48 8B F2 E8 ?? ?? ?? ?? 84 C0 74 ?? F3 0F 10 44 24 78 4C 8B CB "
    "8B 84 24 80 00 00 00 4C 8B C7 48 8B 0D ?? ?? ?? ?? 48 8B D6 "
    "F3 0F 11 44 24 30 89 44 24 28 48 8B 44 24 70 48 89 44 24 20 E8 ?? ?? ?? ??";

#include "native_subtitle_queue.inl"
struct Slot { std::atomic<unsigned> state{0}; Event event{}; };
Slot g_slots[16];
std::atomic<uint64_t> g_sequence{0}, g_dropped{0};
std::shared_ptr<const NativeSubtitleImage> g_image;
uint64_t g_consumed = 0, g_revision = 0;
RasterCaption g_lastRasterized{};
// The notice (NativeNotice_Show): its text under the lock, its image published like a caption's.
constexpr uint32_t kNoticeCapacity = 256, kNoticeHeadingCapacity = 64;
SRWLOCK g_noticeLock = SRWLOCK_INIT;
wchar_t g_noticeText[kNoticeCapacity]{};
wchar_t g_noticeHeading[kNoticeHeadingCapacity]{};
uint32_t g_noticeLength = 0, g_noticeHeadingLength = 0;
NoticeTone g_noticeTone = NoticeTone::Info;
uint64_t g_noticeExpires = 0, g_noticeSerial = 0;
uint64_t g_noticeRasterized = 0; // worker
std::shared_ptr<const NativeSubtitleImage> g_noticeImage;
// The labels (NativeLabel_Set): the same, without an expiry, one per slot.
struct Label {
    wchar_t text[64]{};
    uint32_t length = 0;
    float opacity = 0.92f;
    uint64_t serial = 0, rasterized = 0;
    std::shared_ptr<const NativeSubtitleImage> image;
};
SRWLOCK g_labelLock = SRWLOCK_INIT;
Label g_labels[kNativeLabelSlots];

bool CopyText(const wchar_t* source, Event& event) noexcept {
    if (!source || (reinterpret_cast<uintptr_t>(source) & 1)) return false;
    __try {
        for (uint32_t i = 0; i < subtitles::kTextCapacity; ++i) {
            const wchar_t c = source[i];
            event.text[i] = c;
            if (!c) { event.length = i; return i != 0; }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
    return false;
}

uintptr_t Unique(uintptr_t base, size_t size, const char* pattern) {
    const uintptr_t first = sig::Find(base, size, pattern);
    if (!first || first < base || first >= base + size) return 0;
    return sig::Find(first + 1, base + size - first - 1, pattern) ? 0 : first;
}

#include "native_subtitles_sources.inl"

void __fastcall HookHandoff(const void* a, const void* b, const void* c,
    const wchar_t* text, uint32_t style, float seconds) {
    const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    const GameTitle title = TitleAdapter_GetActiveTitle();
    const RuntimeMode mode = TitleAdapter_GetRuntimeMode();
    // Theatre has a distinct native lifetime source; do not misclassify an
    // ordinary speech handoff as an authored cinematic subtitle.
    if (caller == g_productionReturn &&
        (title == GameTitle::Halo3 || title == GameTitle::Halo3ODST) &&
        (mode == RuntimeMode::Gameplay || mode == RuntimeMode::Vehicle || mode == RuntimeMode::Turret) &&
        !VR_IsCutsceneTheaterActive())
        NativeSubtitles_Capture(title, TitleAdapter_GetGeneration(title), text, seconds, false);
    g_original(a, b, c, text, style, seconds);
}

#include "native_subtitle_interface.inl"

bool __fastcall HookInterfaceHandoff(void* self,const void* a,const void* b,const void* c,
    const wchar_t* text,float seconds,uint32_t style) {
    return DispatchNativeSubtitleInterface(reinterpret_cast<uintptr_t>(_ReturnAddress()),
        self,a,b,c,text,seconds,style);
}

void InstallHostObserver() {
    if (g_attempted) return;
    g_attempted = true;
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    size_t size = 0;
    __try {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return;
        size = nt->OptionalHeader.SizeOfImage;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return; }
    const uintptr_t timed = Unique(base, size, kTimedCaller);
    const uintptr_t production = Unique(base, size, kProductionCaller);
    const uintptr_t target = timed ? sig::RipTarget(timed + 0x32, timed + 0x36) : 0;
    const uintptr_t other = production ? sig::RipTarget(production + 0x1A, production + 0x1E) : 0;
    DWORD64 imageBase = 0;
    const auto* function = target ? RtlLookupFunctionEntry(target, &imageBase, nullptr) : nullptr;
    if (!timed || !production || !target || target != other || target < base ||
        target > base + size - sizeof(kTargetPrologue) || !function ||
        imageBase + function->BeginAddress != target ||
        memcmp(reinterpret_cast<void*>(target), kTargetPrologue, sizeof(kTargetPrologue))) {
        LOG("Native subtitles: shared host handoff proof unavailable; stock subtitles retained");
        return;
    }
    g_productionReturn = production + 0x1E;
    // Superseded by exact title callers at the normal native interface.
    // Keep the earlier observer dormant for the documented rollback path.
    if constexpr(false) {
    const MH_STATUS created = MH_CreateHook(reinterpret_cast<void*>(target),
        reinterpret_cast<void*>(&HookHandoff), reinterpret_cast<void**>(&g_original));
    if (created != MH_OK) {
        LOG("Native subtitles: shared host observer create failed (%d); stock retained", created);
        return;
    }
    const MH_STATUS enabled = MH_EnableHook(reinterpret_cast<void*>(target));
    if (enabled != MH_OK) {
        MH_RemoveHook(reinterpret_cast<void*>(target));
        g_original = nullptr;
        LOG("Native subtitles: shared host observer enable failed (%d); stock retained", enabled);
        return;
    }
    // Host EXE and this injected DLL live for the process; the hook is never
    // removed on title switches. Its ingress checks current title/generation.
    LOG("Native subtitles: H3/ODST shared localized-text observer ready (host +0x%llX); native call preserved",
        static_cast<unsigned long long>(target - base));
    }
    const uintptr_t wrapper=Unique(base,size,kInterfaceWrapper);
    DWORD64 productionImageBase=0,wrapperImageBase=0;
    const auto productionFunction=RtlLookupFunctionEntry(production,&productionImageBase,nullptr);
    const auto wrapperFunction=wrapper?RtlLookupFunctionEntry(wrapper,&wrapperImageBase,nullptr):nullptr;
    const uintptr_t wrapperCallee=wrapper?sig::RipTarget(wrapper+0x55,wrapper+0x59):0;
    if(!wrapper || !productionFunction || !wrapperFunction ||
        wrapperImageBase+wrapperFunction->BeginAddress!=wrapper ||
        productionImageBase+productionFunction->BeginAddress!=wrapperCallee) {
        LOG("Native subtitles: host interface proof unavailable; stock captions retained");
        return;
    }
    const auto wrapperCreated=MH_CreateHook(reinterpret_cast<void*>(wrapper),
        reinterpret_cast<void*>(&HookInterfaceHandoff),reinterpret_cast<void**>(&g_originalInterface));
    if(wrapperCreated!=MH_OK) {
        LOG("Native subtitles: native interface observer create failed (%d); stock retained",wrapperCreated);return;
    }
    const auto wrapperEnabled=MH_EnableHook(reinterpret_cast<void*>(wrapper));
    if(wrapperEnabled!=MH_OK) {
        MH_RemoveHook(reinterpret_cast<void*>(wrapper));g_originalInterface=nullptr;
        LOG("Native subtitles: native interface observer enable failed (%d); stock retained",wrapperEnabled);return;
    }
    LOG("Native subtitles: native interface observer ready (host +0x%llX); return/caller proof required",
        static_cast<unsigned long long>(wrapper-base));
}

// `largestPx`: the font's largest size; it shrinks from there until the text fits.
std::shared_ptr<const NativeSubtitleImage> Rasterize(const RasterCaption& event, int largestPx = 66) {
    const wchar_t* normalized=event.text;
    if(!event.length) return {};
    constexpr int w = NativeSubtitleImage::width, h = NativeSubtitleImage::height;
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return {};
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w; info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) { if (bitmap) DeleteObject(bitmap); DeleteDC(dc); return {}; }
    const auto previousBitmap = SelectObject(dc, bitmap);
    memset(bits, 0, w * h * 4);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255,255,255));
    HFONT font = nullptr;
    HGDIOBJ previousFont = nullptr;
    RECT measured{48,24,w-48,h-24};
    for (int px = largestPx; px >= 18; px -= 4) {
        font = CreateFontW(-px, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (!font) break;
        previousFont = SelectObject(dc, font);
        measured = {48,24,w-48,h-24};
        DrawTextW(dc, normalized, -1, &measured, DT_CENTER|DT_WORDBREAK|DT_NOPREFIX|DT_CALCRECT);
        if (measured.bottom - measured.top <= h - 48 || px == 18) break;
        SelectObject(dc, previousFont); DeleteObject(font); font = nullptr;
    }
    if (!font) {
        SelectObject(dc,previousBitmap);DeleteObject(bitmap);DeleteDC(dc);return {};
    }
    if (font) {
        const int textHeight = measured.bottom - measured.top;
        RECT target{48,(h-textHeight)/2,w-48,(h+textHeight)/2};
        DrawTextW(dc, normalized, -1, &target, DT_CENTER|DT_WORDBREAK|DT_NOPREFIX);
        GdiFlush();
    }
    auto result = std::make_shared<NativeSubtitleImage>();
    result->title = event.title; result->generation = event.generation;
    result->expiresAtMs = event.expires; result->revision = ++g_revision;
    result->theatre = event.theatre;
    auto rgba = std::make_shared<std::vector<uint8_t>>(w*h*4);
    // A bounded two-pass dilation supplies a black outline. RGB contains
    // premultiplied white coverage; alpha additionally contains the outline.
    std::vector<uint8_t> dilation(w*h);
    const auto* pixels = static_cast<const uint8_t*>(bits);
    for (int y=0; y<h; ++y) for (int x=0; x<w; ++x) {
        uint8_t a = 0;
        for (int dx=-3; dx<=3; ++dx)
            if (x+dx>=0 && x+dx<w) a=std::max(a,pixels[4*(y*w+x+dx)]);
        dilation[y*w+x] = a;
    }
    for (int y=0; y<h; ++y) for (int x=0; x<w; ++x) {
        const int i=y*w+x;
        uint8_t a=0;
        for (int dy=-3; dy<=3; ++dy)
            if (y+dy>=0 && y+dy<h) a=std::max(a,dilation[(y+dy)*w+x]);
        const uint8_t white=pixels[4*i];
        (*rgba)[4*i]=(*rgba)[4*i+1]=(*rgba)[4*i+2]=white;
        (*rgba)[4*i+3]=a;
    }
    if (font) { SelectObject(dc,previousFont); DeleteObject(font); }
    SelectObject(dc,previousBitmap); DeleteObject(bitmap); DeleteDC(dc);
    result->rgba = rgba;
    return result;
}
}

// A WAYPOINT LABEL in the style of Halo 3's own navpoints (user 2026-11-16: the caption look "doesn't really look
// halo style"): the word in the HUD's light blue, in a condensed squared face like the HUD's (Bahnschrift, shipped
// with Windows 10; Segoe UI where it is missing), with a soft blue glow instead of an outline, over a small
// downward chevron whose tip is the image's bottom centre (the point it marks). Premultiplied alpha, slightly
// see-through like the HUD.
static std::shared_ptr<const NativeSubtitleImage> RasterizeLabel(const RasterCaption& event, float opacity) {
    if (!event.length) return {};
    constexpr int w = NativeSubtitleImage::width, h = NativeSubtitleImage::height;
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return {};
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w; info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) { if (bitmap) DeleteObject(bitmap); DeleteDC(dc); return {}; }
    const auto previousBitmap = SelectObject(dc, bitmap);
    memset(bits, 0, size_t(w) * h * 4);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255,255,255));
    // The word: the top 300 px, letters spaced a little as the HUD's are.
    auto makeFont = [](const wchar_t* face) {
        return CreateFontW(-250, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
            CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face);
    };
    HFONT font = makeFont(L"Bahnschrift SemiBold SemiConden");
    HGDIOBJ previousFont = font ? SelectObject(dc, font) : nullptr;
    wchar_t face[LF_FACESIZE]{};
    if (font) GetTextFaceW(dc, LF_FACESIZE, face);
    if (!font || _wcsnicmp(face, L"Bahnschrift", 11) != 0) {
        if (font) { SelectObject(dc, previousFont); DeleteObject(font); }
        font = makeFont(L"Bahnschrift");
        if (font) previousFont = SelectObject(dc, font);
        if (font) GetTextFaceW(dc, LF_FACESIZE, face);
        if (font && _wcsnicmp(face, L"Bahnschrift", 11) != 0) {
            SelectObject(dc, previousFont); DeleteObject(font);
            font = makeFont(L"Segoe UI Semibold");
            if (font) previousFont = SelectObject(dc, font);
        }
    }
    if (!font) { SelectObject(dc, previousBitmap); DeleteObject(bitmap); DeleteDC(dc); return {}; }
    SetTextCharacterExtra(dc, 18);
    RECT text{0, 20, w, 320};
    DrawTextW(dc, event.text, -1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    // The chevron: a downward V, open in the middle, its tip at the bottom centre.
    {
        HBRUSH white = CreateSolidBrush(RGB(255,255,255));
        HPEN none = CreatePen(PS_NULL, 0, 0);
        const auto previousBrush = SelectObject(dc, white);
        const auto previousPen = SelectObject(dc, none);
        const POINT outer[]{{w / 2 - 110, 345}, {w / 2 + 110, 345}, {w / 2, 490}};
        Polygon(dc, outer, 3);
        SelectObject(dc, GetStockObject(BLACK_BRUSH));
        const POINT inner[]{{w / 2 - 62, 345}, {w / 2 + 62, 345}, {w / 2, 427}};
        Polygon(dc, inner, 3);
        SelectObject(dc, previousBrush); SelectObject(dc, previousPen);
        DeleteObject(white); DeleteObject(none);
    }
    GdiFlush();
    // Coverage, a blurred copy of it for the glow (two box passes), then colour.
    const auto* pixels = static_cast<const uint8_t*>(bits);
    std::vector<float> cover(size_t(w) * h), blur(size_t(w) * h), tmp(size_t(w) * h);
    for (int i = 0; i < w * h; ++i) cover[i] = pixels[4 * i] / 255.0f;
    constexpr int r = 14;
    for (int pass = 0; pass < 2; ++pass) {
        const std::vector<float>& src = pass == 0 ? cover : blur;
        for (int y = 0; y < h; ++y) {
            float sum = 0;
            for (int x = -r; x <= r; ++x) sum += src[size_t(y) * w + std::clamp(x, 0, w - 1)];
            for (int x = 0; x < w; ++x) {
                tmp[size_t(y) * w + x] = sum / float(2 * r + 1);
                sum += src[size_t(y) * w + std::min(x + r + 1, w - 1)] - src[size_t(y) * w + std::max(x - r, 0)];
            }
        }
        for (int x = 0; x < w; ++x) {
            float sum = 0;
            for (int y = -r; y <= r; ++y) sum += tmp[size_t(std::clamp(y, 0, h - 1)) * w + x];
            for (int y = 0; y < h; ++y) {
                blur[size_t(y) * w + x] = sum / float(2 * r + 1);
                sum += tmp[size_t(std::min(y + r + 1, h - 1)) * w + x] - tmp[size_t(std::max(y - r, 0)) * w + x];
            }
        }
    }
    // Halo 3's HUD blue (measured off the game's ammo bars and tracker in an FA capture) and a deeper blue glow.
    constexpr float kText[3]{0.47f, 0.82f, 1.00f}, kGlow[3]{0.12f, 0.52f, 1.00f};
    const float kOpacity = std::clamp(opacity, 0.05f, 1.0f);
    constexpr float kGlowStrength = 1.6f;
    auto rgba = std::make_shared<std::vector<uint8_t>>(size_t(w) * h * 4);
    for (int i = 0; i < w * h; ++i) {
        const float c = cover[i];
        const float g = std::min(blur[i] * kGlowStrength, 1.0f) * 0.55f * (1.0f - c);
        const float a = std::min(c + g, 1.0f) * kOpacity;
        for (int k = 0; k < 3; ++k) {
            const float v = (kText[k] * c + kGlow[k] * g) * kOpacity;   // premultiplied
            (*rgba)[4 * i + k] = uint8_t(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
        }
        (*rgba)[4 * i + 3] = uint8_t(std::clamp(a, 0.0f, 1.0f) * 255.0f + 0.5f);
    }
    SelectObject(dc, previousFont); DeleteObject(font);
    SelectObject(dc, previousBitmap); DeleteObject(bitmap); DeleteDC(dc);
    auto result = std::make_shared<NativeSubtitleImage>();
    result->expiresAtMs = event.expires; result->revision = ++g_revision;
    result->rgba = rgba;
    return result;
}

// The HUD's face at `px` (a positive height is the cell, negative the letters), selected into `dc`: Bahnschrift
// SemiBold SemiCondensed, as the waypoint labels, else Bahnschrift, else Segoe UI Semibold. Returns the font to
// delete and the one it replaced in `previous`.
static HFONT SelectHudFont(HDC dc, int px, HGDIOBJ& previous) {
    previous = nullptr;
    const wchar_t* faces[]{L"Bahnschrift SemiBold SemiConden", L"Bahnschrift", L"Segoe UI Semibold"};
    for (int i = 0; i < 3; ++i) {
        HFONT font = CreateFontW(-px, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
            CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, faces[i]);
        if (!font) continue;
        HGDIOBJ before = SelectObject(dc, font);
        wchar_t face[LF_FACESIZE]{};
        GetTextFaceW(dc, LF_FACESIZE, face);
        if (i == 2 || _wcsnicmp(face, L"Bahnschrift", 11) == 0) { previous = before; return font; }
        SelectObject(dc, before);
        DeleteObject(font);
    }
    return nullptr;
}

// Two box-blur passes (about a Gaussian) of a w x h coverage map, radius r.
static void BlurTwice(const std::vector<float>& cover, std::vector<float>& blur, int w, int h, int r) {
    std::vector<float> tmp(size_t(w) * h);
    blur.assign(size_t(w) * h, 0.0f);
    for (int pass = 0; pass < 2; ++pass) {
        const std::vector<float>& src = pass == 0 ? cover : blur;
        for (int y = 0; y < h; ++y) {
            float sum = 0;
            for (int x = -r; x <= r; ++x) sum += src[size_t(y) * w + std::clamp(x, 0, w - 1)];
            for (int x = 0; x < w; ++x) {
                tmp[size_t(y) * w + x] = sum / float(2 * r + 1);
                sum += src[size_t(y) * w + std::min(x + r + 1, w - 1)] - src[size_t(y) * w + std::max(x - r, 0)];
            }
        }
        for (int x = 0; x < w; ++x) {
            float sum = 0;
            for (int y = -r; y <= r; ++y) sum += tmp[size_t(std::clamp(y, 0, h - 1)) * w + x];
            for (int y = 0; y < h; ++y) {
                blur[size_t(y) * w + x] = sum / float(2 * r + 1);
                sum += tmp[size_t(std::min(y + r + 1, h - 1)) * w + x] - tmp[size_t(std::max(y - r, 0)) * w + x];
            }
        }
    }
}

// THE NOTICE in the style of Halo 3's HUD (user 2026-11-21: "stylize the mod's panel to look more in line with Halo
// and make it primary"): a dark see-through bar sized to the text, a thin line along its top and bottom and brighter
// bracket corners, an optional small letter-spaced heading, and the text in the HUD's face with a soft glow. Blue
// for information, amber for a warning. The text is as large as fits (two or three lines at most at the smallest).
// Worker only; premultiplied alpha.
static std::shared_ptr<const NativeSubtitleImage> RasterizeNotice(const wchar_t* heading, uint32_t headingLength,
    const wchar_t* text, uint32_t textLength, NoticeTone tone, uint64_t expires) {
    if (!textLength) return {};
    constexpr int w = NativeSubtitleImage::width, h = NativeSubtitleImage::height;
    constexpr int kMargin = 16, kPadX = 80, kPadY = 30, kGap = 6, kHeadingPx = 50, kSmallestPx = 44;
    constexpr int kMaxTextW = w - 2 * kMargin - 2 * kPadX;
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return {};
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w; info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) { if (bitmap) DeleteObject(bitmap); DeleteDC(dc); return {}; }
    const auto previousBitmap = SelectObject(dc, bitmap);
    memset(bits, 0, size_t(w) * h * 4);
    SetBkMode(dc, TRANSPARENT);
    // The heading's size and the text's, the text as large as fits under it. Only the heading is letter-spaced:
    // DrawText breaks lines without the spacing, so spaced wrapped text would run past its box. The heading is one
    // short line, measured with the spacing and drawn with TextOut.
    RECT headingRect{0, 0, 0, 0};
    constexpr int kHeadingSpacing = 10;
    if (headingLength) {
        HGDIOBJ before = nullptr;
        if (HFONT font = SelectHudFont(dc, kHeadingPx, before)) {
            SetTextCharacterExtra(dc, kHeadingSpacing);
            SIZE size{};
            GetTextExtentPoint32W(dc, heading, int(headingLength), &size);
            headingRect = {0, 0, std::min<LONG>(size.cx, kMaxTextW), size.cy};
            SetTextCharacterExtra(dc, 0);
            SelectObject(dc, before); DeleteObject(font);
        } else headingLength = 0;
    }
    const int headingH = headingLength ? int(headingRect.bottom) : 0;
    const int maxBodyH = h - 2 * kMargin - 2 * kPadY - (headingLength ? headingH + kGap : 0);
    int bodyPx = 0;
    RECT bodyRect{0, 0, 0, 0};
    for (int px = 96; px >= kSmallestPx; px -= 6) {
        HGDIOBJ before = nullptr;
        HFONT font = SelectHudFont(dc, px, before);
        if (!font) break;
        bodyRect = {0, 0, kMaxTextW, 0};
        DrawTextW(dc, text, int(textLength), &bodyRect, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
        SelectObject(dc, before); DeleteObject(font);
        bodyPx = px;
        if (bodyRect.bottom <= maxBodyH) break;
    }
    if (!bodyPx) { SelectObject(dc, previousBitmap); DeleteObject(bitmap); DeleteDC(dc); return {}; }
    const int bodyW = std::min<int>(bodyRect.right, kMaxTextW), bodyH = std::min<int>(bodyRect.bottom, maxBodyH);
    const int contentW = std::max<int>(bodyW, headingRect.right);
    const int boxW = std::clamp(contentW + 2 * kPadX, 760, w - 2 * kMargin);
    const int boxH = std::min(h - 2 * kMargin, (headingLength ? headingH + kGap : 0) + bodyH + 2 * kPadY);
    const int boxL = (w - boxW) / 2, boxT = (h - boxH) / 2, boxR = boxL + boxW, boxB = boxT + boxH;
    // The heading in the green channel, the text in the red one: one picture gives both coverages.
    const int textTop = boxT + kPadY;
    if (headingLength) {
        HGDIOBJ before = nullptr;
        if (HFONT font = SelectHudFont(dc, kHeadingPx, before)) {
            SetTextCharacterExtra(dc, kHeadingSpacing);
            SetTextColor(dc, RGB(0, 255, 0));
            // Centred on its width less the spacing after the last letter.
            const int left = (w - int(headingRect.right) + kHeadingSpacing) / 2;
            RECT clip{boxL + kPadX, textTop, boxR - kPadX, textTop + headingH};
            ExtTextOutW(dc, std::max(left, int(clip.left)), textTop, ETO_CLIPPED, &clip, heading, UINT(headingLength),
                nullptr);
            SetTextCharacterExtra(dc, 0);
            SelectObject(dc, before); DeleteObject(font);
        }
    }
    {
        HGDIOBJ before = nullptr;
        if (HFONT font = SelectHudFont(dc, bodyPx, before)) {
            SetTextColor(dc, RGB(255, 0, 0));
            const int top = textTop + (headingLength ? headingH + kGap : 0);
            RECT r{boxL + kPadX, top, boxR - kPadX, top + bodyH};
            DrawTextW(dc, text, int(textLength), &r, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS);
            SelectObject(dc, before); DeleteObject(font);
        }
    }
    GdiFlush();
    const auto* pixels = static_cast<const uint8_t*>(bits);   // B, G, R, A
    std::vector<float> cover(size_t(w) * h), blur;
    for (int i = 0; i < w * h; ++i) cover[i] = std::max(pixels[4 * i + 1], pixels[4 * i + 2]) / 255.0f;
    BlurTwice(cover, blur, w, h, 10);
    // Halo 3's HUD blue (measured off the game's ammo bars and tracker), as the waypoint labels; amber for a warning.
    const bool warning = tone == NoticeTone::Warning;
    const float accent[3]{warning ? 1.00f : 0.47f, warning ? 0.66f : 0.82f, warning ? 0.18f : 1.00f};
    const float body[3]{warning ? 1.00f : 0.84f, warning ? 0.90f : 0.94f, warning ? 0.74f : 1.00f};
    const float glow[3]{warning ? 1.00f : 0.12f, warning ? 0.42f : 0.52f, warning ? 0.04f : 1.00f};
    const float back[3]{warning ? 0.08f : 0.01f, warning ? 0.04f : 0.05f, warning ? 0.01f : 0.10f};
    constexpr float kBackAlpha = 0.58f, kLineAlpha = 0.45f, kCornerAlpha = 0.95f, kOpacity = 0.96f;
    constexpr int kLine = 3, kCorner = 6, kCornerLong = 70, kCornerTall = 30;
    auto rgba = std::make_shared<std::vector<uint8_t>>(size_t(w) * h * 4);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const size_t i = size_t(y) * w + x;
        float col[3]{0, 0, 0}, a = 0;
        auto over = [&](const float* c, float alpha) {
            if (alpha <= 0) return;
            for (int k = 0; k < 3; ++k) col[k] = c[k] * alpha + col[k] * (1 - alpha);
            a = alpha + a * (1 - alpha);
        };
        const bool inBox = x >= boxL && x < boxR && y >= boxT && y < boxB;
        if (inBox) {
            // Darkest along the middle, a touch lighter toward the top and bottom edges.
            const float edge = std::fabs((y - boxT) / float(std::max(boxH - 1, 1)) - 0.5f) * 2.0f;
            over(back, kBackAlpha * (0.92f + 0.08f * edge));
            const bool topLine = y < boxT + kLine, bottomLine = y >= boxB - kLine;
            if (topLine || bottomLine) over(accent, kLineAlpha);
            const bool nearSide = x < boxL + kCornerLong || x >= boxR - kCornerLong;
            const bool nearEnd = y < boxT + kCornerTall || y >= boxB - kCornerTall;
            const bool cornerH = nearSide && (y < boxT + kCorner || y >= boxB - kCorner);
            const bool cornerV = nearEnd && (x < boxL + kCorner || x >= boxR - kCorner);
            if (cornerH || cornerV) over(accent, kCornerAlpha);
        }
        const float c = cover[i];
        const float g = std::min(blur[i] * 1.5f, 1.0f) * 0.5f * (1.0f - c);
        over(glow, g);
        over(accent, pixels[4 * i + 1] / 255.0f);
        over(body, pixels[4 * i + 2] / 255.0f);
        for (int k = 0; k < 3; ++k)
            (*rgba)[4 * i + k] = uint8_t(std::clamp(col[k] * kOpacity, 0.0f, 1.0f) * 255.0f + 0.5f);
        (*rgba)[4 * i + 3] = uint8_t(std::clamp(a * kOpacity, 0.0f, 1.0f) * 255.0f + 0.5f);
    }
    SelectObject(dc, previousBitmap); DeleteObject(bitmap); DeleteDC(dc);
    auto result = std::make_shared<NativeSubtitleImage>();
    result->expiresAtMs = expires; result->revision = ++g_revision;
    result->rgba = rgba;
    return result;
}

void NativeSubtitles_Capture(GameTitle title, uint32_t generation,
    const wchar_t* text, float seconds, bool theatre, uint8_t channel, bool refreshed) noexcept {
    const uint32_t duration = subtitles::DurationMs(seconds);
    if (!generation || !duration || title == GameTitle::None || channel>2) return;
    for (auto& slot : g_slots) {
        unsigned empty=0;
        if (!slot.state.compare_exchange_strong(empty,1,std::memory_order_acquire)) continue;
        auto& e=slot.event;
        e.title=title; e.generation=generation; e.theatre=theatre; e.channel=channel;
        // Renderer-fed captions renew every frame. Revoked native queues expire
        // promptly even when a cinematic is skipped without changing title.
        e.expires=GetTickCount64()+(refreshed?std::min(duration,250u):duration);
        e.sequence=g_sequence.fetch_add(1,std::memory_order_relaxed)+1;
        if (CopyText(text,e)) slot.state.store(2,std::memory_order_release);
        else slot.state.store(0,std::memory_order_release);
        return;
    }
    g_dropped.fetch_add(1,std::memory_order_relaxed);
}

std::shared_ptr<const NativeSubtitleImage> NativeSubtitles_Read() {
    return std::atomic_load_explicit(&g_image,std::memory_order_acquire);
}

void NativeNotice_Show(const wchar_t* text, float seconds) noexcept {
    NativeNotice_Show(nullptr, text, seconds, NoticeTone::Info);
}

uint64_t NativeNotice_Show(const wchar_t* heading, const wchar_t* text, float seconds, NoticeTone tone) noexcept {
    const uint32_t duration = subtitles::DurationMs(seconds);
    if (!text || !duration) return 0;
    // Copied before the lock, so the lock is held only for the swap. No allocation: any thread may call this.
    wchar_t nextText[kNoticeCapacity]{}, nextHeading[kNoticeHeadingCapacity]{};
    uint32_t length = 0, headingLength = 0;
    while (length + 1 < kNoticeCapacity && text[length]) { nextText[length] = text[length]; ++length; }
    while (heading && headingLength + 1 < kNoticeHeadingCapacity && heading[headingLength]) {
        nextHeading[headingLength] = heading[headingLength]; ++headingLength;
    }
    AcquireSRWLockExclusive(&g_noticeLock);
    const bool same = length == g_noticeLength && headingLength == g_noticeHeadingLength && tone == g_noticeTone &&
        !memcmp(nextText, g_noticeText, length * sizeof(wchar_t)) &&
        !memcmp(nextHeading, g_noticeHeading, headingLength * sizeof(wchar_t));
    if (!same) {
        memcpy(g_noticeText, nextText, sizeof(nextText));
        memcpy(g_noticeHeading, nextHeading, sizeof(nextHeading));
        g_noticeLength = length;
        g_noticeHeadingLength = headingLength;
        g_noticeTone = tone;
        ++g_noticeSerial;
    }
    // The same notice again only stays longer: the worker keeps its picture and moves its expiry.
    g_noticeExpires = GetTickCount64() + duration;
    const uint64_t serial = g_noticeSerial;
    ReleaseSRWLockExclusive(&g_noticeLock);
    return serial;
}

void NativeNotice_Hide(uint64_t notice) noexcept {
    if (!notice) return;
    AcquireSRWLockExclusive(&g_noticeLock);
    if (g_noticeSerial == notice) g_noticeExpires = 0;
    ReleaseSRWLockExclusive(&g_noticeLock);
}

std::shared_ptr<const NativeSubtitleImage> NativeNotice_Read() {
    return std::atomic_load_explicit(&g_noticeImage,std::memory_order_acquire);
}

void NativeLabel_Set(int slot, const wchar_t* text, float opacity) noexcept {
    if (slot < 0 || slot >= kNativeLabelSlots) return;
    AcquireSRWLockExclusive(&g_labelLock);
    Label& l = g_labels[slot];
    wchar_t next[_countof(l.text)]{};
    uint32_t length = 0;
    while (text && length + 1 < _countof(next) && text[length]) { next[length] = text[length]; ++length; }
    const bool changed = length != l.length || memcmp(next, l.text, length * sizeof(wchar_t)) ||
        std::fabs(opacity - l.opacity) > 0.004f;
    if (changed) {
        memcpy(l.text, next, sizeof(next));
        l.length = length;
        l.opacity = opacity;
        ++l.serial;
    }
    ReleaseSRWLockExclusive(&g_labelLock);
}

std::shared_ptr<const NativeSubtitleImage> NativeLabel_Read(int slot) {
    if (slot < 0 || slot >= kNativeLabelSlots) return {};
    return std::atomic_load_explicit(&g_labels[slot].image,std::memory_order_acquire);
}

void NativeSubtitles_SetReachSources(uint32_t generation,
    uintptr_t gameplayReturn,uintptr_t theatreReturn) noexcept {
    const SubtitleSource sources[]={{gameplayReturn,0,false},{theatreReturn,0,true}};
    SetSubtitleSources(GameTitle::HaloReach,generation,sources,_countof(sources));
}

void NativeSubtitles_Poll(bool levelRunning) {
    InstallHostObserver();
    PollNativeSubtitleSources(levelRunning);
    const auto title=TitleAdapter_GetActiveTitle();
    const auto generation=TitleAdapter_GetGeneration(title);
    const auto now=GetTickCount64();
    const bool theatre=VR_IsCutsceneTheaterActive();
    Event pending[_countof(g_slots)]{};size_t count=0;
    for(auto& slot:g_slots) {
        unsigned ready=2;
        if(!slot.state.compare_exchange_strong(ready,3,std::memory_order_acquire)) continue;
        if(slot.event.sequence>g_consumed) pending[count++]=slot.event;
        slot.state.store(0,std::memory_order_release);
    }
    std::sort(pending,pending+count,[](const Event& a,const Event& b){return a.sequence<b.sequence;});
    for(size_t i=0;i<count;++i) {
        g_consumed=std::max(g_consumed,pending[i].sequence);
        if(subtitles::Current(pending[i].title,pending[i].generation,pending[i].expires,title,generation,now)&&
            pending[i].theatre==theatre) AcceptSubtitleEvent(pending[i]);
    }
    RasterCaption caption{};
    auto current=NativeSubtitles_Read();
    if(!levelRunning||!ComposeSubtitleCaption(title,generation,theatre,now,caption)) {
        if(!levelRunning) {g_captionLines[0]={};g_captionLines[1]={};g_captionPair=false;}
        std::atomic_store_explicit(&g_image,std::shared_ptr<const NativeSubtitleImage>{},std::memory_order_release);
    } else {
        std::shared_ptr<const NativeSubtitleImage> image;
        if(current&&current->title==title&&current->generation==generation&&current->theatre==theatre&&
            g_lastRasterized.length==caption.length&&
            !memcmp(g_lastRasterized.text,caption.text,caption.length*sizeof(wchar_t))) {
            auto refreshed=std::make_shared<NativeSubtitleImage>(*current);
            refreshed->expiresAtMs=caption.expires;image=refreshed;
        } else {
            image=Rasterize(caption);
            if(image) g_lastRasterized=caption;
        }
        if(image) std::atomic_store_explicit(&g_image,image,std::memory_order_release);
    }
    // The notice: rasterized when its text changes, its expiry moved when the same one is shown again, dropped when
    // it has expired.
    {
        RasterCaption notice{};
        wchar_t heading[kNoticeHeadingCapacity]{};
        uint32_t headingLength = 0;
        NoticeTone tone = NoticeTone::Info;
        uint64_t serial = 0;
        AcquireSRWLockShared(&g_noticeLock);
        serial = g_noticeSerial;
        notice.length = g_noticeLength;
        notice.expires = g_noticeExpires;
        memcpy(notice.text, g_noticeText, sizeof(wchar_t) * (g_noticeLength + 1));
        memcpy(heading, g_noticeHeading, sizeof(heading));
        headingLength = g_noticeHeadingLength;
        tone = g_noticeTone;
        ReleaseSRWLockShared(&g_noticeLock);
        const auto shown = NativeNotice_Read();
        if (!notice.length || now >= notice.expires) {
            if (shown)
                std::atomic_store_explicit(&g_noticeImage,std::shared_ptr<const NativeSubtitleImage>{},
                    std::memory_order_release);
            g_noticeRasterized = 0;   // the same notice shown again later is drawn again
        } else if (serial != g_noticeRasterized) {
            g_noticeRasterized = serial;
            if (auto image = RasterizeNotice(heading, headingLength, notice.text, notice.length, tone, notice.expires))
                std::atomic_store_explicit(&g_noticeImage,
                    std::shared_ptr<const NativeSubtitleImage>(std::move(image)),std::memory_order_release);
        } else if (shown && shown->expiresAtMs != notice.expires) {
            auto longer = std::make_shared<NativeSubtitleImage>(*shown);   // the same pixels, shared
            longer->expiresAtMs = notice.expires;
            std::atomic_store_explicit(&g_noticeImage,
                std::shared_ptr<const NativeSubtitleImage>(std::move(longer)),std::memory_order_release);
        }
    }
    // The labels: each rasterized when its text or opacity changes.
    for (Label& l : g_labels) {
        RasterCaption label{};
        uint64_t serial = 0;
        float opacity = 1.0f;
        AcquireSRWLockShared(&g_labelLock);
        serial = l.serial;
        label.length = l.length;
        opacity = l.opacity;
        memcpy(label.text, l.text, sizeof(wchar_t) * (l.length + 1));
        ReleaseSRWLockShared(&g_labelLock);
        if (serial != l.rasterized) {
            l.rasterized = serial;
            label.expires = UINT64_MAX;
            auto image = label.length ? RasterizeLabel(label, opacity) : std::shared_ptr<const NativeSubtitleImage>{};
            std::atomic_store_explicit(&l.image, std::move(image), std::memory_order_release);
        }
    }
    const auto dropped=g_dropped.exchange(0,std::memory_order_relaxed);
    if (dropped) LOG("Native subtitles: bounded queue dropped %llu captions; native UI unchanged",
        static_cast<unsigned long long>(dropped));
}
