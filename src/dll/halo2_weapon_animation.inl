// Separate native packet producers, shared presentation policy. Neither
// renderer borrows the other's camera-local reference across a graphics switch.
thread_local weapon_animation::Reference g_h2WeaponAnimation[2][2];
thread_local weapon_animation::ReferenceCache g_h2AnimationReferenceCache[2][2];
std::atomic<uint64_t> g_h2WeaponAnimationApplied{},g_h2WeaponAnimationSettling{};
void Halo2AnimateWeaponCarriers(Halo2VisibleConsumerContext& context,
    const float* hands,const float* primary,const float* secondary,int renderer,uint32_t primaryModel,uint32_t secondaryModel)
{
    for(int slot=0;slot<2;++slot) {
        auto& reference=g_h2WeaponAnimation[renderer][slot];
        const float* native=slot?secondary:primary;
        const uint32_t weapon=slot?context.secondaryWeaponObject:context.weaponObject;
        if(!native||weapon==UINT32_MAX||context.unitObject==UINT32_MAX) {reference={};continue;}
        const auto& binding=slot?context.secondaryBinding:context.binding;
        const int wristSource=slot?binding.leftWrist:binding.rightWrist;
        const int32_t* remap=slot?context.secondaryHandsRemap:context.handsRemap;
        int wrist=-1;
        if(!hands||!remap||context.handsCount>kHalo2FirstPersonPaletteCapacity) {reference={};continue;}
        for(uint32_t i=0;i<context.handsCount;++i)if(remap[i]==wristSource) {
            if(wrist!=-1){wrist=-1;break;}wrist=int(i);
        }
        if(wrist<0||wristSource<0){reference={};continue;}
        weapon_animation::Pose delta{};
        const uint32_t model=slot?secondaryModel:primaryModel;
        const uint32_t count=slot?context.secondaryGunCount:context.gunCount;
        // Dual wield changes the primary's authored stance even when its own
        // weapon handle/model stays the same. Both hands belong to this pair.
        const uint32_t partner=slot?context.weaponObject:context.secondaryWeaponObject;
        if(!Halo2ObserveWeaponAnimation(reference,Halo2AnimationIdentity(g_generation.load(),
                context.unitObject,weapon,model,count,partner),native,hands+wrist*kHalo2FirstPersonNodeFloats,context.renderCamera,
                GetTickCount64(),context.worldScale,delta,&g_h2AnimationReferenceCache[renderer][slot])) {++g_h2WeaponAnimationSettling;continue;}
        auto carrier=slot?context.leftCarrier:context.rightCarrier;
        auto visual=context.muzzleVisualCarrier[slot],neutral=context.muzzleNeutralCarrier[slot];
        const float scale=slot?context.leftScale:context.rightScale;
        if(!Halo2ApplyWeaponAnimation(carrier,delta,scale)||
            (context.muzzleVisualFrameValid&&(!Halo2ApplyWeaponAnimation(visual,delta,scale)||
                !Halo2ApplyWeaponAnimation(neutral,delta,scale))))continue;
        (slot?context.leftCarrier:context.rightCarrier)=carrier;
        if(context.muzzleVisualFrameValid) {
            context.muzzleVisualCarrier[slot]=visual;context.muzzleNeutralCarrier[slot]=neutral;
        }
        ++g_h2WeaponAnimationApplied;
    }
}
