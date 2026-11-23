// OFFLINE CHECK: ODST's prompt and map state (src/common/odst_prompt_resolve.h) resolved on the game DLLs on disk, with
// the very code the DLL runs on the loaded module. For a game update: run it against the new halo3odst.dll and read
// what moved, what no longer matches and what no longer agrees.
//
//   halomccvr_odst_prompt_resolve_check [--mcc <MCC folder>] [--dll <path to halo3odst.dll>] [--halo3 <path>]
//
// The MCC folder comes from --mcc or the HALOMCCVR_MCC_DIR environment variable; with neither (and no --dll) the check
// is SKIPPED (exit 77). ODST's halo3odst.dll must resolve whole. Halo 3's halo3.dll (also what Project Reclaimer runs
// ODST's levels on) must resolve NOTHING: it has no such prompt or map state, and the rows must stay off there.
//
// The same run checks ODST's tutorial text find (src/common/odst_prompt_text.h): the chud's text setter and
// chud_show_cinematic_title's call of it must agree in halo3odst.dll, and must not be found in halo3.dll.
#include "../src/common/odst_prompt_resolve.h"
#include "../src/common/odst_prompt_text.h"
#include "../src/common/odst_training_reset.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
constexpr int kExitSkipped = 77;

// A PE32+ file laid out as the loader maps it: headers and each section at its RVA (no relocations: nothing here reads
// an absolute address), and the TLS index's RVA from the PE TLS directory (its address less the preferred base).
struct Laid
{
    std::vector<uint8_t> bytes;
    drive_resolve::Image image{};
};

bool LayOut(const std::wstring& path, Laid& out, std::string& why)
{
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        why = "the file could not be opened";
        return false;
    }
    LARGE_INTEGER size{};
    std::vector<uint8_t> raw;
    bool read = GetFileSizeEx(file, &size) && size.QuadPart > 0x400 && size.QuadPart < 0x40000000;
    if (read)
    {
        raw.resize(static_cast<size_t>(size.QuadPart));
        size_t done = 0;
        while (read && done < raw.size())
        {
            DWORD got = 0;
            const DWORD want = static_cast<DWORD>((std::min)(raw.size() - done, static_cast<size_t>(1u << 24)));
            read = ReadFile(file, raw.data() + done, want, &got, nullptr) && got;
            done += got;
        }
    }
    CloseHandle(file);
    if (!read)
    {
        why = "the file could not be read";
        return false;
    }
    const auto u16 = [&raw](size_t at) { uint16_t v = 0; std::memcpy(&v, raw.data() + at, 2); return v; };
    const auto u32 = [&raw](size_t at) { uint32_t v = 0; std::memcpy(&v, raw.data() + at, 4); return v; };
    const auto u64 = [&raw](size_t at) { uint64_t v = 0; std::memcpy(&v, raw.data() + at, 8); return v; };
    const size_t pe = u32(0x3C);
    if (u16(0) != 0x5A4D || pe + 0x108 > raw.size() || u32(pe) != 0x00004550u || u16(pe + 24) != 0x20B)
    {
        why = "not an x64 PE32+ file";
        return false;
    }
    const size_t optional = pe + 24, table = optional + u16(pe + 20);
    const uint32_t sections = u16(pe + 6), sizeOfImage = u32(optional + 56), sizeOfHeaders = u32(optional + 60);
    if (!sizeOfImage || sizeOfHeaders > raw.size() || sizeOfHeaders > sizeOfImage ||
        table + static_cast<size_t>(sections) * 40 > raw.size())
    {
        why = "its headers do not fit the file";
        return false;
    }
    out.bytes.assign(sizeOfImage, 0);
    std::memcpy(out.bytes.data(), raw.data(), sizeOfHeaders);
    for (uint32_t i = 0; i < sections; ++i)
    {
        const size_t at = table + static_cast<size_t>(i) * 40;
        const uint32_t virtualSize = u32(at + 8), rva = u32(at + 12), rawSize = u32(at + 16), rawAt = u32(at + 20);
        const uint32_t copy = virtualSize ? (std::min)(virtualSize, rawSize) : rawSize;
        if (!copy)
            continue;
        if (rva > sizeOfImage || copy > sizeOfImage - rva || rawAt > raw.size() || copy > raw.size() - rawAt)
        {
            why = "a section does not fit the file or the image";
            return false;
        }
        std::memcpy(out.bytes.data() + rva, raw.data() + rawAt, copy);
    }
    const char* refused = nullptr;
    if (!drive_resolve::ReadImage(out.bytes.data(), out.bytes.size(), out.image, refused))
    {
        why = refused ? refused : "its image did not read";
        return false;
    }
    const uint32_t tlsRva = u32(optional + 112 + 9 * 8);
    if (tlsRva && tlsRva + 0x28 <= sizeOfImage)
    {
        uint64_t indexAddress = 0;
        std::memcpy(&indexAddress, out.bytes.data() + tlsRva + 0x10, 8);   // IMAGE_TLS_DIRECTORY64::AddressOfIndex
        const uint64_t rva = indexAddress - u64(optional + 24);
        if (indexAddress && rva + 4 <= sizeOfImage)
            out.image.tlsIndexRva = static_cast<uint32_t>(rva);
    }
    return true;
}

