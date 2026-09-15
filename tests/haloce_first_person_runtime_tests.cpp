// Production first-person transaction tests with explicit native/XR fixtures.
// These verify palette receipt admission and unchanged-output fallback, not
// game draw visibility. Actual native CPU/GPU consumers have a separate test.
#include "../src/dll/haloce_first_person.cpp"
#include <cstdio>

static GameTitle testTitle=GameTitle::HaloCE;
static uint32_t testGeneration=3;
static halo_ce::RenderContext testContext{};
static halo_ce::RenderContext anniversaryEyeContext{};
static bool anniversaryEyeValid=true;
static bool anniversaryPrimaryValid=true;
static halo_ce::SaberCamera particleCamera{};
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
bool HaloCE_GetAnniversaryPrimaryEyeTracking(halo_ce::Tracking& tracking) noexcept
{
    tracking={};
    if (!anniversaryPrimaryValid||!anniversaryEyeValid||!renderContextValid||!HaloCE_RenderContextCurrent(anniversaryEyeContext)) return false;
    tracking=anniversaryEyeContext.tracking;return true;
}
bool HaloCE_GetAnniversaryEyeTracking(const halo_ce::SaberCamera* camera,halo_ce::Tracking& tracking) noexcept
{
    tracking={};
    if (camera!=&particleCamera||!anniversaryEyeValid||!renderContextValid||!HaloCE_RenderContextCurrent(anniversaryEyeContext)) return false;
    tracking=anniversaryEyeContext.tracking;return true;
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
static ParticleBuffer* particleSource{};
static std::array<float,4096*4> particleBacking{},particleBaseline{},particleUploaded{};
static bool particleNativeChecks=true,particleExpectCorrection{},particleThrow{},particleMutate{},particleNested{},particleRevoke{};
static unsigned particleNativeCalls{};
void __fastcall NativeParticleCommitFixture(uintptr_t address,uintptr_t backend,bool immediate)
{
    ++particleNativeCalls;
    auto& buffer=*reinterpret_cast<ParticleBuffer*>(address);
    particleNativeChecks&=backend==123&&immediate&&buffer.gpu==particleSource->gpu;
    particleNativeChecks&=std::memcmp(particleBacking.data(),particleBaseline.data(),sizeof(particleBacking))==0;
    particleNativeChecks&=particleExpectCorrection?(address!=reinterpret_cast<uintptr_t>(particleSource)&&buffer.data!=particleSource->data):address==reinterpret_cast<uintptr_t>(particleSource);
    if (buffer.capacity>0&&buffer.capacity<=4096)
        std::memcpy(particleUploaded.data(),buffer.data,size_t(buffer.capacity)*16);
    if (particleMutate) { particleSource->first=7;particleSource->end=11; }
    if (particleThrow) RaiseException(0xE000CEA2,0,0,nullptr);
    buffer.first=buffer.capacity;buffer.end=0;
}
void __fastcall NativeParticleDrawFixture(const halo_ce::SaberCamera* camera,uintptr_t texture,int pass,uintptr_t batch)
{
    particleNativeChecks&=texture==55&&pass==2&&batch==66;
    const auto* outer=particleScope;
    if (particleNested)
    {
        particleNested=false;particleExpectCorrection=false;
        halo_ce::SaberCamera auxiliary{};
        // The auxiliary draw masks the outer scope even though its tracking
        // serial would otherwise be the same. Restore the native dirty range.
        const auto before=*particleSource;
        ParticleDrawHook(&auxiliary,55,2,66);
        *particleSource=before;
        particleExpectCorrection=true;
        particleNativeChecks&=particleScope==outer;
    }
    if (particleRevoke) anniversaryPrimaryValid=false;
    CommitParticleProjection(reinterpret_cast<uintptr_t>(particleSource),123,true,true);
}
bool InvokeParticleFaultFixture()
{
    __try { ParticleDrawHook(&particleCamera,55,2,66); }
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
        anniversaryEyeContext=testContext;
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
    // Native Anniversary skinning owns a copied bone array. The next native
    // prepare can invalidate the global palette receipt between ANY two bone
    // conversions without changing this array. Its self-contained scale must
    // apply consistently to all bones, including hidden forearms.
    for (float scale:{.00001f,.3f,1.0f,3.0f})
    {
        source.scale=scale;CHECK(publish());
        for (unsigned bone=0;bone<8;++bone)
        {
            if (bone==3) lastApplied.store(0,std::memory_order_release);
            output=initial;
            CHECK(ScaleConvertedSkin(&source,&output));
            CHECK(output.value[0]==scale&&output.value[5]==scale&&output.value[10]==scale);
            CHECK(output.value[12]==123&&output.value[13]==456&&output.value[14]==789&&output.value[15]==1);
            if (scale==1) CHECK(std::memcmp(&output,&initial,sizeof(output))==0);
        }
    }
    source.scale=.3f;CHECK(publish());
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
    // The next CPU palette rebuild cannot switch any current-eye material
    // back to the fixed weapon lens between depth/color/effect consumers.
    CHECK(publish());lastApplied.store(0,std::memory_order_release);
    CHECK(!HaloCEFirstPerson_Armed());
    for (size_t offset:{size_t(0x170),size_t(0x20),size_t(0x70)})
    {
        for (size_t index=offset/4;index<offset/4+4;++index) constants[index]=1;
        CHECK(ApplyTrackedProjection(constants,reinterpret_cast<uintptr_t>(model.data()),offset));
        for (size_t index=offset/4;index<offset/4+4;++index) CHECK(constants[index]==0);
    }
    anniversaryEyeValid=false;
    for (size_t index=92;index<96;++index) constants[index]=1;
    CHECK(!ApplyTrackedProjection(constants,reinterpret_cast<uintptr_t>(model.data())));
    for (size_t index=92;index<96;++index) CHECK(constants[index]==1);
    anniversaryEyeValid=true;
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
        // A copied source matrix retains its scale across frame/reference
        // churn. Title/feature lifetime changes still reject the consumer.
        CHECK(ScaleConvertedSkin(&source,&output)==(fault<4));
        if (fault<4) CHECK(output.value[0]==source.scale&&output.value[12]==123);
        else CHECK(std::memcmp(&output,&initial,sizeof(output))==0);
        for (size_t index=92;index<96;++index) constants[index]=1;
        CHECK(!ApplyTrackedProjection(constants,reinterpret_cast<uintptr_t>(model.data())));
        for (size_t index=92;index<96;++index) CHECK(constants[index]==1);
        testContext=saved;testGeneration=3;testTitle=GameTitle::HaloCE;
    }
    CHECK(publish(251));CHECK(!HaloCEFirstPerson_Armed());
    CHECK(publish());contextValid=false;CHECK(!HaloCEFirstPerson_Armed());contextValid=true;
    // Failed new preparation cannot claim a Classic tracked-palette receipt.
    // A current Anniversary draw still owns its own world lens; a copied bone
    // retains its explicit scale, and stock scale one stays byte-identical.
    CHECK(publish());PrepareHook(0);CHECK(prepareCalls==1&&nativeSawInvalidated);
    CHECK(!HaloCEFirstPerson_Armed());output=initial;
    classicFov=.9671381116f;CHECK(!ApplyClassicTrackedProjection(classicFov));CHECK(classicFov==.9671381116f);
    CHECK(ScaleConvertedSkin(&source,&output));CHECK(output.value[0]==source.scale);
    source.scale=1;output=initial;
    CHECK(ScaleConvertedSkin(&source,&output));CHECK(std::memcmp(&output,&initial,sizeof(output))==0);
    CHECK(ApplyTrackedProjection(constants,reinterpret_cast<uintptr_t>(model.data())));
    for (size_t index=92;index<96;++index) CHECK(constants[index]==0);
    anniversaryEyeValid=false;
    for (size_t index=92;index<96;++index) constants[index]=1;
    CHECK(!ApplyTrackedProjection(constants,reinterpret_cast<uintptr_t>(model.data())));
    for (size_t index=92;index<96;++index) CHECK(constants[index]==1);
    anniversaryEyeValid=true;
    // Other output users cannot invalidate the locally owned user's receipt.
    CHECK(publish());PrepareHook(1);CHECK(HaloCEFirstPerson_Armed()&&prepareCalls==2);
    CHECK(callbacks.load()==0);
    // The shared production stack verifier rejects batches above eight. A
    // blocked second batch must retain the entire native lifetime for retry.
    // No real game hooks/module are needed to verify this admission boundary.
    // Actual production particle draw + upload hooks own a private full-capacity
    // copy, with only FP lens selectors changed. Source emitters/backing stay
    // exact through partial offsets, exceptions and nested auxiliary draws.
    CHECK(publish());renderContextValid=true;anniversaryEyeValid=true;anniversaryPrimaryValid=true;
    particleProjectionInstalled=true;
    particleDrawHook.original=reinterpret_cast<void*>(&NativeParticleDrawFixture);
    particleCommitHook.original=reinterpret_cast<void*>(&NativeParticleCommitFixture);
    for (const auto shape: {std::pair{0,201},std::pair{117,4096}})
    {
        const int first=shape.first,capacity=shape.second;
        particleBacking.fill(77);
        for (size_t emitter=0;emitter<9;++emitter)
            particleBacking[size_t(first)*4+(21+emitter*20+10)*4]=emitter%2?0.0f:1.0f;
        particleBaseline=particleBacking;
        ParticleBuffer nativeBuffer{moduleBase+0x17f8c78,first,first+201,capacity,0,particleBacking.data(),0,789};
        particleSource=&nativeBuffer;particleExpectCorrection=true;
        ParticleDrawHook(&particleCamera,55,2,66);
        CHECK(particleNativeChecks&&callbacks.load()==0&&!particleScope);
        CHECK(nativeBuffer.first==capacity&&nativeBuffer.end==0);
        for (size_t index=0;index<size_t(capacity)*4;++index)
        {
            bool selector=false;
            for (size_t emitter=0;emitter<9;++emitter)
                selector|=index==size_t(first)*4+(21+emitter*20+10)*4;
            CHECK(particleUploaded[index]==(selector?0.0f:particleBaseline[index]));
        }
        CHECK(particleBacking==particleBaseline);
        nativeBuffer.first=first;nativeBuffer.end=first+201;
        particleThrow=true;const ParticleBuffer before=nativeBuffer;
        CHECK(InvokeParticleFaultFixture());particleThrow=false;
        CHECK(particleNativeChecks&&!std::memcmp(&nativeBuffer,&before,sizeof(before)));
        CHECK(particleBacking==particleBaseline&&callbacks.load()==0&&!particleScope);
        particleNested=true;ParticleDrawHook(&particleCamera,55,2,66);
        CHECK(particleNativeChecks&&!particleScope&&callbacks.load()==0);
        nativeBuffer=before;particleMutate=true;
        ParticleDrawHook(&particleCamera,55,2,66);particleMutate=false;
        CHECK(nativeBuffer.first==7&&nativeBuffer.end==11&&particleBacking==particleBaseline);
        nativeBuffer=before;particleRevoke=true;particleExpectCorrection=false;
        ParticleDrawHook(&particleCamera,55,2,66);particleRevoke=false;anniversaryPrimaryValid=true;
        CHECK(particleNativeChecks&&particleBacking==particleBaseline);
        nativeBuffer=before;
        anniversaryPrimaryValid=false;
        const auto observedDraws=particleDrawObserved.load(),refusedEyes=particleEyeRefused.load();
        ParticleDrawHook(&particleCamera,55,2,66);anniversaryPrimaryValid=true;
        CHECK(particleNativeChecks&&particleBacking==particleBaseline);
        CHECK(particleDrawObserved.load()==observedDraws+1&&particleEyeRefused.load()==refusedEyes+1);
        nativeBuffer=before;nativeBuffer.gpu=0;
        ParticleDrawHook(&particleCamera,55,2,66);
        CHECK(particleNativeChecks&&particleBacking==particleBaseline);
        nativeBuffer=before;nativeBuffer.first=capacity+1;
        ParticleDrawHook(&particleCamera,55,2,66);
        CHECK(particleNativeChecks&&particleBacking==particleBaseline);
        nativeBuffer=before;
        particleBacking[size_t(first)*4+31*4]=.5f;particleBaseline=particleBacking;
        ParticleDrawHook(&particleCamera,55,2,66);
        CHECK(particleNativeChecks&&particleBacking==particleBaseline);
        nativeBuffer=before;nativeBuffer.first=INT_MAX;nativeBuffer.end=INT_MIN;
        // A malformed range falls through unchanged to this explicit fake
        // native service; no arithmetic overflow or optional feature write.
        ParticleDrawHook(&particleCamera,55,2,66);
        CHECK(particleNativeChecks&&particleBacking==particleBaseline);
    }
    CHECK(particleNativeCalls>=14);
    blockedQuiescenceCount=4;
    CHECK(!Remove());CHECK(retiring.load()&&generation.load()==testGeneration);
    CHECK(prepareHook.original==reinterpret_cast<void*>(&NativePrepareFixture));
    CHECK(quiescenceCalls==2&&quiescenceRanges==12);
    blockedQuiescenceCount=0;quiescenceCalls=quiescenceRanges=0;
    CHECK(Remove());CHECK(!retiring.load()&&!generation.load()&&!prepareHook.original);
    CHECK(quiescenceCalls==2&&quiescenceRanges==12);
    CHECK(VirtualFree(native,0,MEM_RELEASE));
    std::puts("PASS production CE first-person receipt: Classic/Anniversary scale/projection, isolated particle uploads, nested/exception admission, recovery and twelve-hook retirement");
    return 0;
}
