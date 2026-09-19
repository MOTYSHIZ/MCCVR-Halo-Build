void Halo2ApplyVisualHandOffsets(const Halo2VisibleConsumerContext& context,float* hands,bool dual) noexcept
{
    if(!VisualHandOffsetsActive(g_config)||!context.valid||!hands||!context.handsRemap||
        !context.binding.valid||!context.handsCount||context.handsCount>64||
        (dual&&(!context.secondaryHandsRemap||!context.secondaryBinding.valid)))return;
    __try
    {
        Halo2FirstPersonTransform staged[64]{};uint64_t left=0,right=0;int leftWrist=-1,rightWrist=-1;
        for(uint32_t node=0;node<context.handsCount;++node)
        {
            if(!Halo2ReadFirstPersonTransform(hands+node*kHalo2FirstPersonNodeFloats,staged[node]))return;
            const int r=context.handsRemap[node],l=dual?context.secondaryHandsRemap[node]:r;
            const auto& lb=dual?context.secondaryBinding:context.binding;
            if(r<0||r>=int(context.binding.count)||l<0||l>=int(lb.count)||r>=64||l>=64)return;
            if(r==context.binding.rightWrist)rightWrist=int(node);if(l==lb.leftWrist)leftWrist=int(node);
            if(context.binding.rightSubtree&(uint64_t{1}<<r))right|=uint64_t{1}<<node;
            if(lb.leftSubtree&(uint64_t{1}<<l))left|=uint64_t{1}<<node;
            if(staged[node].scale<.001f)
            {
                if(context.binding.rightArmAncestors&(uint64_t{1}<<r))right|=uint64_t{1}<<node;
                if(lb.leftArmAncestors&(uint64_t{1}<<l))left|=uint64_t{1}<<node;
            }
        }
        if(leftWrist<0||rightWrist<0)return;float ld[3]{},rd[3]{};
        if(!visual_hand::Deltas(staged[leftWrist].rotation,staged[rightWrist].rotation,context.worldScale,
                VisualLeftHandOffset(g_config),VisualRightHandOffset(g_config),g_config.left_handed,context.handAlignment,ld,rd)||
            !visual_hand::Apply(context.handsCount,left,right,ld,rd,[&](size_t node){return staged[node].translation;},
                [&](size_t node,const float* value){std::memcpy(staged[node].translation,value,12);}))return;
        for(uint32_t node=0;node<context.handsCount;++node)
            if((left|right)&(uint64_t{1}<<node))
                std::memcpy(hands+node*kHalo2FirstPersonNodeFloats+10,staged[node].translation,12);
    }
    __except(EXCEPTION_EXECUTE_HANDLER){}
}
