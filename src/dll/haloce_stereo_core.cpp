#include "haloce_stereo_core.h"
#include "haloce_native_bindings.h"
#include "../common/haloce_contracts.generated.h"
#include "../common/haloce_resource_registry.h"
#include "../common/haloce_surface_transfer.h"
#include "../common/log.h"
#include "game.h"
#include "title_adapter.h"
#include "hook_quiescence.h"
#include "../common/minhook_lifecycle.h"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <array>

namespace
{
using namespace halo_ce;
// e17a664 was rejected in the September 14 headset test: 734 prepared
// frames, zero captured pairs, black VR and mismatched stacked desktop views.
// Keep the failed implementation intact for diagnosis, but do not install it.
constexpr bool kRejectedCeInitialStereoEnabled=false;
using PrepareFn=void(__fastcall*)(uintptr_t);
using BuilderFn=uintptr_t(__fastcall*)(uintptr_t,SaberViewPair*,uint8_t,float*);
using FrameFn=void(__fastcall*)(uintptr_t,uint32_t);
using OutputFn=void(__fastcall*)(int);
using TransferFn=uintptr_t(__fastcall*)(uintptr_t,SurfaceTransfer*);
using CreateFn=uintptr_t(__fastcall*)(uintptr_t,uintptr_t);
using ReleaseFn=void(__fastcall*)(uintptr_t);
using CopyFn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT,
    UINT,UINT,UINT,ID3D11Resource*,UINT,const D3D11_BOX*);
