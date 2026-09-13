#pragma once
#include "haloce_snapshot.h"
#include "haloce_view_pair.h"
#include <limits>

namespace halo_ce
{
// E-CE-5/7: +0x455170 builds either renderer+0xB0 or preparation+0x70.
// The latter is copied to renderer+0xB0 before the second culling call.
// Association is by that explicit source, never by finding a similar camera
// in a tracking ring. Identical stationary poses can belong to different frames.
enum class PreparationOrigin : uint8_t { ActiveList, CopiedList };
struct PreparationTicket
{
    uint64_t revision{};
    uintptr_t sourceList{};
    uint32_t generation{};
    PreparationOrigin origin{};
};
struct PreparedReceipt
{
    PreparationTicket ticket;
    Tracking tracking;
    StagedViewPair pair;
    uint32_t viewFlags[2]{};
};

// Only the known pose/view/projection/raster prefix is compared. Native
// workers may populate resource references in the opaque camera tail; those
// are not tracking identity and this ledger never owns/destroys them.
inline bool SamePreparedCamera(const SaberCamera& a,const SaberCamera& b) noexcept
{
    return std::memcmp(&a,&b,offsetof(SaberCamera,derived160))==0;
}
inline bool MatchesPreparedViews(const SaberViewPair& views,const PreparedReceipt& receipt) noexcept
{
    if (views.flags!=1||views.count!=2) return false;
    for (int eye=0;eye<2;++eye)
        if (views.views[eye].viewIndex!=eye||
            views.views[eye].flags!=receipt.viewFlags[eye]||
            !SamePreparedCamera(views.views[eye].camera,receipt.pair.cameras[eye])) return false;
    return true;
}

class PreparedHandoff
{
    struct Slot
    {
        std::atomic<uint64_t> revision{};
        Snapshot<PreparedReceipt> value;
    } slots_[2];
    static size_t Index(PreparationOrigin origin) noexcept
    {
        return static_cast<size_t>(origin);
    }
public:
    // Called BEFORE every native builder, including stock/failed attempts.
    // Advancing the revision immediately revokes the old receipt. A renderer
    // cannot inherit VR identity just because the list address/pose is reused.
    // No waits; a concurrent begin loses admission for this attempt.
    PreparationTicket Begin(PreparationOrigin origin,uintptr_t sourceList,
        uint32_t generation) noexcept
    {
        const size_t index=Index(origin);
        if (index>=2) return {};
        auto& revision=slots_[index].revision;
        uint64_t previous=revision.load(std::memory_order_acquire);
        if (previous==std::numeric_limits<uint64_t>::max()||
            !revision.compare_exchange_strong(previous,previous+1)) return {};
        if (!sourceList||!generation) return {};
        return {previous+1,sourceList,generation,origin};
    }
    bool Current(const PreparationTicket& ticket) const noexcept
    {
        const size_t index=Index(ticket.origin);
        return index<2&&ticket.revision&&ticket.sourceList&&ticket.generation&&
            slots_[index].revision.load(std::memory_order_acquire)==ticket.revision;
    }
    // Publish only AFTER successful all-or-nothing native camera commit and
    // readback in the exclusive pre-culling scope. A failure drops this frame;
    // it does not disable the title or reuse the preceding receipt.
    bool Publish(const PreparationTicket& ticket,const Tracking& tracking,
        const StagedViewPair& staged,const SaberViewPair& committed) noexcept
    {
        if (!Current(ticket)||tracking.generation!=ticket.generation||
            !tracking.serial||staged.serial!=tracking.serial||
            staged.generation!=tracking.generation||staged.spaceEpoch!=tracking.spaceEpoch)
            return false;
        PreparedReceipt receipt{ticket,tracking,staged,
            {committed.views[0].flags,committed.views[1].flags}};
        // A raw caller cannot stamp reflections or unknown primary flags.
        for (int eye=0;eye<2;++eye)
        {
            const uint32_t expected=eye?0x20bu:0x10bu;
            if ((receipt.viewFlags[eye]&~0x1000u)!=expected) return false;
        }
        if (!MatchesPreparedViews(committed,receipt)) return false;
        return slots_[Index(ticket.origin)].value.Publish(receipt)&&Current(ticket);
    }
    // Called only at the native handoff after preparation/copy. The runtime
    // caller must establish source-list exclusion and supply the source from
    // that exact scope. +0x4556B0 signals completion; it is NOT a worker wait.
    // Freeze the returned receipt for the render frame BEFORE allowing another
    // preparation to begin; do not look up the latest receipt while drawing eyes.
    bool Read(PreparationOrigin origin,uintptr_t sourceList,const SaberViewPair& rendered,
        uint32_t generation,uint64_t spaceEpoch,PreparedReceipt& out) const noexcept
    {
        const size_t index=Index(origin);
        if (index>=2||!sourceList||!generation) return false;
        PreparedReceipt receipt{};
        if (!slots_[index].value.Read(receipt)||!Current(receipt.ticket)||
            receipt.ticket.origin!=origin||receipt.ticket.sourceList!=sourceList||
            receipt.ticket.generation!=generation||receipt.tracking.spaceEpoch!=spaceEpoch||
            !MatchesPreparedViews(rendered,receipt)||!Current(receipt.ticket)) return false;
        out=receipt;
        return true;
    }
    void Invalidate(PreparationOrigin origin) noexcept { (void)Begin(origin,0,0); }
};
}
