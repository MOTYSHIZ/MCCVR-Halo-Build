// Production first-person transaction tests with explicit native/XR fixtures.
// These verify palette receipt admission and unchanged-output fallback, not
// game draw visibility. Actual native CPU/GPU consumers have a separate test.
#include "../src/dll/haloce_first_person.cpp"
#include <cstdio>

static GameTitle testTitle=GameTitle::HaloCE;
static uint32_t testGeneration=3;
static halo_ce::RenderContext testContext{};
static bool contextValid=true,renderContextValid=true,nativeSawInvalidated=true;
static bool lensFault{},lensRebuild{};
static float lensArgument{};
static unsigned prepareCalls{};
static unsigned quiescenceCalls{},quiescenceRanges{};
static size_t blockedQuiescenceCount{};
GameTitle TitleAdapter_GetActiveTitle() { return testTitle; }
uint32_t TitleAdapter_GetGeneration(GameTitle) { return testGeneration; }
bool HaloCE_Armed() noexcept { return testTitle==GameTitle::HaloCE; }
bool HaloCE_GetRenderContext(const halo_ce::Camera&,halo_ce::RenderContext& result) noexcept
{ result=testContext;return contextValid&&renderContextValid; }
bool HaloCE_GetClassicPrimaryEyeContext(halo_ce::RenderContext& result) noexcept
{ result=testContext;return contextValid&&renderContextValid; }
bool HaloCE_GetGameplayContext(halo_ce::RenderContext& result) noexcept
{ result=testContext;return contextValid; }
bool HaloCE_RenderContextCurrent(const halo_ce::RenderContext& candidate) noexcept
{
    return contextValid&&testTitle==GameTitle::HaloCE&&candidate.tracking.generation==testGeneration&&
        candidate.referenceRevision==testContext.referenceRevision&&candidate.rendererEpoch==testContext.rendererEpoch&&
        candidate.tracking.spaceEpoch==testContext.tracking.spaceEpoch&&
        candidate.tracking.serial&&candidate.tracking.serial<=testContext.tracking.serial&&
        testContext.tracking.serial-candidate.tracking.serial<=8;
}
bool HaloCEControls_GetLocalPlayerState(HaloCELocalPlayerState&) noexcept { return false; }
void Logf(const char*,...) {}
bool WaitForNativeDetourQuiescence(const void* const* functions,const void* const* trampolines,size_t count,
    const std::atomic<uint32_t>& activeCallbacks)
{
    ++quiescenceCalls;quiescenceRanges+=unsigned(count);
    return functions&&trampolines&&count&&count<=8&&count!=blockedQuiescenceCount&&!activeCallbacks.load();
}
void __fastcall NativePrepareFixture(int16_t user)
{
    ++prepareCalls;
    if (!user) nativeSawInvalidated&=!HaloCEFirstPerson_Armed();
}
void __fastcall NativeLensFixture(float fov,bool rebuild)
{
    lensArgument=fov;lensRebuild=rebuild;
    if (lensFault) RaiseException(0xE000CEA1,0,0,nullptr);
}
bool InvokeLensFaultFixture()
{
    __try { ClassicLensHook(.9671381116f,true); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return true; }
    return false;
}
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"CE FP transaction failed at %d: %s\n",__LINE__,#x); return 1; } } while(false)
int main()
{
    using namespace halo_ce;
    installed=true;active=true;retiring=false;generation=testGeneration;
    prepareHook.original=reinterpret_cast<void*>(&NativePrepareFixture);
    testContext.tracking.generation=testGeneration;testContext.tracking.spaceEpoch=4;testContext.tracking.serial=12;
    testContext.referenceRevision=6;testContext.rendererEpoch=8;
    testContext.tracking.controllers.controlsPresentationBlocked=false;
    void* native=VirtualAlloc(nullptr,0x2ea0000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    CHECK(native);moduleBase=reinterpret_cast<uintptr_t>(native);
    int32_t& renderer=*reinterpret_cast<int32_t*>(moduleBase+0x1b7aa84);
    auto publish=[&](uint64_t age=0)
    {
        const uint64_t now=GetTickCount64()-age;
        const bool success=paletteReceipt.Publish({testContext,now});
        lastApplied.store(now,std::memory_order_release);return success;
    };
    CHECK(publish());CHECK(HaloCEFirstPerson_Armed());
    float classicFov=.9671381116f;
    CHECK(ApplyClassicTrackedProjection(classicFov));CHECK(classicFov==-2);
    renderContextValid=false;classicFov=.9671381116f;
    CHECK(!ApplyClassicTrackedProjection(classicFov));CHECK(classicFov==.9671381116f);
    renderContextValid=true;
    ++testContext.tracking.serial;classicFov=.9671381116f;
    CHECK(!ApplyClassicTrackedProjection(classicFov));CHECK(classicFov==.9671381116f);
    --testContext.tracking.serial;
    // A foreign caller preserves the original arguments. A native structured
    // exception still drains the new hook's callback lifetime before unwinding.
    classicLensHook.original=reinterpret_cast<void*>(&NativeLensFixture);
    ClassicLensHook(.9671381116f,true);
    CHECK(lensArgument==.9671381116f&&lensRebuild&&callbacks.load()==0);
    lensFault=true;CHECK(InvokeLensFaultFixture());CHECK(callbacks.load()==0);lensFault=false;
    for (int32_t mode:{1,-1,2})
    {
        renderer=mode;classicFov=.9671381116f;
        CHECK(!ApplyClassicTrackedProjection(classicFov));CHECK(classicFov==.9671381116f);
    }
    renderer=0;
    NodeMatrix source{};source.scale=.3f;
    const SaberBoneMatrix initial{{1,0,0,0, 0,1,0,0, 0,0,1,0, 123,456,789,1}};
    SaberBoneMatrix output=initial;
    CHECK(ScaleConvertedSkin(&source,&output));CHECK(output.value[0]==.3f&&output.value[12]==123);
    std::array<uint8_t,0x40> model{};
    uint32_t flags=0x10000000u;std::memcpy(model.data()+0x28,&flags,sizeof(flags));
    float constants[96]{};for (size_t index=92;index<96;++index) constants[index]=1;
    CHECK(ApplyTrackedProjection(constants,reinterpret_cast<uintptr_t>(model.data())));
    for (size_t index=92;index<96;++index) CHECK(constants[index]==0);
    // Exact material offsets are independently exercised by the native writer
    // emulator. At the production transaction boundary, preserve every other
    // byte and reject partial selectors or blocked presentation atomically.
    for (size_t offset:{size_t(0x170),size_t(0x20),size_t(0x70)})
    {
        for (size_t index=0;index<96;++index) constants[index]=float(index)+.25f;
        for (size_t index=offset/4;index<offset/4+4;++index) constants[index]=1;
        float before[96]{};std::memcpy(before,constants,sizeof(before));
        CHECK(ApplyTrackedProjection(constants,reinterpret_cast<uintptr_t>(model.data()),offset));
        for (size_t index=0;index<96;++index)
            CHECK(constants[index]==((index>=offset/4&&index<offset/4+4)?0:before[index]));
        std::memcpy(constants,before,sizeof(before));constants[offset/4+2]=.5f;
        std::memcpy(before,constants,sizeof(before));
        CHECK(!ApplyTrackedProjection(constants,reinterpret_cast<uintptr_t>(model.data()),offset));
        CHECK(std::memcmp(before,constants,sizeof(before))==0);
    }
    testContext.tracking.controllers.controlsPresentationBlocked=true;CHECK(publish());
    classicFov=.9671381116f;CHECK(!ApplyClassicTrackedProjection(classicFov));CHECK(classicFov==.9671381116f);
    for (size_t index=92;index<96;++index) constants[index]=1;
    CHECK(!ApplyTrackedProjection(constants,reinterpret_cast<uintptr_t>(model.data())));
    for (size_t index=92;index<96;++index) CHECK(constants[index]==1);
    testContext.tracking.controllers.controlsPresentationBlocked=false;

    // A renderer switch or recenter makes the previous palette ineligible,
    // even though a fresh gameplay camera/rig remains available right now.
    for (unsigned fault=0;fault<6;++fault)
    {
        CHECK(publish());
        const RenderContext saved=testContext;
        if (fault==0) ++testContext.rendererEpoch;
        if (fault==1) ++testContext.referenceRevision;
        if (fault==2) ++testContext.tracking.spaceEpoch;
        if (fault==3) testContext.tracking.serial+=9;
        if (fault==4) ++testGeneration;
        if (fault==5) testTitle=GameTitle::Halo3;
        CHECK(!HaloCEFirstPerson_Armed());output=initial;
        classicFov=.9671381116f;CHECK(!ApplyClassicTrackedProjection(classicFov));CHECK(classicFov==.9671381116f);
        CHECK(!ScaleConvertedSkin(&source,&output));CHECK(std::memcmp(&output,&initial,sizeof(output))==0);
        for (size_t index=92;index<96;++index) constants[index]=1;
        CHECK(!ApplyTrackedProjection(constants,reinterpret_cast<uintptr_t>(model.data())));
        for (size_t index=92;index<96;++index) CHECK(constants[index]==1);
        testContext=saved;testGeneration=3;testTitle=GameTitle::HaloCE;
    }
    CHECK(publish(251));CHECK(!HaloCEFirstPerson_Armed());
    CHECK(publish());contextValid=false;CHECK(!HaloCEFirstPerson_Armed());contextValid=true;
    // A native prepare that cannot produce a tracked palette must immediately
    // leave both native skin scale and projection selector untouched.
    CHECK(publish());PrepareHook(0);CHECK(prepareCalls==1&&nativeSawInvalidated);
    CHECK(!HaloCEFirstPerson_Armed());output=initial;
    classicFov=.9671381116f;CHECK(!ApplyClassicTrackedProjection(classicFov));CHECK(classicFov==.9671381116f);
    CHECK(!ScaleConvertedSkin(&source,&output));CHECK(std::memcmp(&output,&initial,sizeof(output))==0);
    CHECK(!ApplyTrackedProjection(constants,reinterpret_cast<uintptr_t>(model.data())));
    for (size_t index=92;index<96;++index) CHECK(constants[index]==1);
    // Other output users cannot invalidate the locally owned user's receipt.
    CHECK(publish());PrepareHook(1);CHECK(HaloCEFirstPerson_Armed()&&prepareCalls==2);
    CHECK(callbacks.load()==0);
    // The shared production stack verifier rejects batches above eight. A
    // blocked second batch must retain the entire native lifetime for retry.
    // No real game hooks/module are needed to verify this admission boundary.
    blockedQuiescenceCount=2;
    CHECK(!Remove());CHECK(retiring.load()&&generation.load()==testGeneration);
    CHECK(prepareHook.original==reinterpret_cast<void*>(&NativePrepareFixture));
    CHECK(quiescenceCalls==2&&quiescenceRanges==10);
    blockedQuiescenceCount=0;quiescenceCalls=quiescenceRanges=0;
    CHECK(Remove());CHECK(!retiring.load()&&!generation.load()&&!prepareHook.original);
    CHECK(quiescenceCalls==2&&quiescenceRanges==10);
    CHECK(VirtualFree(native,0,MEM_RELEASE));
    std::puts("PASS production CE first-person receipt: Classic/Anniversary scale/projection, failed prepare, renderer/recenter/generation/tracking, recovery and ten-hook retirement");
    return 0;
}
