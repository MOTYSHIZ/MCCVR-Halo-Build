#pragma once
#include <atomic>
#include "../common/contact_melee_motion.h"

// Bounded render-to-simulation transport; no engine-owned pointers cross threads.
struct ContactMeleePacket
{
    contact_melee::Frame frame{};
    uint64_t publishedAtMs=0;
    uint32_t generation=0;
};
struct ContactMeleeQueue
{
    ContactMeleePacket packets[8]{};
    std::atomic<uint32_t> head{0},tail{0};
    std::atomic<bool> publishing{false};
    uint64_t lastSerial=0;
    int Push(const ContactMeleePacket& packet)
    {
        bool expected=false;
        if(!publishing.compare_exchange_strong(expected,true,std::memory_order_acquire)) return false;
        int accepted=packet.frame.serial==lastSerial ? 1 : 0;
        const uint32_t h=head.load(std::memory_order_relaxed);
        if(packet.frame.serial!=lastSerial && h-tail.load(std::memory_order_acquire)<8)
        {
            packets[h%8]=packet;
            lastSerial=packet.frame.serial;
            head.store(h+1,std::memory_order_release);
            accepted=2;
        }
        publishing.store(false,std::memory_order_release);
        return accepted;
    }
    bool Pop(ContactMeleePacket& packet)
    {
        const uint32_t t=tail.load(std::memory_order_relaxed);
        if(t==head.load(std::memory_order_acquire)) return false;
        packet=packets[t%8];
        tail.store(t+1,std::memory_order_release);
        return true;
    }
    void Reset()
    {
        head.store(0); tail.store(0); publishing.store(false); lastSerial=0;
    }
};
