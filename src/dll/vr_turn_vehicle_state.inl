// Read the title's existing, bounded occupancy proof. First-person vehicle view
// and motion steering are separate settings and do not select this turn mode.
bool SharedVrTurnSeated()
{
    const auto title=TitleAdapter_GetActiveTitle();
    const auto generation=TitleAdapter_GetGeneration(title);
    if(!generation)return false;
    if(title==GameTitle::Halo3)
    {
        if(generation!=g_halo3RuntimeGeneration.load(std::memory_order_acquire))return false;
        const auto seat=Game_Halo3VehicleState();
        return seat.state==Halo3VehicleState::Vehicle&&seat.seatIndex>=0;
    }
    if(title==GameTitle::Halo3ODST)
    {
        if(generation!=g_odstRuntimeGeneration.load(std::memory_order_acquire)||
            g_odstVehicleBinding.load(std::memory_order_acquire)!=
                static_cast<uint8_t>(OdstVehicleBindingState::Installed)||
            g_odstVehicleFpStable.load(std::memory_order_relaxed)!=
                static_cast<uint32_t>(Halo3VehicleState::Vehicle))return false;
        OdstSeatSnapshot seat{};
        return OdstReadSeatSnapshot(seat)&&seat.seatIndex>=0&&
            uint32_t(seat.parentHandle)!=UINT32_MAX&&(uint32_t(seat.parentHandle)>>16)!=0;
    }
    return title==GameTitle::HaloReach&&Game_ReachPlayerIsInVehicle();
}
