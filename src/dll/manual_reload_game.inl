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
        BoneMatrix held{};
        float basis[9]{};
        if (!ComposeBoneMatrices(root, scratch[rootSource], held) || !NormalizedBasis(held, basis)) return;

        // Everything is measured from the gun hand's aim pose AS THE ARM SOLVER PLACES IT in this palette
        // (ControllerWorldPoseEx, the pose DesiredWristWorld builds the hands from), less the gun's mount offsets, and
        // published in that aim frame; the VR thread rebuilds it from the same aim pose (VR_GetAimPose). Whatever
        // maps tracking space into the game (yaw references, the follow basis, the hands' origin) cancels out. A
        // hand-built world transform put the well 40 cm low and the gun 58 degrees off in the first FA pass.
        float aimBasis[9]{}, aimPos[3]{}, meshScale = 1.0f;
        if (!ControllerWorldPoseEx(false, aimBasis, aimPos, meshScale)) return;
        const float unitsPerMetre = g_worldScale.load();
        if (!std::isfinite(unitsPerMetre) || unitsPerMetre <= 0) return;
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
        // Columns: forward, left, up. The magazine travels up into the gun; the rack part is pulled back.
        const float up[3]{basis[6], basis[7], basis[8]};
        const float back[3]{-basis[0], -basis[1], -basis[2]};
        float well[3]{};
        const auto* model = manual_reload::FindModel(title, identity);
        const bool wellValid = parts->magazineNode >= 0 && model && model->vertexCount &&
            weapon_model::ReceiverPoint(model, held.scale, basis, held.translation, well);
        const int rackSource = source(parts->rackNode);
        float rack[3]{};
        bool rackValid = false;
        if (rackSource >= 0)
        {
            BoneMatrix part{};
            rackValid = ComposeBoneMatrices(root, scratch[rackSource], part);
            for (int a = 0; a < 3 && rackValid; ++a) rack[a] = part.translation[a];
        }
        uint32_t weapon = 0;
        reload_ammo::Sim sim{};
        const bool simValid = ReadAmmo(title, weapon, sim);
        if (!simValid) ammoFailures.fetch_add(1, std::memory_order_relaxed);
        manual_reload::Vec wellLocal{}, axisLocal{}, rackLocal{}, backLocal{};
        const bool wellOk = wellValid && toAim(well, true, wellLocal) && toAim(up, false, axisLocal);
        const bool rackOk = rackValid && toAim(rack, true, rackLocal) && toAim(back, false, backLocal);
        VR_PublishManualReloadSample(title, generation, identity, weapon, simValid, sim,
                                     wellOk, wellLocal, axisLocal, rackOk, rackLocal, backLocal);
        samples.fetch_add(1, std::memory_order_relaxed);

        // What the hands asked for, on this frame's private copy only.
        manual_reload::Presentation p{};
        if (!VR_ManualReloadPresentation(title, generation, identity, p)) return;
        const int magazineSource = source(parts->magazineNode);
        if (p.hideMagazine && magazineSource >= 0)
        {
            scratch[magazineSource].scale = 0.0001f;
            hidden.fetch_add(1, std::memory_order_relaxed);
        }
        if (p.rackPull > 0 && rackSource >= 0 && std::isfinite(p.rackPull) && std::isfinite(p.rackTravel))
        {
            // The part's own palette entry moves back along the gun by the hand's pull (world units).
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
