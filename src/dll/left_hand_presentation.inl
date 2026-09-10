// Included after the title-specific render-model readers. The body palette's
// actual remap separates anatomical fingers from weapon descendants; a count
// or copied cross-title skeleton prefix is never used as that boundary.
static int LegacyAnatomicalRenderNodeCount(GameTitle title, uint16_t tag)
{
    __try
    {
        const uint8_t* definition = title == GameTitle::Halo3
            ? Halo3LoadedTagDefinition(tag) : OdstLoadedTagDefinition(tag);
        const size_t offset = title == GameTitle::Halo3
            ? kHalo3RenderModelNodesBlockOffset : kOdstRenderModelNodesCountOffset;
        int count = 0;
        return definition && SafeReadBytes(definition + offset, &count, sizeof(count))
            ? count : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

bool LegacyRouteLeftHandedPalette(GameTitle title, uint16_t tag, const int32_t* boneMap,
    const BoneMatrix& root, const FpInterpolationContext& context,
    FpInterpolationContext& contactContext)
{
    auto& scope = g_fpStereoSolveScope;
    const auto& tracking = scope.anatomicalTracking;
    if (!scope.armed || !tracking.serial || !tracking.leftHanded ||
        !tracking.primaryAimValid || !tracking.hands[0].valid ||
        !context.valid || context.slot < 0 || context.slot > 1 ||
        context.count <= 0 || context.count > 64 || !boneMap ||
        context.wrist < 0 || context.wrist >= context.count ||
        context.lWrist < 0 || context.lWrist >= context.count ||
        (title != GameTitle::Halo3 && title != GameTitle::Halo3ODST)) return false;
    const int count = LegacyAnatomicalRenderNodeCount(title, tag);
    if (count <= 0 || count > 64) return false;
    int32_t remap[64]{};
    if (!SafeReadBytes(boneMap, remap, size_t(count) * sizeof(*remap))) return false;
    uint64_t bodyMask = 0;
    for (int node = 0; node < count; ++node)
    {
        const int source = remap[node];
        if (source < -1 || source >= context.count) return false;
        if (source >= 0) bodyMask |= uint64_t{1} << source;
    }
    const uint64_t rightMask = context.wristDescendants & bodyMask;
    const uint64_t leftMask = context.lWristDescendants & bodyMask;
    if (!(rightMask & (uint64_t{1} << context.wrist)) ||
        !(leftMask & (uint64_t{1} << context.lWrist)) || (rightMask & leftMask))
        return false; // a held-model-only palette cannot authorize hand routing
    if (!scope.anatomicalCarriersValid)
    {
        float hullYaw = 0, hullPitch = 0;
        const bool follow = Halo3ReadRollStableFollow(hullYaw, hullPitch);
        BuildTrackedGameBasisFromFrame(tracking.primaryAimOrientation, false,
            follow, hullYaw, hullPitch, scope.anatomicalPrimaryCarrier.rotation);
        BuildTrackedGameBasisFromFrame(tracking.hands[0].orientation, false,
            follow, hullYaw, hullPitch, scope.anatomicalSupportCarrier.rotation);
        scope.anatomicalTorsoRoot = root;
        float cameraBasis[9]{};
        if (!g_camValid.load() || !LoadCameraBasis(cameraBasis)) return false;
        memcpy(scope.anatomicalTorsoRoot.rotation, cameraBasis, sizeof(cameraBasis));
        scope.anatomicalTorsoRoot.translation[0] = g_camX.load();
        scope.anatomicalTorsoRoot.translation[1] = g_camY.load();
        scope.anatomicalTorsoRoot.translation[2] = g_camZ.load();
        if (g_config.shoulder_level)
        {
            float up[3]{g_worldUp[0].load(),g_worldUp[1].load(),g_worldUp[2].load()};
            const float dot=cameraBasis[0]*up[0]+cameraBasis[1]*up[1]+cameraBasis[2]*up[2];
            float forward[3]{cameraBasis[0]-dot*up[0],cameraBasis[1]-dot*up[1],cameraBasis[2]-dot*up[2]};
            const float length=sqrtf(forward[0]*forward[0]+forward[1]*forward[1]+forward[2]*forward[2]);
            if (isfinite(length) && length>1e-3f)
            {
                for (float& v:forward) v/=length;
                float left[3]{up[1]*forward[2]-up[2]*forward[1],
                              up[2]*forward[0]-up[0]*forward[2],
                              up[0]*forward[1]-up[1]*forward[0]};
                memcpy(scope.anatomicalTorsoRoot.rotation,forward,sizeof(forward));
                memcpy(scope.anatomicalTorsoRoot.rotation+3,left,sizeof(left));
                memcpy(scope.anatomicalTorsoRoot.rotation+6,up,sizeof(up));
            }
        }
        scope.anatomicalCarriersValid =
            Halo4FloatingTransformValid(scope.anatomicalPrimaryCarrier) &&
            Halo4FloatingTransformValid(scope.anatomicalSupportCarrier);
    }
    if (!scope.anatomicalCarriersValid) return false;
    const auto value = [](const BoneMatrix& input) {
        Halo4FloatingTransform out{};
        out.scale = input.scale;
        memcpy(out.rotation, input.rotation, sizeof(out.rotation));
        memcpy(out.translation, input.translation, sizeof(out.translation));
        return out;
    };
    Halo4FloatingTransform palette[64];
    for (int i = 0; i < context.count; ++i) palette[i] = value(g_fpPaletteScratch[i]);
    if (!RouteLeftHandedFloatingPalette(palette, size_t(context.count),
            context.wrist, rightMask, context.lWrist, leftMask, value(root),
            scope.anatomicalPrimaryCarrier, scope.anatomicalSupportCarrier)) return false;

    // Plant shoulders on their authored anatomical side in the frozen torso
    // frame, then solve elbows toward the newly assigned wrists. Use authored
    // lengths, not the length of the previously cross-body stretched arm.
    if (g_config.arm_ik && !g_config.floating_hands)
    {
        const BoneMatrix* authored = g_fpUnmodifiedInterpolations[context.slot];
        BoneMatrix inverseRoot{};
        if (!InvertBoneMatrix(root, inverseRoot)) return false;
        const int shoulders[2]{context.lShoulder, context.shoulder};
        const int elbows[2]{context.lElbow, context.elbow};
        const int wrists[2]{context.lWrist, context.wrist};
        for (int hand = 0; hand < 2; ++hand)
        {
            const int sh = shoulders[hand], el = elbows[hand], wr = wrists[hand];
            if (sh < 0 || el < 0 || sh >= context.count || el >= context.count ||
                sh == el || sh == wr || el == wr ||
                !(bodyMask & (uint64_t{1} << sh)) || !(bodyMask & (uint64_t{1} << el)))
                return false;
            BoneMatrix shWorld{}, elWorld{}, wrWorld{}, desiredWorld{}, desiredLocal{};
            desiredLocal.scale = palette[wr].scale;
            memcpy(desiredLocal.rotation, palette[wr].rotation, sizeof(desiredLocal.rotation));
            memcpy(desiredLocal.translation, palette[wr].translation, sizeof(desiredLocal.translation));
            if (!ComposeBoneMatrices(scope.anatomicalTorsoRoot, authored[sh], shWorld) ||
                !ComposeBoneMatrices(scope.anatomicalTorsoRoot, authored[el], elWorld) ||
                !ComposeBoneMatrices(scope.anatomicalTorsoRoot, authored[wr], wrWorld) ||
                !ComposeBoneMatrices(root, desiredLocal, desiredWorld)) return false;
            const auto distance = [](const float* a, const float* b) {
                const float x = a[0]-b[0], y = a[1]-b[1], z = a[2]-b[2];
                return sqrtf(x*x+y*y+z*z);
            };
            float upper = distance(authored[sh].translation, authored[el].translation) * fabsf(root.scale);
            float lower = distance(authored[el].translation, authored[wr].translation) * fabsf(root.scale);
            float torsoBasis[9]{};
            if (!NormalizedBasis(scope.anatomicalTorsoRoot,torsoBasis)) return false;
            const float drop = hand == 0 ? g_config.right_shoulder_drop : 0.0f;
            for (int axis=0;axis<3;++axis)
                shWorld.translation[axis]-=torsoBasis[6+axis]*drop+
                    torsoBasis[axis]*g_config.shoulder_back_m;
            const float targetDistance = distance(shWorld.translation, desiredWorld.translation);
            if (!isfinite(upper) || !isfinite(lower) || upper <= 1e-6f || lower <= 1e-6f ||
                !isfinite(targetDistance)) return false;
            const float stretch = fminf(fmaxf(targetDistance / (upper + lower), 1.0f), 1.8f);
            float basis[9]{};
            if (!NormalizedBasis(scope.anatomicalTorsoRoot, basis)) return false;
            const float sign = hand == 0 ? 1.0f : -1.0f;
            const float pole[3]{sign*basis[3]-0.6f*basis[6],
                                sign*basis[4]-0.6f*basis[7],
                                sign*basis[5]-0.6f*basis[8]};
            float elbow[3]{};
            if (!IK_SolveTwoBone(shWorld.translation, desiredWorld.translation,
                    upper*stretch, lower*stretch, pole, elbow)) return false;
            const auto unit = [&](const float* a, const float* b, float* out) {
                const float length = distance(a,b);
                if (!isfinite(length) || length <= 1e-6f) return false;
                for (int j=0;j<3;++j) out[j]=(a[j]-b[j])/length;
                return true;
            };
            float oldUpper[3], newUpper[3], oldLower[3], newLower[3], turnUpper[9], turnLower[9];
            if (!unit(elWorld.translation, shWorld.translation, oldUpper) ||
                !unit(elbow, shWorld.translation, newUpper) ||
                !unit(wrWorld.translation, elWorld.translation, oldLower) ||
                !unit(desiredWorld.translation, elbow, newLower)) return false;
            ShortestArcRotation(oldUpper, newUpper, turnUpper);
            ShortestArcRotation(oldLower, newLower, turnLower);
            BoneMatrix newSh = shWorld, newEl = elWorld, localSh{}, localEl{};
            MultiplyBases(turnUpper, shWorld.rotation, newSh.rotation);
            MultiplyBases(turnLower, elWorld.rotation, newEl.rotation);
            memcpy(newEl.translation, elbow, sizeof(elbow));
            if (!ComposeBoneMatrices(inverseRoot, newSh, localSh) ||
                !ComposeBoneMatrices(inverseRoot, newEl, localEl)) return false;
            palette[sh]=value(localSh); palette[el]=value(localEl);
        }
    }
    for (int i = 0; i < context.count; ++i)
    {
        g_fpPaletteScratch[i].scale = palette[i].scale;
        memcpy(g_fpPaletteScratch[i].rotation, palette[i].rotation, sizeof(palette[i].rotation));
        memcpy(g_fpPaletteScratch[i].translation, palette[i].translation, sizeof(palette[i].translation));
    }
    contactContext = context;
    contactContext.wrist = context.lWrist;
    contactContext.lWrist = context.wrist;
    contactContext.wristDescendants = leftMask;
    contactContext.lWristDescendants = rightMask;
    return true;
}
