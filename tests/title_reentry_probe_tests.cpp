#include <windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cstdlib>

// Compile the production resolver. Only loader lookup is replaced: fixtures
// still exercise real committed ranges, guard protection, PE/anchor reads and
// SEH. No title game code is executed or patched by this test.
static std::array<HMODULE, 6> fixtureModules{};
static HMODULE FixtureGetModuleHandleW(LPCWSTR name)
{
    constexpr const wchar_t* names[] = {
        L"halo3.dll", L"halo3odst.dll", L"haloreach.dll", L"halo4.dll",
        L"halo1.dll", L"halo2.dll"};
    for (size_t i = 0; i < std::size(names); ++i)
        if (std::wcscmp(name, names[i]) == 0) return fixtureModules[i];
    return nullptr;
}
#define GetModuleHandleW FixtureGetModuleHandleW
#include "../src/dll/title_reentry_probe.cpp"
#undef GetModuleHandleW

namespace
{
unsigned checks = 0;
void Check(bool passed, const char* name)
{
    ++checks;
    if (!passed) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}
struct Allocation
{
    uint8_t* bytes;
    explicit Allocation(size_t size) : bytes(static_cast<uint8_t*>(
        VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)))
    { Check(bytes != nullptr, "fixture allocation"); }
    ~Allocation() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
    uintptr_t Base() const { return reinterpret_cast<uintptr_t>(bytes); }
};

struct Fixture
{
    Allocation ce{halo_ce::contract::imageSize};
    Allocation h3{0x02D33000};
    Allocation clock{4096};
    Allocation replacementClock{4096};
    TitleRuntimeModuleSet modules{};
    Fixture()
    {
        fixtureModules[0] = reinterpret_cast<HMODULE>(h3.bytes);
        fixtureModules[4] = reinterpret_cast<HMODULE>(ce.bytes);
        modules.moduleBases[0] = h3.Base();
        modules.moduleBases[4] = ce.Base();
        modules.availabilityMask = TitleRuntimeAvailabilityBit(GameTitle::Halo3) |
            TitleRuntimeAvailabilityBit(GameTitle::HaloCE);
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(ce.bytes);
        dos->e_magic = IMAGE_DOS_SIGNATURE;
        dos->e_lfanew = 0x80;
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(ce.bytes + dos->e_lfanew);
        nt->Signature = IMAGE_NT_SIGNATURE;
        nt->FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
        nt->FileHeader.TimeDateStamp = halo_ce::contract::timestamp;
        nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
        nt->OptionalHeader.SizeOfImage = halo_ce::contract::imageSize;
        auto* target = ce.bytes + kCeClockAnchor.rva;
        const char* pattern = kCeClockAnchor.pattern;
        while (*pattern)
        {
            const auto hex = [](char c) { return c <= '9' ? c - '0' : c - 'A' + 10; };
            *target++ = pattern[0] == '?' ? 0 :
                static_cast<uint8_t>((hex(pattern[0]) << 4) | hex(pattern[1]));
            pattern += 2;
            if (*pattern == ' ') ++pattern;
        }
        *reinterpret_cast<uintptr_t*>(ce.bytes + kCeClockSlot) = clock.Base();
        Reset();
    }
    void Reset()
    {
        g_activity = {};
        g_presentHint.store(GameTitle::None);
        g_presentHintMs.store(0);
        clock.bytes[0] = 1;
        Tick(1);
    }
    void Tick(int32_t tick) { std::memcpy(clock.bytes + 0xC, &tick, sizeof(tick)); }
    GameTitle Resolve(uint64_t now, GameTitle retained = GameTitle::Halo3)
    { return TitleReentryProbe_Resolve(modules, now, retained); }
    void StillThenTick()
    {
        Check(Resolve(1000) == GameTitle::Halo3, "first clock sample keeps prior adapter");
        Check(Resolve(1050) == GameTitle::Halo3, "frozen CE keeps prior adapter");
        Tick(2);
    }
};
}

