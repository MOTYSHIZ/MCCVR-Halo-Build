#pragma once

// The Project Reclaimer client executable's image in memory, as src/common/reclaimer_client_finds.h reads it: laid
// out by RVA, its code section, and its function extents from the loader's .pdata (RtlLookupFunctionEntry).

#include <windows.h>

#include <cstring>

#include "../common/reclaimer_client_finds.h"

namespace reclaimer_client
{
inline bool ExtentInMemory(const void* context, uint32_t rva, uint32_t& begin, uint32_t& end)
{
    const auto base = reinterpret_cast<uintptr_t>(context);
    DWORD64 image = 0;
    const PRUNTIME_FUNCTION entry = RtlLookupFunctionEntry(base + rva, &image, nullptr);
    if (!entry || image != base)
        return false;
    begin = entry->BeginAddress;
    end = entry->EndAddress;
    return true;
}

// False when the executable's headers do not read as a PE32+ image with a code section.
inline bool ClientImageInMemory(Image& out)
{
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return false;
    out = Image{};
    out.base = reinterpret_cast<const uint8_t*>(base);
    out.size = nt->OptionalHeader.SizeOfImage;
    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
        if (std::memcmp(section->Name, ".text", 6) == 0)
        {
            out.codeRva = section->VirtualAddress;
            out.codeSize = section->Misc.VirtualSize;
        }
    out.extent = &ExtentInMemory;
    out.context = reinterpret_cast<const void*>(base);
    return out.codeRva != 0;
}
} // namespace reclaimer_client
