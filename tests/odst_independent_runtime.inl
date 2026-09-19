static bool independentExpected=true,independentNested=false,independentThrow=false;
static float queryPosition[3]{},queryDirection[3]{};
static void __fastcall IndependentQuery(int32_t user,uint8_t flags,float* direction,int16_t zoom,float* control,void* output)
{
    ++queryCalls;Check(user==2&&zoom==3,"ODST independent query retains actual input parameters");
    if(queryFault)RaiseException(0xE0441234,0,0,nullptr);
    float camera[3]{};const auto previous=caller;caller=imageBase+0x160647;
    OdstMuzzleViewDetour(owner,flags,direction,queryPosition,camera);caller=previous;
    std::memcpy(queryDirection,direction,12);std::memset(output,0,0x28);
    auto* words=static_cast<uint32_t*>(output);words[1]=UINT32_MAX;words[2]=target;words[3]=UINT32_MAX;
    if(changeOwner)*reinterpret_cast<uint32_t*>(secondaryBytes+0x160)=owner^0x10000;
    if(nonfiniteTarget)static_cast<float*>(output)[4]=NAN;
}
static void __fastcall IndependentAim(uint32_t,float* position,float* direction,uint64_t,float* offset,uint8_t project,uint8_t use)
{
    ++aimCalls;Check(offset&&project&&use,"independent mode preserves native origin/obstruction arguments");
    position[0]=5;position[1]=6;position[2]=7;direction[0]=1;direction[1]=direction[2]=0;
}
static uint64_t __fastcall IndependentFire(uint32_t weapon,int16_t barrel,void* data,int32_t index,uint8_t predicted)
{
    ++fireCalls;Check(barrel==0&&index==-7&&predicted==0xFA,"independent fire ABI unchanged");
    const auto previous=caller;alignas(float) uint8_t markers[0x70]{};caller=imageBase+0x3AF4CA;
    OdstMuzzleMarkersDetour(weapon,0xD5,markers,64,0xAA,0xBB);
    for(auto byte:markers)Check(byte==0x5A,"controller option never rewrites native marker");
    const int slot=weapon==primary?0:1;
    if(independentExpected)
    {
        Check(g_odstMuzzleShot.active&&!g_odstMuzzleShot.barrel&&
            *reinterpret_cast<uint32_t*>(unitBytes+0x230)==target,"native target leased for selected controller");
        Check(queryDirection[slot]==1&&queryDirection[1-slot]==0,"guns query independently");
    }
    else Check(!g_odstMuzzleShot.active,"refused controller shot remains native");
    float position[3]{},direction[3]{},offset[3]{};caller=imageBase+0x3AEB2B;
    OdstMuzzleAimDetour(owner,position,direction,0,offset,1,1);
    Check(position[0]==5&&position[1]==6&&position[2]==7,"native obstruction origin retained");
    if(independentExpected)
    {
        float expected[3]{};BuildIndependentWeaponDirection(position,queryPosition,queryDirection,10,expected);
        for(int axis=0;axis<3;++axis)Check(std::abs(direction[axis]-expected[axis])<1e-6f,"per-gun convergence uses native origin");
        caller=imageBase+0x161204;OdstMuzzleCameraDetour(owner,position,direction);
        Check(std::memcmp(position,queryPosition,12)==0&&std::memcmp(direction,queryDirection,12)==0,
            "later native assist consumes same controller query ray");
    }
    else Check(direction[0]==1&&direction[1]==0&&direction[2]==0,"refused direction native");
    if(independentNested&&weapon==primary)
    {
        const auto outer=g_odstMuzzleShot;
        Check(OdstMuzzleFireDetour(secondary,0,data,-7,0xFA)==99,"nested independent fire return");
        Check(g_odstMuzzleShot.active&&std::memcmp(g_odstMuzzleShot.direction,outer.direction,12)==0,
            "nested shot restores primary scope");
    }
    caller=previous;if(independentThrow)RaiseException(0xE0441235,0,0,nullptr);return 99;
}
static OdstIndependentAimSnapshot IndependentFixture()
{
    OdstIndependentAimSnapshot publication{};auto& aim=publication.aim;
    publication.leftHanded=g_config.left_handed;aim.generation=7;aim.unit=owner;
    aim.weapons[0]=primary;aim.weapons[1]=secondary;aim.trackingEpoch=3;
    aim.sampleMs=GetTickCount64();aim.timeNs=tracking.timeNs;
    aim.positions[0][0]=2;aim.positions[0][1]=3;aim.positions[0][2]=4;aim.directions[0][0]=1;
    aim.positions[1][0]=-2;aim.positions[1][1]=-3;aim.positions[1][2]=4;aim.directions[1][1]=1;
    return publication;
}
static void ResetIndependent()
{
    Reset(true);g_config.gun_barrel_aim=false;g_config.independent_dual_aim=true;
    g_odstMuzzle.queryOriginal=IndependentQuery;g_odstMuzzle.fireOriginal=IndependentFire;
    g_odstMuzzle.aimOriginal=IndependentAim;independentExpected=true;independentNested=independentThrow=false;
    Check(g_odstMuzzle.controllerAim.Publish(IndependentFixture()),"independent pair published");
}
static void IndependentTests()
{
    // Both toggles: the later authored marker nests over the controller lease.
    Reset(true);g_config.independent_dual_aim=true;
    auto both=IndependentFixture();
    for(int slot=0;slot<2;++slot)
    {both.aim.positions[slot][0]=2;both.aim.positions[slot][1]=2.5f;both.aim.positions[slot][2]=4;
        both.aim.directions[slot][0]=0;both.aim.directions[slot][1]=1;}
    Check(g_odstMuzzle.controllerAim.Publish(both),"combined controller pair published");
    nestedFire=true;(void)Shoot();
    Check(fireCalls==2&&queryCalls==4&&*reinterpret_cast<uint32_t*>(unitBytes+0x230)==0x44440007&&
        !g_odstMuzzleShot.active&&!g_odstMuzzle.callbacks,"barrel precedence and nested controller leases restore in order");
    for(bool left:{false,true})
    {
        ResetIndependent();g_config.left_handed=left;
        Check(g_odstMuzzle.controllerAim.Publish(IndependentFixture()),"handed pair published");
        independentNested=true;Check(Shoot()==99,"both independent guns fire");
        Check(fireCalls==2&&queryCalls==2&&*reinterpret_cast<uint32_t*>(unitBytes+0x230)==0x44440007&&
            !g_odstMuzzleShot.active&&!g_odstMuzzleRequest.lease&&!g_odstMuzzle.callbacks,
            "simultaneous shots restore target and every scope");
    }
    for(int reason=0;reason<24;++reason)
    {
        ResetIndependent();independentExpected=false;auto publication=IndependentFixture();
        switch(reason)
        {
        case 0:g_config.independent_dual_aim=false;break;
        case 1:unitBytes[0x277]=0xFF;break;
        case 2:publication.aim.sampleMs=GetTickCount64()-200;break;
        case 3:publication.aim.timeNs-=200000000;break;
        case 4:publication.aim.trackingEpoch=4;break;
        case 5:publication.aim.generation=8;break;
        case 6:publication.aim.unit^=0x10000;break;
        case 7:publication.aim.weapons[1]^=0x10000;break;
        case 8:publication.leftHanded=true;break;
        case 9:tracking.hands[1].valid=false;break;
        case 10:exclusive_input::active=true;break;
        case 11:cinematic=true;break;
        case 12:seated=true;break;
        case 13:g_odstMuzzleQueryContext.sampleMs=GetTickCount64()-200;break;
        case 14:g_odstMuzzleQueryContext.generation=8;break;
        case 15:localUnit^=0x10000;break;
        case 16:*reinterpret_cast<uint32_t*>(unitBytes+0x2B8)=target;break;
        case 17:publication.aim.positions[0][0]=NAN;break;
        case 18:publication.aim.directions[0][0]=0;break;
        case 19:queryFault=true;break;
        case 20:changeOwner=true;break;
        case 21:nonfiniteTarget=true;break;
        case 22:g_odstMuzzle.enabled=false;break;
        case 23:g_odstMuzzle.faulted=true;break;
        }
        Check(g_odstMuzzle.controllerAim.Publish(publication),"refusal input published");
        Check(Shoot()==99&&fireCalls==1&&!g_odstMuzzle.callbacks&&!g_legacyCollisionOwnedQuery,
            "independent refusal forwards native once and drains");
    }
    ResetIndependent();independentThrow=true;
    Check(FaultingShot()&&fireCalls==1&&!g_odstMuzzle.callbacks&&!g_odstMuzzleShot.active&&
        *reinterpret_cast<uint32_t*>(unitBytes+0x230)==0x44440007,"native fire fault is never replayed and lease restores");
    // Exercise the actual publication path, including routed role coordinates.
    ResetIndependent();tracking.hands[1].position[0]=2;tracking.hands[1].position[1]=3;
    tracking.hands[1].position[2]=-4;tracking.hands[0].position[0]=-2;
    tracking.hands[0].position[2]=-8;tracking.hands[0].orientation[0]=0;tracking.hands[0].orientation[1]=1;
    PublishOdstIndependentAim();OdstIndependentAimSnapshot result{};
    Check(g_odstMuzzle.controllerAim.Read(result)&&result.aim.positions[0][0]==4&&result.aim.positions[0][1]==-2&&
        result.aim.positions[0][2]==3&&result.aim.positions[1][0]==8&&result.aim.positions[1][1]==2&&
        result.aim.directions[1][1]==1&&!g_odstMuzzle.callbacks,"production publication converts each routed hand coherently");
    Reset();
}
