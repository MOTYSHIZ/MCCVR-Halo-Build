// Included inside game.cpp's Halo 4 implementation after its identity reader.
// No new hook, engine call, process write or geometry scan is introduced.
bool Halo4ProveRuntimeWeaponBounds(uintptr_t base, size_t size)
{
    struct Window { uint32_t rva; const char* pattern; };
    const Window windows[]{
        // Tag lookup -> render_model root +0x64, expressed in packed dwords.
        {0x344579, "44 0F B7 11 46 8B 5C D0 04 41 8B C3 41 8B DB C1 E8 1C "
            "49 8B 3C C4 33 C0 42 39 44 9F 48 0F 8E AB 03 00 00 "
            "4D 8D 6B 19 33 C9 4E 8D 2C AF"},
        // geometry +0x20 packed compression address -> native shader decoder.
        {0x344651, "48 8B 44 24 28 8B 48 20 8B 50 20 48 8D 05 1D 5B 62 04 "
            "C1 E9 1C 48 8B 0C C8 48 8D 0C 91 E8 45 34 04 00"},
        // Interleaved XYZ min/max consumed as bias and extent by the renderer.
        {0x387AF7, "F3 0F 10 53 18 F3 0F 10 4B 10 F3 0F 10 43 08 "
            "F3 0F 5C 4B 0C F3 0F 5C 43 04 F3 0F 5C 53 14 "
            "C7 40 0C 00 00 80 3F"},
    };
    for (const auto& window : windows)
    {
        const uintptr_t found=sig::Find(base,size,window.pattern);
        if (!found || found-base!=window.rva ||
            sig::Find(found+1,base+size-found-1,window.pattern)) return false;
    }
    return true;
}

bool Halo4ReadRuntimeWeaponBounds(uintptr_t descriptor, uint32_t checksum,
    Halo4WeaponCollisionBounds& output)
{
    if (!descriptor || !g_halo4RuntimeWeaponBoundsProven.load(
            std::memory_order_acquire) || !g_halo4RenderModelGroupBaseTable ||
        descriptor>(std::numeric_limits<uintptr_t>::max)()-0x88u) return false;
    struct Header { int32_t count; uint32_t packed; };
    Header header{};
    if (!Halo4SafeRead(reinterpret_cast<const void*>(descriptor+
            kHalo4RuntimeCompressionBlockOffset), &header, sizeof(header)) ||
        header.count<=0 || header.count>kHalo4RuntimeCompressionMaxRecords ||
        !header.packed || header.packed==0xFFFFFFFFu) return false;
    uintptr_t biasedBase{};
    if (!Halo4SafeRead(reinterpret_cast<const void*>(g_halo4RenderModelGroupBaseTable+
            static_cast<uintptr_t>(header.packed>>28)*8),
            &biasedBase,sizeof(biasedBase)) || !biasedBase) return false;
    const uintptr_t address=biasedBase+static_cast<uintptr_t>(header.packed)*4;
    const size_t bytes=static_cast<size_t>(header.count)*kHalo4RuntimeCompressionRecordBytes;
    if (address<biasedBase || address>(std::numeric_limits<uintptr_t>::max)()-bytes)
        return false;
    Halo4WeaponCollisionBounds combined{};
    combined.runtimeImportChecksum=checksum;
    for (int record=0; record<header.count; ++record)
    {
        uint8_t raw[kHalo4RuntimeCompressionRecordBytes]{};
        Halo4WeaponCollisionBounds bounds{};
        if (!Halo4SafeRead(reinterpret_cast<const void*>(address+
                static_cast<size_t>(record)*sizeof(raw)),raw,sizeof(raw)) ||
            !Halo4DecodeRuntimeCompressionBounds(raw,bounds)) return false;
        for (int axis=0; axis<3; ++axis)
        {
            combined.minimum[axis]=record ?
                std::fmin(combined.minimum[axis],bounds.minimum[axis]) : bounds.minimum[axis];
            combined.maximum[axis]=record ?
                std::fmax(combined.maximum[axis],bounds.maximum[axis]) : bounds.maximum[axis];
            if (!std::isfinite(combined.maximum[axis]-combined.minimum[axis])) return false;
        }
    }
    // Retire torn/replaced definitions instead of publishing mixed model data.
    Header current{};
    uint32_t currentChecksum{};
    if (!Halo4SafeRead(reinterpret_cast<const void*>(descriptor+0x08),
            &currentChecksum,sizeof(currentChecksum)) || currentChecksum!=checksum ||
        !Halo4SafeRead(reinterpret_cast<const void*>(descriptor+
            kHalo4RuntimeCompressionBlockOffset), &current,sizeof(current)) ||
        current.count!=header.count || current.packed!=header.packed) return false;
    output=combined;
    return true;
}