struct Hook { void* target{}; void* original{}; bool enabled{}; };
enum HookIndex { Prepare,Builder,Frame,Output,Transfer,Create,Import,Release,ResetList,Copy,Count };
std::array<Hook,Count> hooks;
NativeBindings bindings;
HMODULE moduleReference{};
std::atomic<bool> installed{},active{},armed{},retiring{},trackingEnabled{};
std::atomic<uint32_t> callbacks{},generation{};
std::atomic<uint64_t> firstCameraMs{},lastCameraMs{},lastOwnedMs{},trackingAtMs{};
std::atomic<bool> recenter{true};
std::atomic<uint64_t> referenceRevision{1};
std::atomic<uintptr_t> copyTarget{};
std::atomic<uintptr_t> activeListAddress{};
std::atomic<uint64_t> built{},captured{},dropped{},stock{},descriptorMiss{},previewFolded{};
std::atomic<uint32_t> lastPairStage{0xffffffffu};
uint32_t rejectedGeneration{};
uint64_t lastReport{},resourceEpoch{};
Snapshot<Tracking> trackingSnapshot;
PreparedHandoff handoff;
ResourceRegistry resources;
EyeCache cache;
struct CompletedFrame { EyeCache::Key key; uint64_t referenceRevision{},capturedAtMs{}; };
Snapshot<CompletedFrame> completedFrame;
struct Wanted { D3D11_TEXTURE2D_DESC descriptor{}; uint32_t generation{}; uintptr_t context{}; };
Snapshot<Wanted> wanted,allocated;
Reference reference;
struct Prepared
{
    uintptr_t sourceList{};
    uint32_t generation{};
    bool synthetic{},valid{};
    PreparedReceipt receipt;
    uint64_t referenceRevision{};
};
Snapshot<Prepared> preparedLists[2],renderReady;
std::atomic_flag preparationBusy=ATOMIC_FLAG_INIT;
struct JobScope { uintptr_t job{},activeList{}; bool owned{}; };
thread_local JobScope jobScope;
struct FrameScope
{
    bool synthetic{},capture{};
    int eye{-1};
    EyeCache::Key key;
    Prepared prepared;
    const SurfaceTransfer* transfer{};
    uintptr_t selectedSource{},selectedDestination{};
};
thread_local FrameScope* frameScope{};
struct Callback
{
    Callback() { callbacks.fetch_add(1,std::memory_order_acq_rel); }
    ~Callback() { callbacks.fetch_sub(1,std::memory_order_release); }
};
template<class T> bool Read(uintptr_t address,T& out) noexcept
{
    if (!address) return false;
    __try { std::memcpy(&out,reinterpret_cast<void*>(address),sizeof(T)); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool Commit(SaberViewPair* destination,const StagedViewPair& pair) noexcept
{
    __try
    {
        // Private staging preserves opaque native resource fields. Never run
        // a native camera/list destructor on the private borrowed bytes.
        destination->views[0].camera=pair.cameras[0];
        destination->views[1].camera=pair.cameras[1];
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool Current() noexcept
{
    return installed.load(std::memory_order_acquire)&&active.load(std::memory_order_acquire)&&
        !retiring.load(std::memory_order_acquire)&&
        TitleAdapter_GetActiveTitle()==GameTitle::HaloCE&&
        TitleAdapter_GetGeneration(GameTitle::HaloCE)==generation.load(std::memory_order_acquire);
}
bool Anniversary() noexcept
{
    int mode{};
    return Read(bindings.base+0x1b7aa84,mode)&&mode!=0;
}
bool TrackingNow(Tracking& value) noexcept
{
    const uint64_t stamp=trackingAtMs.load(std::memory_order_acquire),now=GetTickCount64();
    return trackingEnabled.load(std::memory_order_acquire)&&stamp&&now>=stamp&&now-stamp<250&&
        trackingSnapshot.Read(value)&&value.serial&&value.generation==generation.load();
}
bool HasSyntheticLists() noexcept
{
    Prepared value{};
    for (const auto& slot:preparedLists)
        if (!slot.Read(value)||value.synthetic) return true;
    return !renderReady.Read(value)||value.synthetic;
}
bool SingleCamera() noexcept
{
    int count{}; uintptr_t backend{},array{},camera{}; int stereo{}; SaberCamera source{}; Camera checked{};
    return Read(bindings.base+0x2b17b98,count)&&count==1&&
        Read(bindings.base+0x2e3bdd8,backend)&&Read(backend+0x238,stereo)&&stereo==0&&
        Read(bindings.base+0x2b17b90,array)&&Read(array,camera)&&Read(camera,source)&&
        NativeCameraFromSaber(source,checked);
}
bool LiveGame() noexcept
{
    uintptr_t clock{}; uint8_t initialized{}; int32_t tick{};
    return Read(bindings.base+0x2e9fd68,clock)&&Read(clock,initialized)&&initialized==1&&
        Read(clock+0xc,tick)&&tick>0;
}
uintptr_t __fastcall BuilderHook(uintptr_t arg,SaberViewPair* list,uint8_t secondary,float* settings)
{
    Callback callback;
    const auto original=reinterpret_cast<BuilderFn>(hooks[Builder].original);
    const uintptr_t address=reinterpret_cast<uintptr_t>(list);
    const bool scoped=jobScope.owned&&(address==jobScope.activeList||address==jobScope.job+0x70);
    const auto origin=address==jobScope.activeList?PreparationOrigin::ActiveList:PreparationOrigin::CopiedList;
    const size_t slot=static_cast<size_t>(origin);
    const uint32_t gen=generation.load();
    const auto ticket=scoped?handoff.Begin(origin,address,gen):PreparationTicket{};
    if (scoped) preparedLists[slot].Publish({});
    Tracking tracking{};
    const bool eligible=scoped&&Current()&&Anniversary()&&LiveGame()&&SingleCamera()&&!secondary;
    if (eligible)
    {
        const uint64_t now=GetTickCount64();
        const uint64_t last=lastCameraMs.exchange(now,std::memory_order_acq_rel);
        if (!last||now<last||now-last>=500) firstCameraMs.store(now,std::memory_order_release);
        uint64_t zero=0; firstCameraMs.compare_exchange_strong(zero,now);
    }
    const bool force=eligible&&armed.load()&&TrackingNow(tracking);
    const auto nativeResult=original(arg,list,force?1:secondary,settings);
    if (!scoped) return nativeResult;
    Prepared result{address,gen,force,false,{}};
    result.referenceRevision=referenceRevision.load(std::memory_order_acquire);
    if (force)
    {
        SaberViewPair source{},committed{}; StagedViewPair staged{};
        if (recenter.exchange(false)||reference.generation!=gen||reference.spaceEpoch!=tracking.spaceEpoch)
            reference={tracking.headPosition,tracking.headOrientation,tracking.spaceEpoch,gen};
        const auto stage=Read(address,source)
            ? StageBoundNativePair(bindings,source,tracking,reference,
                Game_GetWorldScale(),Game_IsPositionalTracking(),staged)
            : PairStageResult::InvalidNativePair;
        lastPairStage.store(static_cast<uint32_t>(stage),std::memory_order_relaxed);
        if (stage==PairStageResult::Staged&&
            Commit(list,staged)&&Read(address,committed)&&
            handoff.Publish(ticket,tracking,staged,committed)&&
            handoff.Read(origin,address,committed,gen,tracking.spaceEpoch,result.receipt)&&
            result.referenceRevision==referenceRevision.load(std::memory_order_acquire))
        { result.valid=true; built.fetch_add(1,std::memory_order_relaxed); }
        else dropped.fetch_add(1,std::memory_order_relaxed);
    }
    preparedLists[slot].Publish(result);
    return nativeResult;
}
void PrepareBody(uintptr_t job)
{
    const auto original=reinterpret_cast<PrepareFn>(hooks[Prepare].original);
    const bool claimed=!preparationBusy.test_and_set(std::memory_order_acquire);
    const auto previous=jobScope;
    uintptr_t renderer{}; int renderJob{},copyPrepared{};
    const bool scoped=claimed&&Read(bindings.base+0x1bea9e0,renderer)&&renderer&&
        Read(job+0xbe58,renderJob)&&Read(job+0xbe5c,copyPrepared);
    jobScope={job,renderer?renderer+0xb0:0,scoped};
    if (scoped) activeListAddress.store(renderer+0xb0,std::memory_order_release);
    if (scoped&&renderJob) renderReady.Publish({});
    __try { original(job); }
    __finally
    {
        if (scoped&&renderJob)
        {
            Prepared result{}; SaberViewPair rendered{};
            const auto origin=copyPrepared?PreparationOrigin::CopiedList:PreparationOrigin::ActiveList;
            if (preparedLists[static_cast<size_t>(origin)].Read(result)&&
                result.sourceList==(copyPrepared?job+0x70:renderer+0xb0)&&
                result.generation==generation.load())
            {
                if (result.valid)
                    result.valid=Read(renderer+0xb0,rendered)&&
                        handoff.Read(origin,result.sourceList,rendered,result.generation,
                            result.receipt.tracking.spaceEpoch,result.receipt);
                renderReady.Publish(result);
            }
        }
        jobScope=previous;
        if (claimed) preparationBusy.clear(std::memory_order_release);
    }
}
void __fastcall PrepareHook(uintptr_t job) { Callback callback; PrepareBody(job); }
void FrameBody(uintptr_t arg,uint32_t flags)
{
    const auto original=reinterpret_cast<FrameFn>(hooks[Frame].original);
    FrameScope scope{}; uintptr_t renderer{}; SaberViewPair rendered{};
    if (!frameScope) completedFrame.Publish({});
    // Preparation can signal native completion before its wrapper returns.
    // The builder's already-published marker protects the packed GPU copy in
    // that window. Only the fully frozen receipt below permits VR submission.
    if (!frameScope&&HasSyntheticLists()) scope.synthetic=true;
    if (!frameScope&&renderReady.Read(scope.prepared)&&scope.prepared.synthetic&&
        scope.prepared.generation==generation.load()&&
        Read(bindings.base+0x1bea9e0,renderer)&&Read(renderer+0xb0,rendered)&&
        rendered.flags==1&&rendered.count>=2)
    {
        scope.synthetic=true;
        lastOwnedMs.store(GetTickCount64(),std::memory_order_release);
        scope.capture=Current()&&armed.load()&&trackingEnabled.load()&&scope.prepared.valid&&
            scope.prepared.referenceRevision==referenceRevision.load(std::memory_order_acquire)&&
            MatchesPreparedViews(rendered,scope.prepared.receipt)&&cache.Begin(scope.prepared.receipt,scope.key);
    }
    const auto previous=frameScope;
    if (!previous) frameScope=&scope;
    bool returned=false;
    __try { original(arg,flags); returned=true; }
    __finally
    {
        if (!previous)
        {
            if (returned&&scope.capture&&cache.Finish(scope.key))
            { completedFrame.Publish({scope.key,scope.prepared.referenceRevision,GetTickCount64()}); captured.fetch_add(1,std::memory_order_relaxed); }
            else if (scope.synthetic)
            { cache.Drop(scope.key); dropped.fetch_add(1,std::memory_order_relaxed); }
            else stock.fetch_add(1,std::memory_order_relaxed);
            frameScope=previous;
        }
    }
}
void __fastcall FrameHook(uintptr_t arg,uint32_t flags) { Callback callback; FrameBody(arg,flags); }
void OutputBody(int eye)
{
    auto* scope=frameScope; const int previous=scope?scope->eye:-1;
    if (scope&&scope->synthetic&&eye>=0&&eye<2) scope->eye=eye;
    __try { reinterpret_cast<OutputFn>(hooks[Output].original)(eye); }
    __finally { if (scope) scope->eye=previous; }
}
void __fastcall OutputHook(int eye) { Callback callback; OutputBody(eye); }
uintptr_t TransferBody(uintptr_t backend,SurfaceTransfer* request,uintptr_t caller)
{
    auto* scope=frameScope; SurfaceTransfer copy{};
    const bool scoped=scope&&scope->synthetic&&scope->eye>=0&&caller==bindings.base+0x45e376&&
        Read(reinterpret_cast<uintptr_t>(request),copy);
    if (scoped) { scope->transfer=&copy; scope->selectedSource=scope->selectedDestination=0; }
    uintptr_t result{};
    __try { result=reinterpret_cast<TransferFn>(hooks[Transfer].original)(backend,request); }
    __finally { if (scoped) scope->transfer=nullptr; }
    return result;
}
uintptr_t __fastcall TransferHook(uintptr_t backend,SurfaceTransfer* request)
{ Callback callback; return TransferBody(backend,request,reinterpret_cast<uintptr_t>(_ReturnAddress())); }
void CopyBody(ID3D11DeviceContext* context,ID3D11Resource* destination,
    UINT destinationSub,UINT x,UINT y,UINT z,ID3D11Resource* source,UINT sourceSub,const D3D11_BOX* box,
    uintptr_t caller)
{
    const auto original=reinterpret_cast<CopyFn>(hooks[Copy].original);
    auto* scope=frameScope;
    // Actual native source, after variant resolution, inside CE's copy lock.
    if (scope&&scope->transfer&&caller==bindings.base+0x204da0)
    {
        const auto& transfer=*scope->transfer;
        ResourceRegistry::Record src{},dst{};
        const auto sourceId=reinterpret_cast<uintptr_t>(source),destinationId=reinterpret_cast<uintptr_t>(destination);
        const bool sourceKnown=resources.Read(sourceId,sourceId,src);
        const bool destinationKnown=resources.Read(destinationId,destinationId,dst);
        const bool shape=sourceKnown&&box&&sourceSub==0&&destinationSub==0&&x==0&&z==0&&
            IsPrimaryEyeTransfer(transfer,scope->eye,src.descriptor.Width,src.descriptor.Height)&&
            box->left==0&&box->top==0&&box->front==0&&box->back==1&&
            box->right==src.descriptor.Width&&box->bottom==src.descriptor.Height&&
            y==static_cast<UINT>(transfer.destinationY);
        if (shape)
        {
            wanted.Publish({src.descriptor,generation.load(),reinterpret_cast<uintptr_t>(context)});
            if (scope->capture)
            {
                const auto& camera=scope->prepared.receipt.pair.cameras[scope->eye];
                if (camera.viewportWidth!=src.descriptor.Width||camera.viewportHeight!=src.descriptor.Height)
                    scope->capture=false;
            }
            if (scope->capture&&!cache.Capture(scope->key,scope->eye,context,source,src.descriptor)) scope->capture=false;
        }
        else { scope->capture=false; descriptorMiss.fetch_add(1,std::memory_order_relaxed); }
        // A forced second full-size view can exceed the stock packed target.
        // Preserve native bookkeeping and a bounded desktop preview. Unknown
        // descriptors never authorize this potentially out-of-bounds GPU call.
        if (!shape||!destinationKnown||dst.descriptor.Width<src.descriptor.Width||
            dst.descriptor.Height<src.descriptor.Height||dst.descriptor.ArraySize!=1||
            dst.descriptor.MipLevels!=1||dst.descriptor.SampleDesc.Count!=1||
            src.descriptor.SampleDesc.Count!=1||src.descriptor.SampleDesc.Quality!=0||
            src.descriptor.MipLevels!=1||src.descriptor.ArraySize!=1||
            dst.descriptor.SampleDesc.Quality!=0||dst.descriptor.Format!=src.descriptor.Format)
        { scope->capture=false; return; }
        if (y>dst.descriptor.Height-src.descriptor.Height)
        { y=0; previewFolded.fetch_add(1,std::memory_order_relaxed); }
    }
    original(context,destination,destinationSub,x,y,z,source,sourceSub,box);
}
void STDMETHODCALLTYPE CopyHook(ID3D11DeviceContext* context,ID3D11Resource* destination,
    UINT destinationSub,UINT x,UINT y,UINT z,ID3D11Resource* source,UINT sourceSub,const D3D11_BOX* box)
{
    Callback callback;
    CopyBody(context,destination,destinationSub,x,y,z,source,sourceSub,box,
        reinterpret_cast<uintptr_t>(_ReturnAddress()));
}

// Resource construction/import are cold native management scopes. Render
// hooks use these immutable CPU descriptors and issue only the GPU copy.
void RevokeWrappedResource(uintptr_t wrapper) noexcept
{
    uintptr_t resource{};
    if (Read(wrapper+0xe0,resource)&&resource) resources.Forget(resource);
}
void RecordResource(uintptr_t wrapper) noexcept
{
    ID3D11Resource* resource{};
    if (!Read(wrapper+0xe0,resource)||!resource) return;
    ID3D11Texture2D* texture{};
    __try
    {
        if (SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&texture)))&&texture)
        {
            D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc);
            HaloCE_RecordTextureCreated(texture,desc);
            texture->Release();
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { }
}
uintptr_t __fastcall CreateHook(uintptr_t wrapper,uintptr_t data)
{
    Callback callback; RevokeWrappedResource(wrapper);
    const auto result=reinterpret_cast<CreateFn>(hooks[Create].original)(wrapper,data);
    RecordResource(wrapper); return result;
}
uintptr_t __fastcall ImportHook(uintptr_t wrapper,uintptr_t resource)
{
    Callback callback; RevokeWrappedResource(wrapper);
    const auto result=reinterpret_cast<CreateFn>(hooks[Import].original)(wrapper,resource);
    RecordResource(wrapper); return result;
}
void __fastcall ReleaseHook(uintptr_t wrapper)
{
    Callback callback; RevokeWrappedResource(wrapper);
    reinterpret_cast<ReleaseFn>(hooks[Release].original)(wrapper);
}
uintptr_t __fastcall ResetListHook(uintptr_t list)
{
    Callback callback;
    // Native reset owns the list and destroys its native resources. This is
    // also the retirement barrier for a manufactured secondary view.
    for (size_t index=0;index<2;++index)
    {
        Prepared previous{};
        if (preparedLists[index].Read(previous)&&previous.sourceList==list)
        {
            handoff.Invalidate(static_cast<PreparationOrigin>(index));
            preparedLists[index].Publish({});
        }
    }
    if (activeListAddress.load(std::memory_order_acquire)==list) renderReady.Publish({});
    return reinterpret_cast<uintptr_t(__fastcall*)(uintptr_t)>(hooks[ResetList].original)(list);
}
bool Remove() noexcept
{
    retiring.store(true,std::memory_order_release);
    // Keep copy protection installed until native reset/stock preparation has
    // retired every manufactured list. An inactive title may retain these
    // dormant hooks until its next reset; never remove them under a queued eye.
    if (installed.load()&&(callbacks.load()||HasSyntheticLists())) return false;
    for (auto& hook:hooks) if (hook.enabled)
    {
        const auto status=MCCVR_DisableHookForRetirement(hook.target);
        if (status!=MH_OK&&status!=MH_ERROR_DISABLED) return false;
        hook.enabled=false;
    }
    if (callbacks.load(std::memory_order_acquire)) return false;
    const void* functions[Count]={reinterpret_cast<void*>(&PrepareHook),reinterpret_cast<void*>(&BuilderHook),
        reinterpret_cast<void*>(&FrameHook),reinterpret_cast<void*>(&OutputHook),reinterpret_cast<void*>(&TransferHook),
        reinterpret_cast<void*>(&CreateHook),reinterpret_cast<void*>(&ImportHook),reinterpret_cast<void*>(&ReleaseHook),
        reinterpret_cast<void*>(&ResetListHook),reinterpret_cast<void*>(&CopyHook)};
    const void* originals[Count]{};
    for (size_t i=0;i<Count;++i) originals[i]=hooks[i].original;
    // The shared verifier accepts at most eight ranges. Entries are disabled,
    // so independently checking the remaining roots cannot admit new callers.
    if (!WaitForNativeDetourQuiescence(functions,originals,8,callbacks)||
        !WaitForNativeDetourQuiescence(functions+8,originals+8,Count-8,callbacks)) return false;
    for (auto& hook:hooks) if (hook.target)
    {
        const auto status=MH_RemoveHook(hook.target);
        if (status!=MH_OK&&status!=MH_ERROR_NOT_CREATED) return false;
        hook={};
    }
    if (!cache.Reset()) return false;
    resources.InvalidateAll();
    if (moduleReference) { FreeLibrary(moduleReference); moduleReference=nullptr; }
    bindings={}; installed=false; generation=0; allocated.Publish({});
    completedFrame.Publish({}); renderReady.Publish({}); wanted.Publish({});
    preparedLists[0].Publish({}); preparedLists[1].Publish({});
    handoff.Invalidate(PreparationOrigin::ActiveList); handoff.Invalidate(PreparationOrigin::CopiedList);
    firstCameraMs=0; lastCameraMs=0; lastOwnedMs=0; activeListAddress=0; recenter=true;
    return true;
}
bool Install(uintptr_t base,size_t size,uint32_t gen) noexcept
{
    const char* failure="module retention";
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(base),&moduleReference)) return false;
    if (!ResolveNativeBindings(base,size,gen,bindings,failure))
    { LOG("CE core stock fallback: %s",failure); Remove(); return false; }
    generation=gen; retiring=false;
    preparedLists[0].Publish({}); preparedLists[1].Publish({}); renderReady.Publish({});
    const uintptr_t addresses[Count]={bindings.prepare,bindings.pairBuilder,bindings.frame,
        bindings.output,bindings.transfer,
        base+contract::anniversary_texture_create,base+contract::anniversary_texture_import_2d,
        base+contract::anniversary_texture_release_resources,base+contract::anniversary_view_list_reset,copyTarget.load()};
    void* detours[Count]={reinterpret_cast<void*>(&PrepareHook),reinterpret_cast<void*>(&BuilderHook),
        reinterpret_cast<void*>(&FrameHook),reinterpret_cast<void*>(&OutputHook),reinterpret_cast<void*>(&TransferHook),
        reinterpret_cast<void*>(&CreateHook),reinterpret_cast<void*>(&ImportHook),
        reinterpret_cast<void*>(&ReleaseHook),reinterpret_cast<void*>(&ResetListHook),reinterpret_cast<void*>(&CopyHook)};
    for (size_t i=0;i<Count;++i)
    {
        if (!addresses[i]) { Remove(); return false; }
        auto& hook=hooks[i]; void* target=reinterpret_cast<void*>(addresses[i]);
        const auto status=MH_CreateHook(target,detours[i],&hook.original);
        if (status!=MH_OK)
        { LOG("CE core stock fallback: create hook %zu status %d",i,status); Remove(); return false; }
        hook.target=target;
    }
    for (auto& hook:hooks)
    {
        const auto status=MH_EnableHook(hook.target);
        if (status!=MH_OK) { LOG("CE core stock fallback: enable status %d",status); Remove(); return false; }
        hook.enabled=true;
    }
    installed=true;
    LOG("CE core installed: native two-view preparation, source lifetime and GPU output hooks; waiting for fresh Anniversary camera");
    return true;
}
}

