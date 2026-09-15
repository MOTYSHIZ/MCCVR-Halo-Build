#include "haloce_hud_target.h"
#include "../common/haloce_contracts.generated.h"
#include <cstring>

namespace
{
template<class T> T ReadValue(uintptr_t address) noexcept
{
    T value{};
    std::memcpy(&value,reinterpret_cast<const void*>(address),sizeof(value));
    return value;
}
using SelectFn=uintptr_t(__fastcall*)(uintptr_t);
using BindFn=bool(__fastcall*)(uintptr_t,const void*);
bool OwnedSurface(uintptr_t base,uintptr_t wrapper,uint32_t requiredFlag,
    uintptr_t& surface,uintptr_t& resource,SelectFn select) noexcept
{
    if (!wrapper||ReadValue<uintptr_t>(wrapper)!=base+0x17fb608||
        !(ReadValue<uint32_t>(wrapper+0x88)&requiredFlag)) return false;
    surface=select(wrapper);
    if (!surface||ReadValue<uintptr_t>(surface)!=base+0x17fb608) return false;
    resource=ReadValue<uintptr_t>(surface+0xe0);
    return resource!=0;
}
bool ReadBody(uintptr_t base,ID3D11DeviceContext* expected,CeHudTargetSnapshot& out,SelectFn select) noexcept
{
    if (!base||!expected) return false;
    CeHudTargetSnapshot result{};
    result.moduleBase=base;result.context=expected;
    result.backend=ReadValue<uintptr_t>(base+0x2e3bde0);
    if (!result.backend||ReadValue<uintptr_t>(result.backend)!=base+0x17f9d10||
        ReadValue<ID3D11DeviceContext*>(base+0x2ea2d30)!=expected||
        ReadValue<ID3D11DeviceContext*>(result.backend+0xce0)!=expected) return false;
    std::memcpy(result.descriptor.data(),reinterpret_cast<const void*>(result.backend+0x18),0x48);
    result.count=ReadValue<uint32_t>(result.backend+0xcf8);
    if (result.count>4) return false;
    std::memcpy(result.rtvs,reinterpret_cast<const void*>(result.backend+0xd00),sizeof(result.rtvs));
    result.dsv=ReadValue<ID3D11DepthStencilView*>(result.backend+0xd20);
    const uintptr_t descriptor=reinterpret_cast<uintptr_t>(result.descriptor.data());
    uint32_t colors=0;
    bool ended=false;
    for (uint32_t slot=0;slot<4;++slot)
    {
        uintptr_t wrapper=ReadValue<uintptr_t>(descriptor+0x10+slot*8);
        const uint32_t flags=ReadValue<uint32_t>(descriptor+slot*4);
        const bool defaultTarget=!wrapper&&(flags&2)!=0;
        if (!wrapper&&!defaultTarget) ended=true;
        // The native binder validates all explicit color wrappers before its
        // first unused slot terminates the active target list.
        if (wrapper)
        {
            if (!OwnedSurface(base,wrapper,1u<<8,result.surfaces[slot],result.resources[slot],select)) return false;
            result.wrappers[slot]=wrapper;
        }
        if (ended) continue;
        if (defaultTarget)
        {
            wrapper=ReadValue<uintptr_t>(result.backend+0xcf0);
            if (!OwnedSurface(base,wrapper,1u<<8,result.surfaces[slot],result.resources[slot],select)) return false;
            result.wrappers[slot]=wrapper;
        }
        const int32_t mip=defaultTarget?0:ReadValue<int16_t>(descriptor+0x40);
        const int32_t face=defaultTarget?0:ReadValue<int8_t>(descriptor+0x42);
        const int32_t layer=defaultTarget?0:ReadValue<int8_t>(descriptor+0x43);
        const int32_t mipCount=ReadValue<int8_t>(wrapper+0x1a);
        const int32_t faces=(ReadValue<uint32_t>(wrapper+0x88)&(1u<<12))?6:1;
        if (mipCount<=0||mipCount>16||mip<0||mip>=mipCount||face<0||face>=faces||layer<0) return false;
        const uint32_t index=uint32_t((faces*layer+face)*mipCount+mip);
        const uintptr_t tableOffset=(flags&(1u<<6))?0xf8:0xe8;
        const uintptr_t table=ReadValue<uintptr_t>(result.surfaces[slot]+tableOffset);
        // Native teardown22B0A0 walks BOTH E8/F8 tables with the F0 count,
        // including the case where the alternate table aliases the first.
        const int32_t count=ReadValue<int32_t>(result.surfaces[slot]+0xf0);
        if (!table||count<=0||count>4096||index>=uint32_t(count)) return false;
        const auto view=ReadValue<ID3D11RenderTargetView*>(table+index*sizeof(uintptr_t));
        if (!view||colors>=result.count||result.rtvs[colors]!=view) return false;
        ++colors;
    }
    if (colors!=result.count) return false;
    const uintptr_t depth=ReadValue<uintptr_t>(descriptor+0x30);
    if (depth)
    {
        if (!OwnedSurface(base,depth,1u<<9,result.surfaces[4],result.resources[4],select)) return false;
        result.wrappers[4]=depth;
        const auto view=ReadValue<ID3D11DepthStencilView*>(result.surfaces[4]+
            (ReadValue<uint8_t>(descriptor+0x44)?0x110:0x108));
        if (!view||result.dsv!=view) return false;
    }
    else if (result.dsv) return false;
    // Reject a changing publication instead of mixing native descriptor and
    // view-cache states. No pointer here grants ownership to the adapter.
    if (ReadValue<uintptr_t>(base+0x2e3bde0)!=result.backend||
        ReadValue<uintptr_t>(result.backend)!=base+0x17f9d10||
        ReadValue<ID3D11DeviceContext*>(result.backend+0xce0)!=expected||
        ReadValue<uint32_t>(result.backend+0xcf8)!=result.count||
        std::memcmp(result.descriptor.data(),reinterpret_cast<const void*>(result.backend+0x18),0x48)||
        std::memcmp(result.rtvs,reinterpret_cast<const void*>(result.backend+0xd00),sizeof(result.rtvs))||
        ReadValue<ID3D11DepthStencilView*>(result.backend+0xd20)!=result.dsv) return false;
    out=result;
    return true;
}
bool SameIntent(const CeHudTargetSnapshot& a,const CeHudTargetSnapshot& b) noexcept
{
    return a.backend==b.backend&&a.context==b.context&&a.descriptor==b.descriptor&&a.count==b.count&&
        a.dsv==b.dsv&&!std::memcmp(a.rtvs,b.rtvs,sizeof(a.rtvs))&&
        !std::memcmp(a.wrappers,b.wrappers,sizeof(a.wrappers))&&
        !std::memcmp(a.surfaces,b.surfaces,sizeof(a.surfaces))&&
        !std::memcmp(a.resources,b.resources,sizeof(a.resources));
}
bool CaptureSourceCurrentBody(const CeHudTargetSnapshot& saved,SelectFn select) noexcept
{
    if (!saved.moduleBase||!saved.backend||!saved.context||saved.count!=1||
        !saved.wrappers[0]||!saved.surfaces[0]||!saved.resources[0]||
        ReadValue<uintptr_t>(saved.moduleBase+0x2e3bde0)!=saved.backend||
        ReadValue<uintptr_t>(saved.backend)!=saved.moduleBase+0x17f9d10||
        ReadValue<ID3D11DeviceContext*>(saved.moduleBase+0x2ea2d30)!=saved.context||
        ReadValue<ID3D11DeviceContext*>(saved.backend+0xce0)!=saved.context||
        std::memcmp(saved.descriptor.data(),reinterpret_cast<const void*>(saved.backend+0x18),0x48)) return false;
    // 740B0 restores target-kind globals and clears the output-merger cache.
    // Follow the saved descriptor's wrapper, not either restored kind global.
    uintptr_t surface{},resource{};
    if (!OwnedSurface(saved.moduleBase,saved.wrappers[0],1u<<8,surface,resource,select)||
        surface!=saved.surfaces[0]||resource!=saved.resources[0]) return false;
    const uintptr_t descriptor=reinterpret_cast<uintptr_t>(saved.descriptor.data());
    const bool defaultTarget=!ReadValue<uintptr_t>(descriptor+0x10)&&(ReadValue<uint32_t>(descriptor)&2);
    if (defaultTarget&&ReadValue<uintptr_t>(saved.backend+0xcf0)!=saved.wrappers[0]) return false;
    return ReadValue<uintptr_t>(saved.moduleBase+0x2e3bde0)==saved.backend&&
        ReadValue<ID3D11DeviceContext*>(saved.moduleBase+0x2ea2d30)==saved.context&&
        ReadValue<ID3D11DeviceContext*>(saved.backend+0xce0)==saved.context&&
        ReadValue<uintptr_t>(surface+0xe0)==resource&&
        !std::memcmp(saved.descriptor.data(),reinterpret_cast<const void*>(saved.backend+0x18),0x48);
}
bool DetachedDepthCurrent(const CeHudTargetSnapshot& saved,SelectFn select) noexcept
{
    if (!saved.dsv) return true;
    uintptr_t surface{},resource{};
    if (!OwnedSurface(saved.moduleBase,saved.wrappers[4],1u<<9,surface,resource,select)||
        surface!=saved.surfaces[4]||resource!=saved.resources[4]) return false;
    const auto view=ReadValue<ID3D11DepthStencilView*>(surface+(saved.descriptor[0x44]?0x110:0x108));
    return view==saved.dsv;
}
CeHudTargetRestoreResult RestoreBody(const CeHudTargetSnapshot& saved,SelectFn select,BindFn bind) noexcept
{
    CeHudTargetSnapshot latest{};
    if (!ReadBody(saved.moduleBase,saved.context,latest,select)||latest.backend!=saved.backend)
        return CeHudTargetRestoreResult::Unavailable;
    const bool same=SameIntent(saved,latest);
    if (!bind(latest.backend,latest.descriptor.data())) return CeHudTargetRestoreResult::Unavailable;
    CeHudTargetSnapshot rebound{};
    if (!ReadBody(saved.moduleBase,saved.context,rebound,select)||rebound.backend!=saved.backend)
        return CeHudTargetRestoreResult::Unavailable;
    return same&&SameIntent(latest,rebound)?CeHudTargetRestoreResult::Unchanged:
        CeHudTargetRestoreResult::Changed;
}
}
bool HaloCEHudTarget_Read(uintptr_t base,ID3D11DeviceContext* expected,CeHudTargetSnapshot& out) noexcept
{
    const auto select=reinterpret_cast<SelectFn>(base+halo_ce::contract::hud_target::hud_target_surface_select);
    __try { return ReadBody(base,expected,out,select); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool HaloCEHudTarget_CaptureSourceCurrent(const CeHudTargetSnapshot& saved) noexcept
{
    const auto select=reinterpret_cast<SelectFn>(saved.moduleBase+halo_ce::contract::hud_target::hud_target_surface_select);
    __try { return CaptureSourceCurrentBody(saved,select); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
CeHudTargetRestoreResult HaloCEHudTarget_Restore(const CeHudTargetSnapshot& saved) noexcept
{
    const auto select=reinterpret_cast<SelectFn>(saved.moduleBase+halo_ce::contract::hud_target::hud_target_surface_select);
    const auto bind=reinterpret_cast<BindFn>(saved.moduleBase+halo_ce::contract::hud_target::hud_target_bind);
    __try { return RestoreBody(saved,select,bind); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return CeHudTargetRestoreResult::Unavailable; }
}
bool HaloCEHudTarget_Replace(const CeHudTargetSnapshot& expected,
    const std::array<uint8_t,0x48>& descriptor,CeHudTargetSnapshot& result) noexcept
{
    const auto select=reinterpret_cast<SelectFn>(expected.moduleBase+halo_ce::contract::hud_target::hud_target_surface_select);
    const auto bind=reinterpret_cast<BindFn>(expected.moduleBase+halo_ce::contract::hud_target::hud_target_bind);
    __try
    {
        CeHudTargetSnapshot current{};
        return ReadBody(expected.moduleBase,expected.context,current,select)&&SameIntent(expected,current)&&
            bind(expected.backend,descriptor.data())&&ReadBody(expected.moduleBase,expected.context,result,select)&&
            result.backend==expected.backend&&result.descriptor==descriptor;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool HaloCEHudTarget_RestorePrepared(const CeHudTargetSnapshot& original,
    const std::array<uint8_t,0x48>& preparedDescriptor,CeHudTargetSnapshot& result) noexcept
{
    const auto select=reinterpret_cast<SelectFn>(original.moduleBase+halo_ce::contract::hud_target::hud_target_surface_select);
    const auto bind=reinterpret_cast<BindFn>(original.moduleBase+halo_ce::contract::hud_target::hud_target_bind);
    __try
    {
        CeHudTargetSnapshot current{};
        if (ReadBody(original.moduleBase,original.context,current,select)&&SameIntent(original,current))
        { result=current;return true; }
        auto prepared=original;prepared.descriptor=preparedDescriptor;
        if ((!CaptureSourceCurrentBody(prepared,select)&&!CaptureSourceCurrentBody(original,select))||
            !DetachedDepthCurrent(original,select)) return false;
        if (!bind(original.backend,original.descriptor.data())||
            !ReadBody(original.moduleBase,original.context,result,select)) return false;
        return SameIntent(original,result);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