std::wstring Widen(const char* text)
{
    std::wstring wide;
    for (const char* p = text; p && *p; ++p)
        wide += static_cast<wchar_t>(static_cast<unsigned char>(*p));
    return wide;
}

void Print(const char* name, const odst_prompt_resolve::Result& r)
{
    if (r.ok)
        std::printf("  %s: RESOLVED. accept read returns to +0x%X (control builder +0x%X, reader +0x%X); engine TLS index "
                    "+0x%X, records at TLS +0x%X, %u bytes each; prompt bytes +0x%X (confirm) and +0x%X (cancel); "
                    "the map is test bit %u\n",
                    name, r.acceptReadReturn, r.builderStart, r.readerRva, r.tlsIndexRva, r.block, r.stride,
                    r.confirmFlag, r.cancelFlag, r.pdaBit);
    else
        std::printf("  %s: NOT RESOLVED, the ODST prompt and map rows stay off: %s\n", name, r.missing);
}
void PrintText(const char* name, const odst_prompt_text::Where& w)
{
    if (w.ok)
        std::printf("  %s: tutorial text RESOLVED. the chud's text setter +0x%X, called by chud_show_cinematic_title at "
                    "+0x%X\n", name, w.setterRva, w.titleCallRva);
    else
        std::printf("  %s: tutorial text NOT RESOLVED, the game's own words stay: %s\n", name, w.missing);
}
} // namespace

int main(int argc, char** argv)
{
    std::wstring mcc, odst, halo3;
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--mcc")) mcc = Widen(argv[++i]);
        else if (!std::strcmp(argv[i], "--dll")) odst = Widen(argv[++i]);
        else if (!std::strcmp(argv[i], "--halo3")) halo3 = Widen(argv[++i]);
    }
    if (mcc.empty())
    {
        char env[1024]{};
        const DWORD n = GetEnvironmentVariableA("HALOMCCVR_MCC_DIR", env, sizeof(env));
        if (n && n < sizeof(env)) mcc = Widen(env);
    }
    if (!mcc.empty())
    {
        if (odst.empty()) odst = mcc + L"\\halo3odst\\halo3odst.dll";
        if (halo3.empty()) halo3 = mcc + L"\\halo3\\halo3.dll";
    }
    if (odst.empty())
    {
        std::printf("ODST prompt and map resolution check: SKIPPED (no MCC folder: pass --mcc <folder> or --dll <path>, "
                    "or set HALOMCCVR_MCC_DIR)\n");
        return kExitSkipped;
    }
    int failures = 0;
    std::printf("ODST prompt and map resolution check (odst_prompt_resolve.h)\n");
    {
        Laid laid;
        std::string why;
        if (!LayOut(odst, laid, why))
        {
            std::printf("  halo3odst.dll: could not be laid out (%s)\n", why.c_str());
            return GetFileAttributesW(odst.c_str()) == INVALID_FILE_ATTRIBUTES ? kExitSkipped : 1;
        }
        // The reader is checked by the DLL against the one it hooked; here the resolution names it.
        const auto r = odst_prompt_resolve::Resolve(laid.image, 0);
        Print("halo3odst.dll", r);
        failures += r.ok ? 0 : 1;
        const auto text = odst_prompt_text::Resolve(laid.image);
        PrintText("halo3odst.dll", text);
        failures += text.ok ? 0 : 1;
        const auto hud = odst_prompt_text::ResolveHudMessage(laid.image);
        if (hud.rva)
            std::printf("  halo3odst.dll: HUD prompts RESOLVED. the HUD message text +0x%X\n", hud.rva);
        else
            std::printf("  halo3odst.dll: HUD prompts NOT RESOLVED, the game's own words stay: %s\n", hud.missing);
        failures += hud.rva ? 0 : 1;
        const auto gp = odst_training::Resolve(laid.image);
        if (gp.ok)
            std::printf("  halo3odst.dll: training reset RESOLVED. gp_boolean_get +0x%X, its lookup +0x%X, the "
                        "progression tag's getter +0x%X\n", gp.getRva, gp.lookupRva, gp.tagRva);
        else
            std::printf("  halo3odst.dll: training reset NOT RESOLVED: %s\n", gp.missing);
        failures += gp.ok ? 0 : 1;
    }
    if (!halo3.empty() && GetFileAttributesW(halo3.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        Laid laid;
        std::string why;
        if (!LayOut(halo3, laid, why))
            std::printf("  halo3.dll: could not be laid out (%s)\n", why.c_str());
        else
        {
            // Halo 3's engine (and Project Reclaimer's ODST levels on it) has none of this: it must resolve nothing.
            const auto r = odst_prompt_resolve::Resolve(laid.image, 0);
            Print("halo3.dll (Halo 3; Project Reclaimer's ODST)", r);
            if (r.ok)
            {
                std::printf("  FAIL: Halo 3's engine must not resolve ODST's prompt and map state\n");
                ++failures;
            }
            const auto text = odst_prompt_text::Resolve(laid.image);
            PrintText("halo3.dll", text);
            if (text.ok)
            {
                std::printf("  FAIL: Halo 3's engine must not resolve ODST's tutorial text setter\n");
                ++failures;
            }
        }
    }
    std::printf("%s\n", failures ? "FAILED" : "PASSED");
    return failures ? 1 : 0;
}
