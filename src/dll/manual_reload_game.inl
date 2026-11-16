// MANUAL RELOAD, the game's side (src/common/manual_reload_logic.h, manual_reload_publication.h). Runs in the
// first-person palette hook, a game call: in Project Reclaimer the game's thread-local data exists only there.
//
// READS ONLY, plus the local first-person palette (multiplayer safety, AGENTS.md): the held gun's magazine
// record is read, never written; the gun's own magazine part is hidden and its rack part drawn back by editing
// this frame's private palette scratch, which no other player sees.
//
// Magazine records (docs/MANUAL-RELOAD-PORT-2026-11-15.md, pinned retail halo3.dll b209d845 / halo3odst.dll
// 5bb20976): the weapon datum's magazines at +0x230 (ODST +0x228), 0x18 apart: +0x00 state (1-3 reloading),
// +0x06 rounds inventory, +0x0A rounds loaded. The weapon tag's magazines block (count +0x424, address +0x428;
// ODST +0x434 / +0x438), 0x80 apart: +0x0A rounds loaded maximum. Halo 3: 0x35F174 recharges +0x0A up to the tag's
// +0x0A; 0x3621F2-0x3623D3 (pickup) caps +0x06 + +0x0A at the tag total and adds to +0x06. ODST: 0x3A5340 the same
// with +0x232 / tag +0x438.
namespace manual_reload_game
{
std::atomic<uint64_t> samples{0}, ammoFailures{0}, hidden{0}, posed{0}, faults{0};
// The gun's layout in the aim frame, for the log (a seqlock: the hook writes, the worker reads).
struct Layout_ { float root[3], fwd[3], left[3], up[3], magNode[3], rackNode[3], well[3]; uint64_t identity; };
Layout_ g_layout{};
std::atomic<uint32_t> g_layoutSeq{0};

struct Layout { size_t magazineBase, tagCount, tagAddress; };
inline constexpr Layout kHalo3{0x230, 0x424, 0x428}, kOdst{0x228, 0x434, 0x438};

bool ReadAmmo(GameTitle title, uint32_t& weapon, reload_ammo::Sim& sim)
{
    weapon = 0;
    sim = {};
    const bool odst = title == GameTitle::Halo3ODST;
    const Layout& layout = odst ? kOdst : kHalo3;
    uint32_t unit = UINT32_MAX, weapons[2]{UINT32_MAX, UINT32_MAX};
    const uint8_t* object = nullptr;
    const uint8_t* definition = nullptr;
    const uint8_t* tagBase = nullptr;
    if (odst)
    {
        if (!g_odstPlayerUnitGetter || !g_odstUnitInVehicle) return false;
        unit = uint32_t(g_odstPlayerUnitGetter(0));
        if (unit == UINT32_MAX || g_odstUnitInVehicle(int32_t(unit)) || !OdstReadMuzzleWeapons(unit, weapons) ||
            weapons[0] == UINT32_MAX || weapons[1] != UINT32_MAX) return false;
        object = OdstMuzzleObject(weapons[0], 2);
        definition = object ? OdstLoadedTagDefinition(*reinterpret_cast<const uint32_t*>(object)) : nullptr;
        tagBase = g_odstTagDataBase ? static_cast<const uint8_t*>(*g_odstTagDataBase) : nullptr;
    }
    else
    {
        if (!g_halo3PlayerUnitGetter || !g_halo3UnitInVehicle) return false;
        unit = uint32_t(g_halo3PlayerUnitGetter(0));
        if (unit == UINT32_MAX || g_halo3UnitInVehicle(int32_t(unit)) ||
            !Halo3ReadOwnedWeapons(unit, weapons, false) || weapons[0] == UINT32_MAX ||
            weapons[1] != UINT32_MAX) return false;
        object = Halo3MeleeSelectionObject(weapons[0], 2);
        definition = object ? Halo3LoadedTagDefinition(*reinterpret_cast<const uint32_t*>(object)) : nullptr;
        tagBase = g_halo3TagDataBase ? static_cast<const uint8_t*>(*g_halo3TagDataBase) : nullptr;
    }
    if (!object || !definition || !tagBase) return false;
    const int32_t count = *reinterpret_cast<const int32_t*>(definition + layout.tagCount);
    const uint32_t address = *reinterpret_cast<const uint32_t*>(definition + layout.tagAddress);
    if (count < 1 || count > 2 || !address) return false;
    const uint8_t* magazine = tagBase + size_t(address) * 4;
    const int16_t capacity = *reinterpret_cast<const int16_t*>(magazine + 0x0A);
    const uint8_t* record = object + layout.magazineBase;
    const int16_t state = *reinterpret_cast<const int16_t*>(record + 0x00);
    const int16_t reserve = *reinterpret_cast<const int16_t*>(record + 0x06);
    const int16_t loaded = *reinterpret_cast<const int16_t*>(record + 0x0A);
    if (capacity < 1 || capacity > 999 || loaded < 0 || loaded > capacity || reserve < 0 || reserve > 9999) return false;
    weapon = weapons[0];
    sim = {loaded, reserve, capacity, state >= 1 && state <= 3};
    return true;
}

// THE GUN'S LOOK WHILE THE MANUAL RELOAD HAS IT (reload_clip_logic.h). The game reloads at the drop, unseen:
// while it does, the gun is drawn in its resting pose (held on the hand where it rests, every part where it rests),
// and the game's own reload animation is recorded off this frame's palette into a clip, once per gun and kind
// (reload_clip_store). From then on the gun is posed from that clip: the magazine's way out plays at the drop,
// its way in is scrubbed by how far the hand has pushed, and the rack part's stroke by the hand's pull.
// The hook's thread only; allocation-free.
struct GunState
{
    uint64_t identity = 0;
    uint32_t weapon = 0;
    bool idleValid = false;
    reload_clip::Bone idleRel[reload_clip::kMaxNodes]{};
    BoneMatrix idleOnWrist{};         // the gun's root relative to the gun hand's wrist, at rest
    bool reloading = false;
    double reloadEndedAt = -1, lastShotAt = -1;
    int16_t lastLoaded = -1;
    reload_clip::Clip* recording = nullptr;
    double recordStart = 0;
    int16_t loadedAtStart = 0;
    bool loadedRose = false;
};
GunState g_gun{};
std::atomic<uint64_t> held{0}, clipPosed{0}, ejectPlayed{0}, clipsRecorded{0}, clipsRefused{0};

double NowSeconds() noexcept
{
    static LARGE_INTEGER frequency{};
    if (!frequency.QuadPart) QueryPerformanceFrequency(&frequency);
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return frequency.QuadPart ? double(counter.QuadPart) / double(frequency.QuadPart) : 0.0;
}
static_assert(sizeof(BoneMatrix) == sizeof(reload_clip::Bone), "a clip's bone is the palette's matrix");
inline reload_clip::Bone ToBone(const BoneMatrix& m) noexcept { reload_clip::Bone b; memcpy(&b, &m, sizeof b); return b; }
inline BoneMatrix ToMatrix(const reload_clip::Bone& b) noexcept { BoneMatrix m; memcpy(&m, &b, sizeof m); return m; }

void EndRecording(bool complete) noexcept
{
    if (!g_gun.recording) return;
    reload_clip_store::FinishRecording(g_gun.recording, complete);
    (complete ? clipsRecorded : clipsRefused).fetch_add(1, std::memory_order_relaxed);
    g_gun.recording = nullptr;
}

// Before the native palette call for the held fp gun (scratch is this frame's private copy).
void Apply(GameTitle title, uint16_t tag, uint32_t generation, const FpInterpolationContext& context,
           const BoneMatrix& root, BoneMatrix* scratch, const int32_t* boneMap)
{
    if (!g_config.manual_reload || !g_config.manual_reload_magazine || !scratch || !boneMap || !context.valid ||
        context.slot != 0 || context.player != 0 || context.count <= 0 || context.count > 64 ||
        (title != GameTitle::Halo3 && title != GameTitle::Halo3ODST)) return;
    __try
    {
        uint32_t identity = 0;
        if (!LegacyReadWeaponRenderModelChecksum(title, tag, identity)) return;
        const auto* parts = manual_reload::FindParts(title, identity);
        if (!parts || parts->nodeCount <= 0 || parts->nodeCount > 64) return;
        int32_t map[64]{};
        if (!SafeReadBytes(boneMap, map, size_t(parts->nodeCount) * sizeof(int32_t))) return;
        auto source = [&](int node) {
            return node >= 0 && node < parts->nodeCount && map[node] >= 0 && map[node] < context.count ? map[node] : -1;
        };
        const int rootSource = source(0);
        if (rootSource < 0 || !g_fpStereoSolveScope.armed || !g_baseCamValid.load()) return;
        const int wrist = context.wrist;
        if (wrist < 0 || wrist >= context.count) return;
        const int nodes = parts->nodeCount < reload_clip::kMaxNodes ? parts->nodeCount : reload_clip::kMaxNodes;
        int src[reload_clip::kMaxNodes]{};
        for (int i = 0; i < nodes; ++i) src[i] = source(i);
        const int magazineSource = source(parts->magazineNode);
        const int rackSource = source(parts->rackNode);
        const float unitsPerMetre = g_worldScale.load();
        if (!std::isfinite(unitsPerMetre) || unitsPerMetre <= 0) return;

        uint32_t weapon = 0;
        reload_ammo::Sim sim{};
        const bool simValid = ReadAmmo(title, weapon, sim);
        if (!simValid) ammoFailures.fetch_add(1, std::memory_order_relaxed);
        manual_reload::Presentation p{};
        const bool shown = VR_ManualReloadPresentation(title, generation, identity, p);
        const bool owned = shown && p.owned;
        const double now = NowSeconds();
        const uint64_t nowMs = GetTickCount64();

        // The game's pose this frame: every node relative to the gun's root, and the root on the gun hand's wrist.
        BoneMatrix inverseRoot{}, inverseWrist{}, onWrist{};
        if (!InvertBoneMatrix(scratch[rootSource], inverseRoot) || !InvertBoneMatrix(scratch[wrist], inverseWrist) ||
            !ComposeBoneMatrices(inverseWrist, scratch[rootSource], onWrist)) return;
        reload_clip::Bone rel[reload_clip::kMaxNodes]{};
        bool complete = true;
        for (int i = 0; i < nodes; ++i)
        {
            BoneMatrix r{};
            if (src[i] >= 0 && ComposeBoneMatrices(inverseRoot, scratch[src[i]], r)) rel[i] = ToBone(r);
            else complete = false;
        }

        if (identity != g_gun.identity || (simValid && weapon != g_gun.weapon))
        {
            EndRecording(false);
            g_gun = GunState{};
            g_gun.identity = identity;
            g_gun.weapon = weapon;
        }
        if (simValid)
        {
            if (sim.loaded != g_gun.lastLoaded) { g_gun.lastShotAt = now; g_gun.lastLoaded = sim.loaded; }
            // RECORD the game's reload of this gun (the first of each kind), starting from the resting pose.
            if (sim.reloading && !g_gun.reloading)
            {
                g_gun.reloading = true;
                if (g_config.manual_reload_animation && g_gun.idleValid && !g_gun.recording && complete)
                {
                    const auto kind = sim.loaded == 0 ? reload_clip::Kind::Empty : reload_clip::Kind::Full;
                    if (auto* clip = reload_clip_store::BeginRecording(title, identity, kind))
                    {
                        reload_clip::Append(*clip, 0.f, g_gun.idleRel, nodes);
                        g_gun.recording = clip;
                        g_gun.recordStart = now;
                        g_gun.loadedAtStart = sim.loaded;
                        g_gun.loadedRose = false;
                    }
                }
            }
            else if (!sim.reloading && g_gun.reloading)
            {
                g_gun.reloading = false;
                g_gun.reloadEndedAt = now;
            }
            if (g_gun.recording)
            {
                const float at = float(now - g_gun.recordStart);
                if (complete) (void)reload_clip::Append(*g_gun.recording, at, rel, nodes);
                if (sim.loaded > g_gun.loadedAtStart) g_gun.loadedRose = true;
                const bool tailDone = !g_gun.reloading && now - g_gun.reloadEndedAt > 0.35;
                if (tailDone || at > reload_clip::kMaxSeconds || g_gun.recording->frames >= reload_clip::kMaxFrames)
                    EndRecording(g_gun.loadedRose && tailDone);
            }
        }
        // THE RESTING POSE, taken while nothing reloads, shoots or is held by the reload.
        const bool recentReload = g_gun.reloadEndedAt >= 0 && now - g_gun.reloadEndedAt < 0.4;
        const bool quiet = !shown || (p.phase == manual_reload::Phase::Idle && !p.hideMagazine && p.rackPull <= 0 &&
                                      !p.pressPending);
        if (simValid && complete && !sim.reloading && !recentReload && quiet &&
            (g_gun.lastShotAt < 0 || now - g_gun.lastShotAt > 0.3))
        {
            memcpy(g_gun.idleRel, rel, sizeof(reload_clip::Bone) * size_t(nodes));
            g_gun.idleOnWrist = onWrist;
            g_gun.idleValid = true;
        }

        // What the gun has learned.
        reload_clip_store::Learned magazineClip{}, rackClip{};
        const auto preferred = owned && p.emptyAtDrop ? reload_clip::Kind::Empty : reload_clip::Kind::Full;
        const auto other = preferred == reload_clip::Kind::Empty ? reload_clip::Kind::Full : reload_clip::Kind::Empty;
        bool haveMagazine = reload_clip_store::Find(title, identity, preferred, magazineClip) &&
                            magazineClip.segments.magazine;
        if (!haveMagazine)
            haveMagazine = reload_clip_store::Find(title, identity, other, magazineClip) && magazineClip.segments.magazine;
        bool haveRack = reload_clip_store::Find(title, identity, reload_clip::Kind::Empty, rackClip) &&
                        rackClip.segments.rack;
        if (!haveRack)
            haveRack = reload_clip_store::Find(title, identity, reload_clip::Kind::Full, rackClip) && rackClip.segments.rack;
        if (!g_config.manual_reload_animation) haveMagazine = haveRack = false;
        if (haveMagazine && (magazineClip.clip->nodes != nodes || magazineClip.magazineNode != parts->magazineNode))
            haveMagazine = false;
        if (haveRack && (rackClip.clip->nodes != nodes || rackClip.rackNode != parts->rackNode)) haveRack = false;

        // POSE: the game's reload is not shown while the manual reload has the gun.
        BoneMatrix gunBefore{};
        const bool gunBeforeOk = ComposeBoneMatrices(root, scratch[rootSource], gunBefore);
        const float metresPerUnit = gunBeforeOk ? std::fabs(gunBefore.scale) / unitsPerMetre : 0.f;
        const bool hold = owned && g_gun.idleValid &&
            (sim.reloading || recentReload || p.phase != manual_reload::Phase::Idle || p.hideMagazine ||
             p.rackPull > 0 || p.pressPending);
        bool magazineShown = !(owned && p.hideMagazine);
        bool rackByClip = false;
        bool posedMagazineValid = false;
        reload_clip::Bone pose[reload_clip::kMaxNodes]{};
        if (hold && metresPerUnit > 0)
        {
            memcpy(pose, g_gun.idleRel, sizeof(reload_clip::Bone) * size_t(nodes));
            auto sampleAll = [&](const reload_clip::Clip& clip, float t) {
                for (int i = 1; i < nodes; ++i) pose[i] = reload_clip::Sample(clip, t, i);
            };
            const auto& ms = magazineClip.segments;
            if (p.phase == manual_reload::Phase::Pushing && haveMagazine)
            {
                // The magazine rides the animation's way in; the hand's push picks the time.
                const float t = reload_clip::TimeAtDistance(*magazineClip.clip, parts->magazineNode, ms.in, ms.seat,
                                                            p.pushDepth / metresPerUnit);
                sampleAll(*magazineClip.clip, t);
                magazineShown = true;
                posedMagazineValid = true;
                clipPosed.fetch_add(1, std::memory_order_relaxed);
            }
            else if (p.phase == manual_reload::Phase::MagOut && haveMagazine && p.droppedAt && nowMs >= p.droppedAt &&
                     float(nowMs - p.droppedAt) * 0.001f < ms.outEnd - ms.outStart)
            {
                // The drop: the animation's way out plays, then the magazine is gone.
                sampleAll(*magazineClip.clip, ms.outStart + float(nowMs - p.droppedAt) * 0.001f);
                magazineShown = true;
                ejectPlayed.fetch_add(1, std::memory_order_relaxed);
            }
            else if (p.phase == manual_reload::Phase::Racking && haveRack && p.rackPull > 0)
            {
                const auto& rs = rackClip.segments;
                const float t = reload_clip::TimeAtDistance(*rackClip.clip, parts->rackNode, rs.rackStart, rs.rackFull,
                                                            std::fmin(p.rackPull, 1.f) * rs.rackTravel);
                sampleAll(*rackClip.clip, t);
                rackByClip = true;
                clipPosed.fetch_add(1, std::memory_order_relaxed);
            }
            // The gun rests on the hand where it rests; each part relative to it as posed.
            BoneMatrix gunRoot{};
            if (ComposeBoneMatrices(scratch[wrist], g_gun.idleOnWrist, gunRoot))
            {
                scratch[rootSource] = gunRoot;
                for (int i = 1; i < nodes; ++i)
                {
                    BoneMatrix placed{};
                    if (src[i] >= 0 && ComposeBoneMatrices(gunRoot, ToMatrix(pose[i]), placed)) scratch[src[i]] = placed;
                }
                held.fetch_add(1, std::memory_order_relaxed);
            }
        }

        // THE GEOMETRY the hands work against, from the gun as now drawn (its resting pose while held).
        BoneMatrix gun{};
        float basis[9]{};
        if (!ComposeBoneMatrices(root, scratch[rootSource], gun) || !NormalizedBasis(gun, basis)) return;
        const float metres = std::fabs(gun.scale) / unitsPerMetre;
        auto worldPoint = [&](const reload_clip::Bone& local, float out[3]) {
            BoneMatrix w{};
            if (!ComposeBoneMatrices(gun, ToMatrix(local), w)) return false;
            for (int a = 0; a < 3; ++a) out[a] = w.translation[a];
            return true;
        };
        auto worldDirection = [&](const reload_clip::V3& d, float out[3]) {
            for (int r = 0; r < 3; ++r) out[r] = basis[r] * d.x + basis[3 + r] * d.y + basis[6 + r] * d.z;
        };

        // Measured from the gun hand's aim pose AS THE ARM SOLVER PLACES IT in this palette (ControllerWorldPoseEx,
        // the pose DesiredWristWorld builds the hands from), less the gun's mount offsets, and published in that aim
        // frame; the VR thread rebuilds it from the same aim pose (VR_GetAimPose). Whatever maps tracking space into
        // the game cancels out. A hand-built world transform put the well 40 cm low in the first FA pass.
        float aimBasis[9]{}, aimPos[3]{}, meshScale = 1.0f;
        if (!ControllerWorldPoseEx(false, aimBasis, aimPos, meshScale)) return;
        {
            const float standoff = weapon_alignment::Forward(false, g_config.gun_forward_m, g_config.left_hand_forward_m) * unitsPerMetre;
            const float rightOff = weapon_alignment::Right(false, g_config.gun_right_m) * unitsPerMetre;
            const float upOff = weapon_alignment::Up(g_config.gun_up_m) * unitsPerMetre;
            for (int j = 0; j < 3; ++j)
                aimPos[j] -= aimBasis[j] * standoff + aimBasis[3 + j] * rightOff + aimBasis[6 + j] * upOff;
        }
        // World -> the aim's OpenXR frame (x right, y up, z back), metres.
        auto toAim = [&](const float* v, bool point, manual_reload::Vec& out) {
            float d[3]{v[0], v[1], v[2]};
            if (point) for (int j = 0; j < 3; ++j) d[j] = (v[j] - aimPos[j]) / unitsPerMetre;
            const float f = d[0] * aimBasis[0] + d[1] * aimBasis[1] + d[2] * aimBasis[2];
            const float l = d[0] * aimBasis[3] + d[1] * aimBasis[4] + d[2] * aimBasis[5];
            const float u = d[0] * aimBasis[6] + d[1] * aimBasis[7] + d[2] * aimBasis[8];
            out = {-l, u, -f};
            return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z) &&
                   (!point || out.x * out.x + out.y * out.y + out.z * out.z < 4.0f);
        };

        manual_reload::GameSample sample{};
        sample.title = title; sample.generation = generation; sample.identity = identity;
        sample.weapon = weapon; sample.simValid = simValid; sample.sim = sim;
        // The well: the animation's own (the magazine's resting place and the way it goes in), else the reload
        // catalogue's magazine rest with the gun's up as its axis.
        float well[3]{}, axis[3]{};
        bool wellValid = false;
        if (haveMagazine && worldPoint(magazineClip.clip->bone[0][parts->magazineNode], well))
        {
            worldDirection(magazineClip.segments.approach, axis);
            wellValid = true;
            sample.learned = true;
            sample.insertDepth = magazineClip.segments.window * metres;
        }
        else
        {
            const auto* model = manual_reload::FindModel(title, identity);
            wellValid = parts->magazineNode >= 0 && model && model->vertexCount &&
                        weapon_model::ReceiverPoint(model, gun.scale, basis, gun.translation, well);
            for (int r = 0; r < 3; ++r) axis[r] = basis[6 + r];
        }
        float rack[3]{}, back[3]{};
        bool rackValid = false;
        if (haveRack && worldPoint(rackClip.clip->bone[0][parts->rackNode], rack))
        {
            worldDirection(rackClip.segments.rackBack, back);
            rackValid = true;
            sample.rackTravel = rackClip.segments.rackTravel * metres;
        }
        else if (rackSource >= 0)
        {
            BoneMatrix part{};
            rackValid = ComposeBoneMatrices(root, scratch[rackSource], part);
            for (int a = 0; a < 3 && rackValid; ++a) rack[a] = part.translation[a];
            for (int r = 0; r < 3; ++r) back[r] = -basis[r];
        }
        sample.wellValid = wellValid && toAim(well, true, sample.well) && toAim(axis, false, sample.wellAxis);
        sample.rackValid = rackValid && toAim(rack, true, sample.rack) && toAim(back, false, sample.rackBack);
        if (posedMagazineValid && magazineSource >= 0)
        {
            BoneMatrix m{};
            if (ComposeBoneMatrices(root, scratch[magazineSource], m))
                sample.posedValid = toAim(m.translation, true, sample.posedMagazine);
        }
        {
            // The layout for the log: the root's origin and axes, the magazine and rack nodes, the well.
            Layout_ l{};
            manual_reload::Vec v{};
            auto put = [&](const float* w, bool point, float (&o)[3]) {
                if (toAim(w, point, v)) { o[0] = v.x; o[1] = v.y; o[2] = v.z; }
            };
            const float c0[3]{basis[0], basis[1], basis[2]}, c1[3]{basis[3], basis[4], basis[5]},
                        c2[3]{basis[6], basis[7], basis[8]};
            put(gun.translation, true, l.root); put(c0, false, l.fwd); put(c1, false, l.left); put(c2, false, l.up);
            BoneMatrix node{};
            if (magazineSource >= 0 && ComposeBoneMatrices(root, scratch[magazineSource], node)) put(node.translation, true, l.magNode);
            if (rackValid) put(rack, true, l.rackNode);
            if (wellValid) put(well, true, l.well);
            l.identity = identity;
            g_layoutSeq.fetch_add(1, std::memory_order_acq_rel);
            g_layout = l;
            g_layoutSeq.fetch_add(1, std::memory_order_release);
        }
        VR_PublishManualReloadSample(sample);
        samples.fetch_add(1, std::memory_order_relaxed);

        // What the hands asked for, on this frame's private copy only.
        if (!owned) return;
        if (!magazineShown && magazineSource >= 0)
        {
            scratch[magazineSource].scale = 0.0001f;
            hidden.fetch_add(1, std::memory_order_relaxed);
        }
        if (p.rackPull > 0 && !rackByClip && rackSource >= 0 && std::isfinite(p.rackPull) && std::isfinite(p.rackTravel))
        {
            // No learned stroke: the part's own palette entry moves back along the gun by the hand's pull.
            const float units = std::fmin(p.rackPull, 1.0f) * std::fmin(std::fmax(p.rackTravel, 0.0f), 0.2f) *
                                unitsPerMetre;
            float local[3]{};
            const float worldBack[3]{back[0] * units, back[1] * units, back[2] * units};
            // scratch is in the root's frame: carry the world offset back through the root's basis.
            float rootBasis[9]{};
            const float s = std::fabs(root.scale) > 1e-6f ? root.scale : 1.0f;
            for (int i = 0; i < 9; ++i) rootBasis[i] = root.rotation[i];
            for (int c = 0; c < 3; ++c)
                local[c] = (rootBasis[c * 3 + 0] * worldBack[0] + rootBasis[c * 3 + 1] * worldBack[1] +
                            rootBasis[c * 3 + 2] * worldBack[2]) / s;
            for (int a = 0; a < 3; ++a) scratch[rackSource].translation[a] += local[a];
            posed.fetch_add(1, std::memory_order_relaxed);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        faults.fetch_add(1, std::memory_order_relaxed);
    }
}

void Report()
{
    {
        // Once per gun model: where its parts sit in the gun hand's aim frame.
        static uint64_t loggedModel = 0;
        {
            const uint32_t before = g_layoutSeq.load(std::memory_order_acquire);
            const Layout_ l = g_layout;
            if (!(before & 1u) && before && g_layoutSeq.load(std::memory_order_acquire) == before &&
                l.identity != loggedModel)
            {
                loggedModel = l.identity;
                LOG("Manual reload gun layout (aim frame, m; x right y up z back): model=%016llX root=(%.3f,%.3f,%.3f) "
                    "fwd=(%.2f,%.2f,%.2f) left=(%.2f,%.2f,%.2f) up=(%.2f,%.2f,%.2f) magazine-node=(%.3f,%.3f,%.3f) "
                    "rack-node=(%.3f,%.3f,%.3f) well=(%.3f,%.3f,%.3f)",
                    static_cast<unsigned long long>(l.identity), l.root[0], l.root[1], l.root[2], l.fwd[0], l.fwd[1],
                    l.fwd[2], l.left[0], l.left[1], l.left[2], l.up[0], l.up[1], l.up[2], l.magNode[0], l.magNode[1],
                    l.magNode[2], l.rackNode[0], l.rackNode[1], l.rackNode[2], l.well[0], l.well[1], l.well[2]);
            }
        }
    }
    {
        char line[256];
        reload_clip_store::Tick(line, sizeof(line));
        if (line[0]) LOG("%s", line);
    }
    {
        static uint64_t lastClips[5]{};
        static ULONGLONG clipsAt = 0;
        const uint64_t clips[]{held.load(), clipPosed.load(), ejectPlayed.load(), clipsRecorded.load(), clipsRefused.load()};
        const ULONGLONG tick = GetTickCount64();
        if (std::memcmp(lastClips, clips, sizeof(clips)) &&
            (clips[3] != lastClips[3] || clips[4] != lastClips[4] || tick - clipsAt >= 10000))
        {
            std::memcpy(lastClips, clips, sizeof(clips));
            clipsAt = tick;
            LOG("Manual reload (gun's look): held=%llu posed-from-animation=%llu way-out-played=%llu reloads-learned=%llu "
                "reloads-not-learned=%llu; %u clip(s) known",
                clips[0], clips[1], clips[2], clips[3], clips[4], reload_clip_store::LearnedCount());
        }
    }
    static uint64_t last[5]{};
    static ULONGLONG at = 0;
    const uint64_t now[]{samples.load(), ammoFailures.load(), hidden.load(), posed.load(), faults.load()};
    const auto clock = GetTickCount64();
    if (std::memcmp(last, now, sizeof(now)) && (clock - at >= 10000 || now[4] != last[4]))
    {
        std::memcpy(last, now, sizeof(now));
        at = clock;
        LOG("Manual reload (game side): gun samples=%llu ammo-unread=%llu magazine-hidden=%llu rack-drawn=%llu "
            "faults=%llu (reads only; the gun's magazine and rack part are this frame's first-person palette)",
            now[0], now[1], now[2], now[3], now[4]);
    }
}
} // namespace manual_reload_game
