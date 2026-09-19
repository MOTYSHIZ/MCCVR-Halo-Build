bool ReachCaptureMuzzleOwner(int view,int slot,uint32_t& unit,uint32_t& weapon) noexcept
{
    if(!g_config.gun_barrel_aim||!g_reachBarrelBindingsReady.load(std::memory_order_acquire)||view!=0||slot<0||slot>1||
        !g_reachCamera.playerUnitByOutputUser||!g_reachCamera.unitInVehicle)return false;
    __try
    {
        const uint32_t owner=uint32_t(g_reachCamera.playerUnitByOutputUser(0));uint32_t weapons[2]{};
        if(!ReachMuzzleTargetStorage(owner)||!ReachReadMuzzleWeapons(owner,weapons)||weapons[slot]==UINT32_MAX)return false;
        auto** slots=reinterpret_cast<uint8_t**>(__readgsqword(0x58));
        const auto index=*reinterpret_cast<const uint32_t*>(g_reachCamera.base+kReachEngineTlsIndexRva);
        const auto* tls=slots&&index<0x200?slots[index]:nullptr;
        // HREK 8CEED0 / retail 2B1218: TLS6A0, user53A8, slot2978, full weapon3C.
        const auto* users=tls?*reinterpret_cast<const uint8_t* const*>(tls+0x6A0):nullptr;
        if(!users||*reinterpret_cast<const uint32_t*>(users+slot*0x2978+0x3C)!=weapons[slot])return false;
        unit=owner;weapon=weapons[slot];return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void ReachPublishMuzzlePalette(uint16_t tag,const ReachFpInterpolationContext& context,const BoneMatrix* destination) noexcept
{
    if(!g_config.gun_barrel_aim||!g_reachBarrelBindingsReady.load(std::memory_order_acquire)||
        !context.valid||context.interpolationView!=0||context.interpolationSlot!=0||!destination||
        !g_reachFpPairScope.armed||!context.generation||context.generation!=g_reachCamera.generation.load()||
        context.preparedSerial!=g_reachFpPairScope.preparedSerial)return;
    __try
    {
        uint32_t checksum=0;int count=0;
        if(!ReachReadRenderModelIdentity(tag,checksum,count)||count<=0||count>64||
            (!weapon_muzzle::Find(GameTitle::HaloReach,checksum,0)&&!weapon_muzzle::Find(GameTitle::HaloReach,checksum,1)))return;
        weapon_muzzle::Palette palette{};const auto& tracking=context.targets;
        uint32_t unit=UINT32_MAX,weapon=UINT32_MAX;
        if(ReachCaptureMuzzleOwner(0,0,unit,weapon)&&unit==context.muzzleUnit&&weapon==context.muzzleWeapon&&
            tracking.contactSerial&&tracking.contactReference&&tracking.contactTimeNs>0&&tracking.rightWristValid)
        {
            const auto now=GetTickCount64();
            for(uint8_t barrel=0;barrel<2;++barrel)
            {
                const auto* marker=weapon_muzzle::Find(GameTitle::HaloReach,checksum,barrel);weapon_muzzle::Ray ray{};
                if(!marker||marker->nodeCount!=count||marker->node>=count)continue;
                const auto& node=destination[marker->node];
                if(!weapon_muzzle::Transform(*marker,node.scale,node.rotation,node.translation,ray))continue;
                palette.barrels[barrel]={GameTitle::HaloReach,context.generation,unit,weapon,checksum,
                    tracking.contactReference,tracking.contactSerial,now,tracking.contactTimeNs,0,barrel,g_config.left_handed,ray};
            }
        }
        (void)g_reachBarrelMuzzles.Publish(GameTitle::HaloReach,0,palette);
    }
    __except(EXCEPTION_EXECUTE_HANDLER){(void)g_reachBarrelMuzzles.Publish(GameTitle::HaloReach,0,{});}
}