int main()
{
    Fixture f;
    f.StillThenTick();
    Check(f.Resolve(1100) == GameTitle::HaloCE,
          "retained Halo 3 plus newly ticking CE selects CE without Present hint");
    Check(f.Resolve(1150) == GameTitle::Halo3,
          "frozen CE does not supply new activity");

    f.Reset();
    f.clock.bytes[0] = 0;
    Check(f.Resolve(1000) == GameTitle::Halo3, "uninitialized CE does not select");
    f.clock.bytes[0] = 1; f.Tick(1);
    Check(f.Resolve(1050) == GameTitle::HaloCE,
          "native initialized transition establishes fresh CE activity");

    f.Reset();
    Check(f.Resolve(1000) == GameTitle::Halo3, "already running first sample");
    for (int i = 1; i < kAlreadyRunningSamples; ++i)
    {
        f.Tick(i + 1);
        Check(f.Resolve(1000 + i * 50) == GameTitle::Halo3,
              "already running CE waits for complete sustained evidence");
    }
    f.Tick(kAlreadyRunningSamples + 1);
    Check(f.Resolve(1000 + kAlreadyRunningSamples * 50) == GameTitle::HaloCE,
          "already running CE selected after full sustained activity");

    f.Reset(); f.StillThenTick();
    Check(f.Resolve(2000) == GameTitle::Halo3, "stale sample gap resets CE evidence");
    f.Reset(); f.StillThenTick();
    Check(f.Resolve(900) == GameTitle::Halo3, "backward time resets CE evidence");

    f.Reset(); f.StillThenTick();
    fixtureModules[4] = nullptr;
    Check(f.Resolve(1100) == GameTitle::Halo3, "foreign/unloaded CE mapping rejected");
    fixtureModules[4] = reinterpret_cast<HMODULE>(f.ce.bytes);
    Check(f.Resolve(1150) == GameTitle::Halo3, "mapping loss clears prior activity");

    f.Reset(); f.StillThenTick();
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(f.ce.bytes + 0x80);
    nt->FileHeader.TimeDateStamp ^= 1;
    Check(f.Resolve(1100) == GameTitle::Halo3, "foreign CE PE identity rejected");
    nt->FileHeader.TimeDateStamp ^= 1;
    Check(f.Resolve(1150) == GameTitle::Halo3, "identity failure clears prior activity");
    f.Reset(); f.StillThenTick();
    f.ce.bytes[kCeClockAnchor.rva + 9] ^= 1;
    Check(f.Resolve(1100) == GameTitle::Halo3, "modified clock RIP operand rejected");
    f.ce.bytes[kCeClockAnchor.rva + 9] ^= 1;

    f.Reset(); f.StillThenTick();
    f.clock.bytes[0] = 2;
    Check(f.Resolve(1100) == GameTitle::Halo3, "invalid initialized boolean rejected");
    f.clock.bytes[0] = 1; f.Tick(-1);
    Check(f.Resolve(1150) == GameTitle::Halo3, "negative native tick rejected");
    f.Tick(3);
    Check(f.Resolve(1200) == GameTitle::Halo3, "invalid clock clears prior activity");

    f.Reset(); f.StillThenTick();
    DWORD oldProtection = 0;
    Check(VirtualProtect(f.clock.bytes, 4096, PAGE_NOACCESS, &oldProtection) != 0,
          "protect unreadable clock");
    Check(f.Resolve(1100) == GameTitle::Halo3, "inaccessible clock rejected");
    DWORD ignored = 0;
    Check(VirtualProtect(f.clock.bytes, 4096, oldProtection, &ignored) != 0,
          "restore clock access");
    Check(f.Resolve(1150) == GameTitle::Halo3, "unreadable clock clears activity");

    f.Reset(); f.StillThenTick();
    *reinterpret_cast<uintptr_t*>(f.ce.bytes + kCeClockSlot) = f.clock.Base() + 4090;
    Check(f.Resolve(1100) == GameTitle::Halo3, "clock crossing committed region rejected");
    *reinterpret_cast<uintptr_t*>(f.ce.bytes + kCeClockSlot) = f.clock.Base();

    f.Reset(); f.StillThenTick();
    f.replacementClock.bytes[0] = 1;
    int32_t replacementTick = 4;
    std::memcpy(f.replacementClock.bytes + 0xC, &replacementTick, 4);
    *reinterpret_cast<uintptr_t*>(f.ce.bytes + kCeClockSlot) = f.replacementClock.Base();
    Check(f.Resolve(1100) == GameTitle::Halo3, "replacement clock cannot inherit stillness");
    *reinterpret_cast<uintptr_t*>(f.ce.bytes + kCeClockSlot) = f.clock.Base();

    f.Reset(); f.StillThenTick();
    ++f.h3.bytes[0x02D2F680];
    Check(f.Resolve(1100, GameTitle::None) == GameTitle::None,
          "two changing engines do not select a new adapter");
    f.Tick(3); ++f.h3.bytes[0x02D2F680];
    Check(f.Resolve(1150) == GameTitle::Halo3,
          "ambiguous activity retains the previously selected adapter only");

    f.Reset();
    g_presentHint.store(GameTitle::HaloCE);
    g_presentHintMs.store(1000);
    Check(f.Resolve(1250) == GameTitle::HaloCE, "fresh direct CE Present hint remains supported");
    Check(f.Resolve(1251) == GameTitle::Halo3, "expired Present hint cannot select CE");
    g_presentHintMs.store(2000);
    Check(f.Resolve(1300) == GameTitle::Halo3, "future Present hint cannot select CE");
    std::printf("PASS: %u production title reentry checks\n", checks);
}
