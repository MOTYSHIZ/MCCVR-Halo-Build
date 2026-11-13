#include "odst_finger_pose_backend.inl"
namespace odst_finger_runtime {
// [0] ODST, [1] Halo 3: the same 37-node first-person rig (odst_finger_pose_logic.h).
std::atomic<uint64_t> applications[2]{},refusals[2]{},faults[2]{};
std::atomic<uint32_t> faultedGeneration[2]{};
inline bool FingersWanted() noexcept { return g_config.experimental_body_ik||g_config.hand_gestures; }
bool Gameplay(GameTitle title,const FpInterpolationContext& context,uint32_t generation) {
    return context.valid&&context.generation==generation&&context.player==0&&context.slot==0&&
       g_fpStereoSolveScope.armed&&!g_scopeRenderActive.load()&&g_enabled.load()&&g_vrAim.load()&&
       VR_IsStereoEnabled()&&!VR_IsCutsceneTheaterActive()&&!exclusive_input::Active()&&
       TitleAdapter_GetActiveTitle()==title&&TitleAdapter_GetRuntimeMode()==RuntimeMode::Gameplay;
}
bool TrackingCurrent(const VrContactTrackingSnapshot& tracking) {
    VrContactTrackingSnapshot current{};
    return tracking.serial&&tracking.referenceEpoch&&tracking.timeNs>0&&tracking.headValid&&
       tracking.hands[0].valid&&VR_GetContactTrackingSnapshot(current)&&
       current.serial==tracking.serial&&current.referenceEpoch==tracking.referenceEpoch;
}
// The hands whose fingers are free this solve, and what shapes them:
//  - the support hand, off the gun and not dual wielding: a playing track's
//    hand shape, else the finger gesture, else trigger curls the index and grip
//    the rest;
//  - the gun hand, only while the gun is stowed: the gesture, else the curl.
bool Configure(odst_fingers::Request& request,const VrContactTrackingSnapshot& tracking,
    bool secondaryPresent,bool attached,bool stowed) {
    const bool support=odst_fingers::ConfigureFreeSupport(request,tracking.handAlignment,secondaryPresent,
        attached||tracking.twoHandAimActive||g_fpStereoSolveScope.twoHandAimActive,tracking.controllerFingers[0]);
    const bool primary=stowed&&!secondaryPresent&&
        odst_fingers::ConfigureStowedPrimary(request,tracking.handAlignment,tracking.controllerFingers[1]);
    if(!support&&!primary)return false;
    const bool gestures=g_config.hand_gestures;
    if(support) {
        const unsigned side=FingerAnatomicalSupportSide(tracking.handAlignment);
        const auto& raw=tracking.controllerFingers[0];
        float controller[hand_track::kFingers]{raw.trigger,raw.grip,raw.grip,raw.grip,raw.grip};
        float gesture[hand_track::kFingers]{};
        const bool haveGesture=gestures&&HandTracks_GestureCurls(0,gesture);
        if(haveGesture)std::memcpy(controller,gesture,sizeof(controller));
        float curls[hand_track::kFingers]{};
        if(HandTracks_CurrentCurls(controller,curls))(void)odst_fingers::SetSideCurls(request,side,curls);
        else if(haveGesture&&odst_fingers::SetSideCurls(request,side,gesture))HandTracks_NoteGestureApplied(0);
    }
    if(primary) {
        const unsigned side=1u-FingerAnatomicalSupportSide(tracking.handAlignment);
        float gesture[hand_track::kFingers]{};
        if(gestures&&HandTracks_GestureCurls(1,gesture)&&odst_fingers::SetSideCurls(request,side,gesture))
            HandTracks_NoteGestureApplied(1);
    }
    return true;
}
bool ReadInverse(const uint8_t* base,uint32_t address,size_t stride,size_t inverseOffset,odst_fingers::Pose& inverse) {
    if(!base||!address)return false;
    const auto* nodes=base+size_t(address)*4;
    for(unsigned node=0;node<odst_fingers::Count;++node)
        std::memcpy(&inverse[node],nodes+size_t(node)*stride+inverseOffset,sizeof(inverse[node]));
    return true;
}
bool Prepare(uint16_t tag,const FpInterpolationContext& context,uint32_t generation,
    odst_fingers::Pose& inverse,odst_fingers::Request& request) {
    if(!FingersWanted()||!generation||faultedGeneration[0].load()==generation||
       !g_odstCamera.armed.load()||g_odstCamera.teardownRequested.load()||
       generation!=g_odstRuntimeGeneration.load()||!Gameplay(GameTitle::Halo3ODST,context,generation))return false;
    const auto& tracking=g_fpStereoSolveScope.anatomicalTracking;
    if(!TrackingCurrent(tracking)||!g_odstPlayerUnitGetter||!g_odstEngineTlsIndex||*g_odstEngineTlsIndex>=0x200)return false;
    const auto* model=OdstLoadedTagDefinition(tag);if(!model)return false;
    std::memcpy(&request.checksum,model+8,4);
    std::memcpy(&request.count,model+kOdstRenderModelNodesCountOffset,4);
    AnatomicalPalmMarkers markers{};
    if(!OdstAnatomicalPalmMarkers(request.checksum,int(request.count),markers))return false;
    const uint32_t unit=uint32_t(g_odstPlayerUnitGetter(0));
    int32_t scene=-1,shot=-1;uint32_t weapons[2]{};
    if(!OdstMuzzleTargetStorage(unit)||ReadOdstCinematicControl(scene,shot)!=CinematicControlState::PlayerControlled||
       !OdstReadMuzzleWeapons(unit,weapons))return false;
    // Retained ODSTEK/reload proof: native FP slot's actual weapon must still
    // match the live full-salt inventory owner at the committed palette.
    const auto* tls=OdstContactTls();
    const auto* users=tls?*reinterpret_cast<const uint8_t* const*>(tls+0x598):nullptr;
    if(!users||*reinterpret_cast<const uint32_t*>(users+0x3C)!=weapons[0])return false;
    SupportGripRelationshipSnapshot support{};
    const bool attached=VR_GetSupportGripRelationship(support)&&support.engaged&&
        support.title==GameTitle::Halo3ODST&&support.generation==generation&&
        support.unit==unit&&support.weapon==weapons[0];
    if(!Configure(request,tracking,weapons[1]!=UINT32_MAX,attached,VR_WeaponStowed(GameTitle::Halo3ODST,generation)))
        return false;
    const auto* base=g_odstTagDataBase?static_cast<const uint8_t*>(*g_odstTagDataBase):nullptr;
    uint32_t address=0;std::memcpy(&address,model+kOdstRenderModelNodesDataOffset,4);
    return ReadInverse(base,address,kOdstRenderModelNodeStride,kOdstRenderModelNodeInverseOffset,inverse);
}
// Halo 3: masterchief/fp, checksum 504041493 (anatomical_palette_logic.h), read
// from Halo 3's own loaded render-model tag like the avatar and palm markers.
bool PrepareHalo3(uint16_t tag,const FpInterpolationContext& context,uint32_t generation,
    odst_fingers::Pose& inverse,odst_fingers::Request& request) {
    if(!FingersWanted()||!generation||faultedGeneration[1].load()==generation||
       generation!=g_halo3RuntimeGeneration.load()||!Gameplay(GameTitle::Halo3,context,generation))return false;
    const auto& tracking=g_fpStereoSolveScope.anatomicalTracking;
    if(!TrackingCurrent(tracking)||!g_halo3PlayerUnitGetter||!g_halo3UnitInVehicle)return false;
    const auto* model=Halo3LoadedTagDefinition(tag);if(!model)return false;
    std::memcpy(&request.checksum,model+8,4);
    std::memcpy(&request.count,model+kHalo3RenderModelNodesBlockOffset,4);
    AnatomicalPalmMarkers markers{};
    if(!odst_fingers::PalmMarkers(request.checksum,request.count,markers)||
       odst_fingers::RigTitle(request.checksum)!=GameTitle::Halo3)return false;
    const uint32_t unit=uint32_t(g_halo3PlayerUnitGetter(0));
    int32_t scene=-1,shot=-1;
    if(unit==UINT32_MAX||g_halo3UnitInVehicle(int32_t(unit))||
       ReadCinematicControl(scene,shot)!=CinematicControlState::PlayerControlled)return false;
    uint32_t dual[2]{UINT32_MAX,UINT32_MAX};
    const bool dualWield=Halo3ReadOwnedDualWeapons(unit,dual);
    SupportGripRelationshipSnapshot support{};
    const bool attached=VR_GetSupportGripRelationship(support)&&support.engaged&&
        support.title==GameTitle::Halo3&&support.generation==generation&&support.unit==unit;
    if(!Configure(request,tracking,dualWield,attached,VR_WeaponStowed(GameTitle::Halo3,generation)))return false;
    const auto* base=g_halo3TagDataBase?static_cast<const uint8_t*>(*g_halo3TagDataBase):nullptr;
    uint32_t address=0;std::memcpy(&address,model+kHalo3RenderModelNodesBlockOffset+4,4);
    return ReadInverse(base,address,kHalo3RenderModelNodeStride,kHalo3RenderModelNodeInverseOffset,inverse);
}
void Run(unsigned rig,bool (*prepare)(uint16_t,const FpInterpolationContext&,uint32_t,odst_fingers::Pose&,odst_fingers::Request&),
    uint16_t tag,const FpInterpolationContext& context,BoneMatrix* destination,uint32_t generation) noexcept {
    if(!FingersWanted()||!destination)return;
    odst_fingers::Pose inverse{};odst_fingers::Request request{};
    volatile bool fault=false;bool prepared=false;
    __try {prepared=prepare(tag,context,generation,inverse,request);}
    __except(EXCEPTION_EXECUTE_HANDLER){fault=true;}
    if(fault){faultedGeneration[rig].store(generation);faults[rig].fetch_add(1);return;}
    if(!prepared)return;
    static_assert(sizeof(BoneMatrix)==sizeof(odst_fingers::Matrix));
    const auto result=odst_fingers::Apply(reinterpret_cast<odst_fingers::Matrix*>(destination),inverse,request);
    if(result==odst_fingers::Result::Applied)applications[rig].fetch_add(1,std::memory_order_relaxed);
    else if(result==odst_fingers::Result::Faulted){faultedGeneration[rig].store(generation);faults[rig].fetch_add(1);}
    else if(result==odst_fingers::Result::Refused)refusals[rig].fetch_add(1,std::memory_order_relaxed);
}
}
__declspec(noinline) void OdstApplyControllerFingers(uint16_t tag,const FpInterpolationContext& context,
    BoneMatrix* destination,uint32_t generation) noexcept {
    odst_finger_runtime::Run(0,odst_finger_runtime::Prepare,tag,context,destination,generation);
}
__declspec(noinline) void Halo3ApplyControllerFingers(uint16_t tag,const FpInterpolationContext& context,
    BoneMatrix* destination,uint32_t generation) noexcept {
    odst_finger_runtime::Run(1,odst_finger_runtime::PrepareHalo3,tag,context,destination,generation);
}
void ReportOdstControllerFingers() {
    using namespace odst_finger_runtime;
    if(!FingersWanted())faultedGeneration[0].store(0),faultedGeneration[1].store(0);
    static uint64_t last[2][3]{};static ULONGLONG reportedAt[2]{};
    static const char* const names[2]{"ODST","Halo 3"};
    for(unsigned rig=0;rig<2;++rig) {
        const uint64_t now[]{applications[rig].load(),refusals[rig].load(),faults[rig].load()};const auto clock=GetTickCount64();
        if(std::memcmp(last[rig],now,sizeof(now))&&(clock-reportedAt[rig]>=5000||now[2]!=last[rig][2])) {
            std::memcpy(last[rig],now,sizeof(now));reportedAt[rig]=clock;
            LOG("%s controller fingers: applied=%llu refused=%llu optional-faults=%llu; authored weapon/support grips retained; full-world avatar not implemented",
                names[rig],now[0],now[1],now[2]);
        }
    }
}
