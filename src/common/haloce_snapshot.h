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
    static constexpr uint32_t writer=0x80000000u;
    T values_[2]{};
    mutable std::atomic<uint32_t> states_[2]{};
    std::atomic<uint32_t> index_{2};
public:
    bool Publish(const T& value) noexcept
    {
        const uint32_t current=index_.load(std::memory_order_seq_cst);
        const uint32_t next=current<2?1-current:0;
        uint32_t state=0;
        if (!states_[next].compare_exchange_strong(state,writer)) return false;
        values_[next]=value;
        index_.store(next,std::memory_order_seq_cst);
        states_[next].store(0,std::memory_order_seq_cst);
        return true;
    }
    bool Read(T& value) const noexcept
    {
        const uint32_t index=index_.load(std::memory_order_seq_cst);
        if (index>=2) return false;
        uint32_t state=states_[index].load(std::memory_order_seq_cst);
        if (state>=writer-1||!states_[index].compare_exchange_strong(state,state+1))
            return false;
        const bool current=index_.load(std::memory_order_seq_cst)==index;
        if (current) value=values_[index];
        states_[index].fetch_sub(1,std::memory_order_seq_cst);
        return current;
    }
};
}
