// The offline check of src/common/reclaimer_client_finds.h against a real Project Reclaimer client executable: the
// executable on disk is laid out by its sections, its .pdata gives the function extents, and every find the DLL makes
// in the client runs exactly as it runs in memory. A new Reclaimer release can be checked here before anyone plays it.
//
// The executable is HALOMCCVR_RECLAIMER_CLIENT_EXE (its path is in the mod folder's reclaimer_client.txt; 0.10.4 ships
// as project-reclaimer-client-v0.9.23.exe). Without it the check exits 77, which ctest reports as skipped. A find
// that fails exits 1, naming what was not found: that hook would stay off in the game.

#include "../src/common/reclaimer_client_finds.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
template <class T> T Load(const std::vector<uint8_t>& data, size_t at)
{
    T value{};
    if (at + sizeof(T) <= data.size())
        std::memcpy(&value, data.data() + at, sizeof(T));
    return value;
}

struct Mapped
{
    std::vector<uint8_t> image;
    std::vector<std::pair<uint32_t, uint32_t>> functions;   // .pdata, sorted by start
    uint32_t codeRva = 0, codeSize = 0;
};

bool Extent(const void* context, uint32_t rva, uint32_t& begin, uint32_t& end)
{
    const auto& functions = static_cast<const Mapped*>(context)->functions;
    auto it = std::upper_bound(functions.begin(), functions.end(), std::make_pair(rva, UINT32_MAX));
    if (it == functions.begin())
        return false;
    --it;
    if (rva >= it->second)
        return false;
    begin = it->first;
    end = it->second;
    return true;
}

// The PE32+ file laid out by RVA. False when it is not one.
bool Map(const std::vector<uint8_t>& file, Mapped& out)
{
    if (file.size() < 0x40 || file[0] != 'M' || file[1] != 'Z')
        return false;
    const uint32_t nt = Load<uint32_t>(file, 0x3C);
    if (Load<uint32_t>(file, nt) != 0x00004550 || Load<uint16_t>(file, nt + 0x18) != 0x20B)
        return false;
    const uint16_t sections = Load<uint16_t>(file, nt + 6);
    const uint16_t optionalSize = Load<uint16_t>(file, nt + 0x14);
    const size_t optional = nt + 0x18;
    const uint32_t imageSize = Load<uint32_t>(file, optional + 0x38);
    const uint32_t headersSize = Load<uint32_t>(file, optional + 0x3C);
    const uint32_t pdataRva = Load<uint32_t>(file, optional + 0x70 + 3 * 8);   // data directory 3: exceptions
    const uint32_t pdataSize = Load<uint32_t>(file, optional + 0x70 + 3 * 8 + 4);
    if (!imageSize || imageSize > (1u << 30))
        return false;
    out.image.assign(imageSize, 0);
    std::memcpy(out.image.data(), file.data(), std::min<size_t>({headersSize, file.size(), imageSize}));
    const size_t table = optional + optionalSize;
    for (uint16_t i = 0; i < sections; ++i)
    {
        const size_t s = table + i * 40;
        char name[9]{};
        std::memcpy(name, file.data() + s, 8);
        const uint32_t virtualSize = Load<uint32_t>(file, s + 8), rva = Load<uint32_t>(file, s + 12);
        const uint32_t rawSize = Load<uint32_t>(file, s + 16), raw = Load<uint32_t>(file, s + 20);
        if (rva >= imageSize)
            continue;
        const size_t count = std::min<size_t>({rawSize, virtualSize ? virtualSize : rawSize, imageSize - rva,
                                                raw < file.size() ? file.size() - raw : 0});
        std::memcpy(out.image.data() + rva, file.data() + raw, count);
        if (std::string(name) == ".text")
        {
            out.codeRva = rva;
            out.codeSize = std::min(virtualSize, imageSize - rva);
        }
    }
    for (uint32_t at = pdataRva; pdataSize && at + 12 <= pdataRva + pdataSize && at + 12 <= imageSize; at += 12)
    {
        uint32_t begin = 0, end = 0;
        std::memcpy(&begin, out.image.data() + at, 4);
        std::memcpy(&end, out.image.data() + at + 4, 4);
        if (begin < end && end <= imageSize)
            out.functions.push_back({begin, end});
    }
    std::sort(out.functions.begin(), out.functions.end());
    return out.codeRva && !out.functions.empty();
}
} // namespace

int main()
{
    const char* path = std::getenv("HALOMCCVR_RECLAIMER_CLIENT_EXE");
    FILE* f = nullptr;
    if (!path || fopen_s(&f, path, "rb") != 0 || !f)
    {
        std::printf("no Project Reclaimer client (HALOMCCVR_RECLAIMER_CLIENT_EXE): skipped\n");
        return 77;
    }
    std::vector<uint8_t> file;
    uint8_t buffer[1 << 16];
    size_t n = 0;
    while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0)
        file.insert(file.end(), buffer, buffer + n);
    fclose(f);

    Mapped mapped;
    if (!Map(file, mapped))
    {
        std::printf("%s: not a PE32+ executable with a code section and .pdata\n", path);
        return 1;
    }
    reclaimer_client::Image image;
    image.base = mapped.image.data();
    image.size = mapped.image.size();
    image.codeRva = mapped.codeRva;
    image.codeSize = mapped.codeSize;
    image.extent = &Extent;
    image.context = &mapped;

    int failures = 0;
    const auto report = [&](const char* what, uint32_t rva, const char* missing) {
        if (missing)
        {
            ++failures;
            std::printf("%-22s NOT FOUND: %s\n", what, missing);
        }
        else
            std::printf("%-22s exe+0x%X\n", what, rva);
    };
    const reclaimer_client::Find camera = reclaimer_client::FindVisrCamera(image);
    report("VISR camera", camera.function, camera.missing);
    const reclaimer_client::Find constants = reclaimer_client::FindVisrConstants(image);
    report("VISR constants", constants.function, constants.missing);
    if (!camera.missing && !constants.missing)
        report("VISR finds agree", camera.function,
               reclaimer_client::VisrAgree(image, camera.function, constants.function));
    const reclaimer_client::NavpointDraw draw = reclaimer_client::FindNavpointDraw(image);
    report("navpoint version", draw.version, draw.missing);
    report("navpoint draw", draw.draw, draw.missing);
    report("navpoint count read", draw.countDisp, draw.missing);
    std::printf("%s: %d find(s) failed\n", path, failures);
    return failures ? 1 : 0;
}
