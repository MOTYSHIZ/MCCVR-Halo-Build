#pragma once
#include <atomic>
#include <cstdint>
#include <type_traits>

namespace halo_ce
{
// Bounded publication, following the existing per-title render snapshots.
// Readers pin a buffer; a busy buffer causes a missed publication/read, never
// a render-thread wait or a concurrent non-atomic payload access.
template<class T> class Snapshot
{
    static_assert(std::is_trivially_copyable_v<T>);
    static constexpr uint64_t writer=uint64_t{1}<<63;
    T values_[2]{};
    mutable std::atomic<uint64_t> states_[2]{};
    std::atomic<uint32_t> index_{2};
public:
    bool Publish(const T& value) noexcept
    {
        const uint32_t current=index_.load(std::memory_order_seq_cst);
        const uint32_t next=current<2?1-current:0;
        uint64_t state=0;
        if (!states_[next].compare_exchange_strong(state,writer)) return false;
        values_[next]=value;
        index_.store(next,std::memory_order_seq_cst);
        // A reader may briefly increment a writer-owned slot and then reject
        // it. Preserve that outstanding pin instead of clearing its count;
        // otherwise its later decrement would underflow the slot state.
        states_[next].fetch_sub(writer,std::memory_order_seq_cst);
        return true;
    }
    bool Read(T& value) const noexcept
    {
        const uint32_t index=index_.load(std::memory_order_seq_cst);
        if (index>=2) return false;
        // Readers do not exclude one another. A single-CAS increment could
        // reject solely because another reader changed the count, causing
        // independent material workers to select conflicting depth/color
        // lenses despite reading an unchanged policy. One atomic pin avoids
        // that failure without waiting or retrying. The high bit reserves the
        // writer; 63 count bits exceed the possible live reader population.
        const uint64_t state=states_[index].fetch_add(1,std::memory_order_seq_cst);
        if (state>=writer-1)
        {
            states_[index].fetch_sub(1,std::memory_order_seq_cst);
            return false;
        }
        const bool current=index_.load(std::memory_order_seq_cst)==index;
        if (current) value=values_[index];
        states_[index].fetch_sub(1,std::memory_order_seq_cst);
        return current;
    }
};
}