bool HaloCE_Poll(uintptr_t base,size_t size,uint32_t gen,bool isActive) noexcept
{
    active.store(isActive,std::memory_order_release);
    if (moduleReference&&(!isActive||gen!=generation.load()||base!=bindings.base||retiring.load()))
    {
        if (armed.exchange(false)) LOG("CE core disarmed by HaloCE_Poll: title/generation retirement");
        TitleAdapter_PublishLifecycle(GameTitle::HaloCE,generation.load(),{installed.load(),false,true,0});
        if (!Remove()) return false;
    }
    if (!isActive||!base||!gen) return false;
    if (!kRejectedCeInitialStereoEnabled)
    {
        if (gen!=rejectedGeneration)
        {
            LOG("CE core disabled by HaloCE_Poll: e17a664 stereo rejected by headset test; controller input and graphics gesture retained");
            rejectedGeneration=gen;
        }
        TitleAdapter_PublishLifecycle(GameTitle::HaloCE,gen,{false,false,false,0});
        return false;
    }
    if (!installed.load()&&gen!=rejectedGeneration&&copyTarget.load())
        if (!Install(base,size,gen)) rejectedGeneration=gen;
    const uint64_t now=GetTickCount64(),first=firstCameraMs.load(),last=lastCameraMs.load();
    const bool fresh=last&&now>=last&&now-last<500;
    if (installed.load()&&fresh&&first&&now-first>=1000&&!armed.exchange(true))
        LOG("CE core armed: Anniversary stereo and positional 6DoF; Classic, tracked weapons, HUD extraction, melee and world collision remain stock/deferred");
    if (!fresh&&armed.exchange(false))
    { recenter=true; LOG("CE core disarmed by HaloCE_Poll: camera heartbeat expired; hooks retained for re-entry"); firstCameraMs=0; }
    constexpr uint32_t capabilities=TitleCapability_Stereo|TitleCapability_RoomScale|
        TitleCapability_ControllerInput|TitleCapability_RuntimeModes;
    TitleAdapter_PublishLifecycle(GameTitle::HaloCE,gen,{installed.load(),armed.load(),retiring.load(),armed.load()?capabilities:0});
    if (fresh) TitleAdapter_PublishHeartbeat(GameTitle::HaloCE,gen,last);
    if (now-lastReport>=2000)
    {
        lastReport=now;
        LOG("CE DIAG gen=%u installed=%d armed=%d built=%llu pairs=%llu dropped=%llu stock=%llu descriptorMiss=%llu previewFolded=%llu stage=%u",
            gen,installed.load(),armed.load(),built.load(),captured.load(),dropped.load(),stock.load(),descriptorMiss.load(),previewFolded.load(),lastPairStage.load());
    }
    return armed.load();
}
bool HaloCE_Armed() noexcept { return Current()&&armed.load(std::memory_order_acquire); }
void HaloCE_Recenter() noexcept
{
    referenceRevision.fetch_add(1,std::memory_order_acq_rel);
    recenter=true;
    completedFrame.Publish({});
}
void HaloCE_PublishTracking(const halo_ce::Tracking& tracking,bool enabled) noexcept
{
    trackingEnabled.store(false,std::memory_order_release);
    if (enabled&&trackingSnapshot.Publish(tracking))
    { trackingAtMs.store(GetTickCount64(),std::memory_order_release); trackingEnabled.store(true,std::memory_order_release); }
}
void HaloCE_PresentResources(ID3D11Device* device,ID3D11DeviceContext* context) noexcept
{
    if (!device||!context) return;
    copyTarget.store(reinterpret_cast<uintptr_t>((*reinterpret_cast<void***>(context))[46]),std::memory_order_release);
    if (!HaloCE_Armed()) return;
    Wanted next{};
    if (!wanted.Read(next)||!next.generation||next.generation!=generation.load()||
        next.context!=reinterpret_cast<uintptr_t>(context)) return;
    Wanted previous{};
    if (allocated.Read(previous)&&previous.generation==next.generation&&previous.context==next.context&&
        std::memcmp(&previous.descriptor,&next.descriptor,sizeof(next.descriptor))==0) return;
    if (cache.Prepare(device,context,next.descriptor,next.generation,++resourceEpoch))
    {
        allocated.Publish(next);
        LOG("CE eye caches prepared: %ux%u format=%u generation=%u resource=%llu",
            next.descriptor.Width,next.descriptor.Height,next.descriptor.Format,next.generation,resourceEpoch);
    }
}
bool HaloCE_AcquirePair(ID3D11DeviceContext* context,uint64_t currentSerial,uint64_t spaceEpoch,
    halo_ce::EyeCache::Completed& pair) noexcept
{
    CompletedFrame frame{};
    const uint64_t now=GetTickCount64();
    if (!HaloCE_Armed()||recenter.load()||!completedFrame.Read(frame)||
        frame.referenceRevision!=referenceRevision.load(std::memory_order_acquire)||
        !frame.capturedAtMs||now<frame.capturedAtMs||now-frame.capturedAtMs>=250) return false;
    const auto key=frame.key;
    if (!key.serial||
        key.generation!=generation.load()||key.spaceEpoch!=spaceEpoch||
        key.serial>currentSerial||currentSerial-key.serial>8) return false;
    return cache.AcquireCompleted(key,context,pair);
}
void HaloCE_ReleasePair(uint64_t borrowId) noexcept { cache.ReleaseCompleted(borrowId); }
bool HaloCE_OwnsPresentation() noexcept
{
    const uint64_t last=lastOwnedMs.load(std::memory_order_acquire),now=GetTickCount64();
    return Current()&&last&&now>=last&&now-last<500;
}
void HaloCE_RecordTextureCreated(ID3D11Texture2D* texture,
    const D3D11_TEXTURE2D_DESC& descriptor) noexcept
{
    const auto identity=reinterpret_cast<uintptr_t>(texture);
    resources.Forget(identity);
    if (!identity||!(descriptor.BindFlags&D3D11_BIND_RENDER_TARGET)) return;
    const auto revision=resources.Revoke(identity);
    resources.Publish(identity,identity,revision,descriptor);
}
