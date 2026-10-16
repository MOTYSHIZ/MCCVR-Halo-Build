// VEHICLE CAMERAS: the runtime. Included by game.cpp, after the Halo 3 seat snapshot (Halo3ReadSeatSnapshot).
// The file's format, the selection and the geometry are engine-free (src/common/vehicle_camera_*.h); this is
// who runs them and what a game's seat camera asks of them.
//
// THREADS.
//   The cold worker (WaitThread, VehicleCameras_ColdTick) owns the file's store and the selector: it reads the
//     file, follows the seat the player is in, takes up the steps the menu asked for, and publishes the camera
//     in use with the seat it was chosen for. It logs and allocates; nothing else here does.
//   A game's camera thread reads that record once a frame and places its seat view from it
//     (Halo3VehicleCameraPlace), publishing the point for the threads that build the hands and the aim on it.
//   The menu's thread asks for steps (two atomics) and reads the readout line.
//
// OFF (vehicle_cameras = 0, the default): the cold worker publishes no camera, every reader returns false on one
// atomic load, and each game's seat view is exactly what it is without this file.
//
// HALO 3. A chase camera stands in for the seat's authored point in the mod's own seat camera (ApplyHeadLook),
// so what hangs off the rendered eye goes with it, and the hands' origin follows (ControllerWorldPoseEx). The
// camera director is left on its own third-person answer for that seat (halo3_native_seat_patch.inl), so the
// game draws the player's character in the seat and no first-person arms. Nothing the simulation reads is
// touched. The camera's mode turns the view (Halo3ApplySeatYawFollow): rotationTracking yaw follows the hull's
// heading, and pitch with it where the seat takes the hull's pitch. How the seat is steered is a separate
// choice (vehicle_control).
// Not applied in this game yet: rotationTracking roll, pitch without yaw, the leash, collide, and an origin
// naming a marker (the vehicle's origin stands in). The cold worker says so in the log for a camera that asks
// for one.

const char* Halo3VehicleIdName(uint32_t id);

// The seat a camera was chosen for, so a game's camera thread never lays one seat's camera onto another.
struct VehicleCameraSeatKey
{
    uint32_t generation = 0;
    int32_t vehicle = -1;
    int32_t seatIndex = -1;
    uint32_t identity = 0;
    bool mounted = false;
    friend bool operator==(const VehicleCameraSeatKey&, const VehicleCameraSeatKey&) = default;
};

// One writer (the cold worker). `chase` is the readers' whole cost while no chase camera is in use.
struct VehicleCameraPublished
{
    std::atomic<uint32_t> seq{0}; // even = stable
    std::atomic<uint32_t> chase{0};
    vehicle_camera_select::ActiveCamera camera{};
    VehicleCameraSeatKey seat{};
};
VehicleCameraPublished g_vehicleCameraPublished;

void VehicleCameraPublish(const vehicle_camera_select::ActiveCamera& camera, const VehicleCameraSeatKey& seat)
{
    VehicleCameraPublished& published = g_vehicleCameraPublished;
    published.seq.fetch_add(1, std::memory_order_acq_rel);
    published.camera = camera;
    published.seat = seat;
    published.chase.store(camera.valid && camera.chase ? 1u : 0u, std::memory_order_relaxed);
    published.seq.fetch_add(1, std::memory_order_release);
}

bool VehicleCameraRead(vehicle_camera_select::ActiveCamera& camera, VehicleCameraSeatKey& seat)
{
    const VehicleCameraPublished& published = g_vehicleCameraPublished;
    if (!published.chase.load(std::memory_order_acquire))
        return false;
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        const uint32_t before = published.seq.load(std::memory_order_acquire);
        if (before & 1u)
            continue;
        camera = published.camera;
        seat = published.seat;
        std::atomic_thread_fence(std::memory_order_acquire);
        if (published.seq.load(std::memory_order_relaxed) != before)
            continue;
        return camera.valid && camera.chase;
    }
    return false;
}

// The menu's line: which camera is in use, or why none is. One writer (the cold worker).
struct VehicleCameraReadout
{
    std::atomic<uint32_t> seq{0};
    char text[384]{};
};
VehicleCameraReadout g_vehicleCameraReadout;
std::atomic<int> g_vehicleCameraStepCamera{0};
std::atomic<int> g_vehicleCameraStepMode{0};
// The left stick click in a seat (vehicle_seat_buttons.h), posted by the input hook: the next way to control it.
std::atomic<int> g_vehicleControlStep{0};
// The seat's control mode, as vehicle_control's numbers: the player's choice for this seat where he made one,
// else the setting. -1 = not in a seat the vehicle cameras run in, where the setting alone decides.
std::atomic<int> g_vehicleControlEffective{-1};
// The camera buttons are the mod's in this seat (bit 0), and it is a passenger's (bit 1). Read by the input hook.
std::atomic<uint32_t> g_vehicleSeatButtons{0};
inline constexpr uint32_t kVehicleSeatButtonsLive = 1u, kVehicleSeatPassenger = 2u;

// How the seat the player is in is controlled (vehicle_control_logic.h). Any thread.
vehicle_control::Mode VehicleControlModeNow()
{
    const int effective = g_vehicleControlEffective.load(std::memory_order_acquire);
    return vehicle_control::FromConfig(effective >= 0 ? effective : g_config.vehicle_control);
}

void VehicleCameraSetReadout(const std::string& text)
{
    VehicleCameraReadout& readout = g_vehicleCameraReadout;
    readout.seq.fetch_add(1, std::memory_order_acq_rel);
    const size_t length = text.size() < sizeof(readout.text) - 1 ? text.size() : sizeof(readout.text) - 1;
    memcpy(readout.text, text.data(), length);
    readout.text[length] = '\0';
    readout.seq.fetch_add(1, std::memory_order_release);
}

bool VehicleCameraGetReadout(char* out, size_t capacity)
{
    if (!out || capacity == 0)
        return false;
    out[0] = '\0';
    const VehicleCameraReadout& readout = g_vehicleCameraReadout;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        const uint32_t before = readout.seq.load(std::memory_order_acquire);
        if (before & 1u)
            continue;
        char text[sizeof(readout.text)];
        memcpy(text, readout.text, sizeof(text));
        std::atomic_thread_fence(std::memory_order_acquire);
        if (readout.seq.load(std::memory_order_relaxed) != before)
            continue;
        text[sizeof(text) - 1] = '\0';
        const size_t length = strlen(text) < capacity - 1 ? strlen(text) : capacity - 1;
        memcpy(out, text, length);
        out[length] = '\0';
        return length != 0;
    }
    return false;
}

void VehicleCameraRequestStep(int cameraDirection, int modeDirection)
{
    if (cameraDirection)
        g_vehicleCameraStepCamera.fetch_add(cameraDirection > 0 ? 1 : -1, std::memory_order_relaxed);
    if (modeDirection)
        g_vehicleCameraStepMode.fetch_add(modeDirection > 0 ? 1 : -1, std::memory_order_relaxed);
}

// ---- Halo 3 ----------------------------------------------------------------------------------------------

// The chase camera's point this frame, before the player's own head movement. One writer (the camera thread).
struct Halo3VehicleCameraPoint
{
    std::atomic<uint32_t> seq{0};
    std::atomic<uint64_t> ms{0};
    float point[3]{};
};
Halo3VehicleCameraPoint g_halo3VehicleCameraPoint;
inline constexpr uint64_t kHalo3VehicleCameraPointFreshMs = 100;
vehicle_camera_place::State g_halo3VehicleCameraState; // camera thread
vehicle_camera_place::LagState g_halo3VehicleCameraLag; // camera thread
LARGE_INTEGER g_halo3VehicleCameraLagAt{};              // camera thread: the last lag step's clock
// Frames placed; those of them placed from the simulation's vehicle frame because the drawn root node did not
// check out; and those placed at the mod's stand-in because the game's own third-person camera could not be
// read. Counted by the camera thread, taken by the cold worker for its log lines.
std::atomic<uint32_t> g_halo3VehicleCameraFrames{0};
std::atomic<uint32_t> g_halo3VehicleCameraTickFrames{0};
std::atomic<uint32_t> g_halo3VehicleCameraStandInFrames{0};
// "collide": frames that asked for it and had a fresh trace, frames held short of something, and frames that
// asked with no trace to go on (the title's trace is not running for this seat).
std::atomic<uint32_t> g_halo3VehicleCameraCollideFrames{0};
std::atomic<uint32_t> g_halo3VehicleCameraHeldFrames{0};
std::atomic<uint32_t> g_halo3VehicleCameraNoTraceFrames{0};
// How much of the way out the camera may go, eased (vehicle_camera_place::EaseReach). Camera thread.
float g_halo3VehicleCameraReach = 1.0f;
// This pass's placement, for the leash on the head (Halo3VehicleCameraLeashHead), later in the same pass.
// Camera thread.
struct Halo3VehicleCameraPass
{
    bool placed = false;
    float reach = 1.0f;
    vehicle_camera_select::ActiveCamera camera{};
    vehicle_camera_place::Placed where{};
};
Halo3VehicleCameraPass g_halo3VehicleCameraPass;
uint64_t g_halo3VehicleCameraReachAt = 0;
inline constexpr float kHalo3CameraReleasePerSecond = 1.5f;

// A chase camera is chosen for exactly the seat the snapshot shows. Any thread; one atomic load while none is.
bool Halo3VehicleCameraSelected(vehicle_camera_select::ActiveCamera& camera, Halo3SeatSnapshot& seat,
                                VehicleCameraSeatKey& key)
{
    VehicleCameraSeatKey chosenFor;
    if (!VehicleCameraRead(camera, chosenFor))
        return false;
    if (!Halo3ReadSeatSnapshot(seat))
        return false;
    key = VehicleCameraSeatKey{g_halo3RuntimeGeneration.load(std::memory_order_relaxed), seat.parentHandle,
                               seat.seatIndex, seat.identity, seat.mounted};
    return key == chosenFor;
}

// The seat sampler's question: is this seat shown from a chase camera? It is from the moment one is chosen, so
// the game's director is already drawing the seat in third person when the camera is first placed.
bool Halo3VehicleCameraThirdPerson()
{
    vehicle_camera_select::ActiveCamera camera;
    Halo3SeatSnapshot seat;
    VehicleCameraSeatKey key;
    return Halo3VehicleCameraSelected(camera, seat, key);
}

// How a chosen chase camera turns the view (Halo3ApplySeatYawFollow): its mode's rotationTracking. Yaw follows
// the hull's heading; pitch follows it too where the seat takes the hull's pitch at all, and only with yaw.
bool Halo3VehicleCameraViewTurn(bool& yaw, bool& pitch)
{
    vehicle_camera_select::ActiveCamera camera;
    Halo3SeatSnapshot seat;
    VehicleCameraSeatKey key;
    if (!Halo3VehicleCameraSelected(camera, seat, key))
        return false;
    yaw = camera.rotYaw;
    pitch = camera.rotYaw && camera.rotPitch;
    return true;
}

bool Halo3VehicleCameraFollowWanted()
{
    bool yaw = false, pitch = false;
    return Halo3VehicleCameraViewTurn(yaw, pitch) && yaw;
}

// The game's own camera for the local player as its observer computed it this frame, before any effect and
// before the mod's seat camera: where it stands, and the point it turns about. For a seat the director draws in
// third person those are the game's chase camera and the seat's camera pivot. `forward`, when asked for, is
// the way it looks: the line a flat player's crosshair marks.
bool Halo3ReadObserverCamera(float position[3], float focus[3], float* forward = nullptr)
{
    Halo3ObserverCapture& cap = g_halo3ObserverCapture;
    if (!cap.valid.load(std::memory_order_acquire))
        return false;
    if (GetTickCount64() - cap.captureMs.load(std::memory_order_relaxed) > 200)
        return false;
    constexpr size_t positionIndex = (kHalo3ObserverPosOffset - kHalo3ObserverCaptureBase) / 4;
    constexpr size_t focusIndex = (kHalo3ObserverFocusOffset - kHalo3ObserverCaptureBase) / 4;
    constexpr size_t forwardIndex = (kHalo3ObserverFwdOffset - kHalo3ObserverCaptureBase) / 4;
    static_assert(positionIndex + 2 < kHalo3ObserverCaptureFloats && focusIndex + 2 < kHalo3ObserverCaptureFloats &&
                      forwardIndex + 2 < kHalo3ObserverCaptureFloats,
                  "the position, focus and forward triplets must sit inside the capture window");
    const uint32_t seq = cap.seq.load(std::memory_order_acquire);
    if (seq & 1u)
        return false;
    const float at[3] = {cap.floats[positionIndex], cap.floats[positionIndex + 1], cap.floats[positionIndex + 2]};
    const float about[3] = {cap.floats[focusIndex], cap.floats[focusIndex + 1], cap.floats[focusIndex + 2]};
    float along[3] = {cap.floats[forwardIndex], cap.floats[forwardIndex + 1], cap.floats[forwardIndex + 2]};
    if (cap.seq.load(std::memory_order_acquire) != seq)
        return false;
    if (!vehicle_camera_place::Finite3(at) || !vehicle_camera_place::Finite3(about))
        return false;
    if (forward)
    {
        if (!vehicle_camera_place::Finite3(along) || !vehicle_camera_frame::Normalize(along))
            return false;
        memcpy(forward, along, sizeof(along));
    }
    memcpy(position, at, sizeof(at));
    memcpy(focus, about, sizeof(about));
    return true;
}

// WHERE THE GAME'S OWN THIRD-PERSON CAMERA RESTS in the seat ("origin": "thirdperson"). The game's camera
// swings about its pivot with the aim; where it RESTS is that pivot, its distance from it, and its height above
// it when it looks level (measured headless in a Mongoose: 474 cm from the pivot, 151 cm of that above it,
// looking level). The distance is the furthest the game's camera has stood off in this seat while looking within
// kHalo3RestLevelSin of level: the game pulls its camera in from walls and through its change of view, the
// furthest it stands is the distance it means, and how high it stands depends on where the player aims. Until it
// has looked level, the furthest it has stood at any aim is used. Camera thread.
struct Halo3VehicleCameraRest
{
    VehicleCameraSeatKey seat{};
    float distance = 0.0f; // world units
    float rise = 0.0f;     // of that distance, the height above the pivot along the vehicle's up
    bool level = false;    // the distance and height were taken while the game's camera looked level
    uint32_t waited = 0;   // placements asked for before a distance was known
};
Halo3VehicleCameraRest g_halo3VehicleCameraRest;
// The resting distance in use, world units (0 = none yet), for the cold worker's log line, with the shape of the
// game's camera where it stood furthest: its height above its pivot along the vehicle's up, the pitch it looked
// at, and the pivot's own height above the vehicle's origin.
std::atomic<float> g_halo3VehicleCameraRestWu{0.0f};
std::atomic<float> g_halo3VehicleCameraRestUpWu{0.0f};
std::atomic<float> g_halo3VehicleCameraRestPitchDeg{0.0f};
std::atomic<float> g_halo3VehicleCameraRestPivotUpWu{0.0f};
inline constexpr float kHalo3RestKnownWu = 0.25f; // nearer than this the director is still in the seat's own view
inline constexpr float kHalo3RestFurthestWu = 8.0f;
inline constexpr float kHalo3RestPivotReachWu = 10.0f; // a pivot further than this from the vehicle is not its seat's
inline constexpr uint32_t kHalo3RestWaitFrames = 45;
inline constexpr float kHalo3RestLevelSin = 0.26f; // within about 15 degrees of level

// Fills `in` with the resting place. False: not known YET, and the seat keeps the view it has for this frame.
// True with in.restValid false: it could not be read at all, and the caller's stand-in applies.
bool Halo3VehicleCameraRestPlace(const VehicleCameraSeatKey& seat, vehicle_camera_place::Inputs& in)
{
    Halo3VehicleCameraRest& rest = g_halo3VehicleCameraRest;
    if (!(rest.seat == seat))
    {
        rest = {};
        rest.seat = seat;
    }
    float at[3], about[3], looks[3];
    bool observed = Halo3ReadObserverCamera(at, about, looks);
    if (observed)
    {
        const float fromVehicle[3] = {about[0] - in.vehicleOrigin[0], about[1] - in.vehicleOrigin[1],
                                      about[2] - in.vehicleOrigin[2]};
        observed = vehicle_camera_frame::Dot(fromVehicle, fromVehicle) <= kHalo3RestPivotReachWu * kHalo3RestPivotReachWu;
    }
    if (observed)
    {
        const float stood[3] = {at[0] - about[0], at[1] - about[1], at[2] - about[2]};
        const float distance = sqrtf(vehicle_camera_frame::Dot(stood, stood));
        const float lookUp = vehicle_camera_frame::Dot(looks, in.vehicle.up);
        const bool level = std::fabs(lookUp) <= kHalo3RestLevelSin;
        // The first level sample replaces whatever was taken at another aim; after that only a level one that
        // stands further does.
        const bool take = std::isfinite(distance) && distance <= kHalo3RestFurthestWu &&
                          (level ? (!rest.level || distance > rest.distance) : (!rest.level && distance > rest.distance));
        if (take)
        {
            rest.distance = distance;
            rest.rise = vehicle_camera_frame::Dot(stood, in.vehicle.up);
            rest.level = level;
            const float pivot[3] = {about[0] - in.vehicleOrigin[0], about[1] - in.vehicleOrigin[1],
                                    about[2] - in.vehicleOrigin[2]};
            g_halo3VehicleCameraRestUpWu.store(rest.rise, std::memory_order_relaxed);
            g_halo3VehicleCameraRestPitchDeg.store(
                asinf(lookUp < -1.0f ? -1.0f : (lookUp > 1.0f ? 1.0f : lookUp)) * 57.29578f,
                std::memory_order_relaxed);
            g_halo3VehicleCameraRestPivotUpWu.store(vehicle_camera_frame::Dot(pivot, in.vehicle.up),
                                                    std::memory_order_relaxed);
        }
    }
    if (observed && rest.distance >= kHalo3RestKnownWu)
    {
        in.restValid = true;
        memcpy(in.rest, about, sizeof(in.rest));
        in.restDistance = rest.distance;
        in.restRise = std::isfinite(rest.rise) ? rest.rise : 0.0f;
        g_halo3VehicleCameraRestWu.store(rest.distance, std::memory_order_relaxed);
        return true;
    }
    if (rest.waited < kHalo3RestWaitFrames)
    {
        ++rest.waited;
        return false;
    }
    in.restValid = false;
    return true;
}

// Camera thread, once a camera pass. `viewYaw` is the heading the player's room faces. True with `out` the
// world point the seat view is seen from: a chase camera is in use for exactly the seat the snapshot shows.
bool Halo3VehicleCameraPlace(float viewYaw, float out[3])
{
    g_halo3VehicleCameraPass.placed = false;
    vehicle_camera_select::ActiveCamera camera;
    Halo3SeatSnapshot seat;
    VehicleCameraSeatKey key;
    if (!Halo3VehicleCameraSelected(camera, seat, key))
        return false;

    vehicle_camera_place::Inputs in;
    bool drawn = seat.rootValid;
    if (drawn)
    {
        memcpy(in.vehicleOrigin, seat.rootPos, sizeof(in.vehicleOrigin));
        memcpy(in.vehicle.forward, seat.rootFwd, sizeof(in.vehicle.forward));
        memcpy(in.vehicle.left, seat.rootLeft, sizeof(in.vehicle.left));
        memcpy(in.vehicle.up, seat.rootUp, sizeof(in.vehicle.up));
        drawn = vehicle_camera_frame::Usable(in.vehicle);
    }
    if (!drawn)
    {
        if (!vehicle_camera_frame::FromForwardUp(seat.hullFwd, seat.hullUp, in.vehicle))
            return false;
        memcpy(in.vehicleOrigin, seat.hullPos, sizeof(in.vehicleOrigin));
    }
    // A vehicle the mod has no seat point for (one it does not know) has neither of these; a camera measured
    // from one then falls back to the vehicle's origin.
    in.seatValid = seat.anchorBaseValid;
    memcpy(in.seat, seat.anchorBase, sizeof(in.seat));
    in.headValid = seat.anchorValid;
    memcpy(in.head, seat.anchor, sizeof(in.head));
    in.viewYaw = viewYaw;
    if (camera.origin == static_cast<uint8_t>(vehicle_camera_file::Origin::ThirdPerson) &&
        !Halo3VehicleCameraRestPlace(key, in))
        return false;

    float point[3];
    vehicle_camera_place::Placed placed;
    if (!vehicle_camera_place::Place(camera, in, g_halo3VehicleCameraState, point, &placed))
        return false;
    const float* pivot = placed.pivot;
    float reachUsed = 1.0f;

    // LAG ("lag"): the point follows its place on a spring (vehicle_camera_place::FollowLag), on the frame's own
    // clock -- a millisecond tick is a sixth of a frame at 90 Hz. Before the wall check, so a wall still holds the
    // lagged camera short.
    {
        static LARGE_INTEGER frequency{};
        if (!frequency.QuadPart)
            QueryPerformanceFrequency(&frequency);
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        const float seconds = g_halo3VehicleCameraLagAt.QuadPart && frequency.QuadPart
            ? static_cast<float>(static_cast<double>(now.QuadPart - g_halo3VehicleCameraLagAt.QuadPart) /
                                 static_cast<double>(frequency.QuadPart))
            : -1.0f;
        g_halo3VehicleCameraLagAt = now;
        vehicle_camera_place::FollowLag(camera, point, seconds, g_halo3VehicleCameraLag, point);
    }

    // HELD SHORT OF A WALL ("collide"): the line from the pivot to the camera's place is traced by the title's own
    // simulation thread beside the aim's ray (legacy_sight_trace.inl), and the camera stops short of what it meets.
    // The trace is a tick or two behind the line it is read against; at the speed a camera moves that is a few
    // centimetres. With no trace to go on the camera goes all the way, as before.
    if (camera.collide)
    {
        const float toward[3] = {point[0] - pivot[0], point[1] - pivot[1], point[2] - pivot[2]};
        const float length = sqrtf(vehicle_camera_frame::Dot(toward, toward));
        float wanted = 1.0f;
        if (std::isfinite(length) && length > 0.05f)
        {
            const uint32_t generation = g_halo3RuntimeGeneration.load(std::memory_order_relaxed);
            SightAuxRay ray{};
            ray.generation = generation;
            for (int axis = 0; axis < 3; ++axis)
            {
                ray.origin[axis] = pivot[axis];
                ray.dir[axis] = toward[axis] / length;
            }
            Sight_PublishCameraRay(GameTitle::Halo3, ray);
            bool hit = false;
            float rangeWu = 0.0f;
            if (Sight_ReadCameraTrace(GameTitle::Halo3, generation, 150, hit, rangeWu))
            {
                wanted = vehicle_camera_place::CollideReach(hit, rangeWu, length, camera.collideMarginCm);
                g_halo3VehicleCameraCollideFrames.fetch_add(1, std::memory_order_relaxed);
            }
            else
            {
                g_halo3VehicleCameraNoTraceFrames.fetch_add(1, std::memory_order_relaxed);
            }
        }
        const uint64_t nowMs = GetTickCount64();
        const float seconds = g_halo3VehicleCameraReachAt && nowMs > g_halo3VehicleCameraReachAt
            ? static_cast<float>(nowMs - g_halo3VehicleCameraReachAt) * 0.001f
            : 0.0f;
        g_halo3VehicleCameraReachAt = nowMs;
        const float reach = vehicle_camera_place::EaseReach(g_halo3VehicleCameraReach, wanted,
                                                            seconds > 0.1f ? 0.1f : seconds,
                                                            kHalo3CameraReleasePerSecond);
        if (reach < 0.999f)
        {
            for (int axis = 0; axis < 3; ++axis)
                point[axis] = pivot[axis] + toward[axis] * reach;
            g_halo3VehicleCameraHeldFrames.fetch_add(1, std::memory_order_relaxed);
            reachUsed = reach;
        }
    }
    else
    {
        g_halo3VehicleCameraReach = 1.0f;
    }

    g_halo3VehicleCameraPass.placed = true;
    g_halo3VehicleCameraPass.reach = reachUsed;
    g_halo3VehicleCameraPass.camera = camera;
    g_halo3VehicleCameraPass.where = placed;

    Halo3VehicleCameraPoint& published = g_halo3VehicleCameraPoint;
    published.seq.fetch_add(1, std::memory_order_acq_rel);
    memcpy(published.point, point, sizeof(published.point));
    published.ms.store(GetTickCount64(), std::memory_order_relaxed);
    published.seq.fetch_add(1, std::memory_order_release);
    g_halo3VehicleCameraFrames.fetch_add(1, std::memory_order_relaxed);
    if (!drawn)
        g_halo3VehicleCameraTickFrames.fetch_add(1, std::memory_order_relaxed);
    if (g_halo3VehicleCameraState.standIn)
        g_halo3VehicleCameraStandInFrames.fetch_add(1, std::memory_order_relaxed);
    memcpy(out, point, sizeof(point));
    return true;
}

void Halo3VehicleCameraBeginPass()
{
    g_halo3VehicleCameraPass.placed = false;
}

// Camera thread, after Halo3VehicleCameraPlace in the same pass: the player's head movement `head` (world units,
// added to the camera's point) held inside the camera's leash. False when this pass placed no chase camera or the
// camera sets no leash.
bool Halo3VehicleCameraLeashHead(float head[3])
{
    const Halo3VehicleCameraPass& pass = g_halo3VehicleCameraPass;
    return pass.placed && vehicle_camera_place::LeashHead(pass.camera, pass.where, pass.reach, head);
}

// Any thread. The point the camera thread last placed, while it is fresh.
bool Halo3VehicleCameraPublishedPoint(float out[3])
{
    if (!g_vehicleCameraPublished.chase.load(std::memory_order_acquire))
        return false;
    const Halo3VehicleCameraPoint& published = g_halo3VehicleCameraPoint;
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        const uint32_t before = published.seq.load(std::memory_order_acquire);
        if (!before || (before & 1u))
            continue;
        float point[3];
        memcpy(point, published.point, sizeof(point));
        const uint64_t ms = published.ms.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (published.seq.load(std::memory_order_relaxed) != before)
            continue;
        if (GetTickCount64() - ms > kHalo3VehicleCameraPointFreshMs)
            return false;
        memcpy(out, point, sizeof(point));
        return true;
    }
    return false;
}

// The cold worker's reading of the Halo 3 seat. Unknown: seated, but this moment's snapshot could not be read;
// the answer of the last tick stands. A vehicle the mod does not know is a seat like any other here: its name
// is "unidentified", and the file's "default" entry is the one it gets.
// `seatView`: the mod has a first-person seat point for this seat, so the seat view is a view of its own; a
// vehicle it does not know (a custom one) has none and starts in third person. `wheelSeat`: the driver's seat of
// a look-steered ground vehicle, where the wheel can be taken.
enum class VehicleCameraSeatRead { OnFoot, Seated, Unknown };
VehicleCameraSeatRead Halo3VehicleCameraSeat(VehicleCameraSeatKey& key, const char*& name, uint8_t& seatBits,
                                             bool& seatView, bool& wheelSeat)
{
    if (!Halo3VehicleFpActive())
        return VehicleCameraSeatRead::OnFoot;
    Halo3SeatSnapshot seat;
    if (!Halo3ReadSeatSnapshot(seat))
        return VehicleCameraSeatRead::Unknown;
    const auto id = static_cast<Halo3VehicleId>(seat.identity);
    const bool turret = id == Halo3VehicleId::StationaryTurret;
    key = VehicleCameraSeatKey{g_halo3RuntimeGeneration.load(std::memory_order_relaxed), seat.parentHandle,
                               seat.seatIndex, seat.identity, seat.mounted};
    // A gunner on a vehicle's own gun is named by the vehicle that carries it; a turret the player walks up to
    // is driven and fired from its one seat.
    name = turret ? "turret" : Halo3VehicleIdName(seat.identity);
    seatBits = vehicle_camera_file::SeatBits(turret || Halo3SeatIsDriver(id, seat.seatIndex, seat.mounted),
                                             turret || seat.mounted);
    seatView = Halo3FindSeatPoint(id, seat.seatIndex, seat.mounted) != nullptr;
    wheelSeat = Halo3SeatAuthorsSteering(id, seat.seatIndex, seat.mounted, true) && Halo3VehicleUsesWheel(id);
    return VehicleCameraSeatRead::Seated;
}

// What a selected camera asks for that Halo 3 does not do yet, for the log; empty when it asks for nothing of
// the kind.
std::string Halo3VehicleCameraNotApplied(const vehicle_camera_select::ActiveCamera& camera)
{
    if (!camera.valid || !camera.chase)
        return {};
    std::string list;
    const auto add = [&list](const char* what) {
        if (!list.empty())
            list += "; ";
        list += what;
    };
    if (camera.rotRoll)
        add("rotationTracking roll (the horizon is never rolled)");
    if (camera.rotPitch && !camera.rotYaw)
        add("rotationTracking pitch without yaw");
    if (camera.origin == static_cast<uint8_t>(vehicle_camera_file::Origin::Marker))
        add("an origin naming a marker (the vehicle's origin is used)");
    return list;
}

// ---- The cold worker --------------------------------------------------------------------------------------

// THE SEAT MEMORY ON DISK (Selector::MemoryText): halomccvr_vehicle_cameras_last.txt beside the camera file,
// the player's own, written by the mod and never shipped. Replaced whole through a temporary file, so a reader
// never finds half of one.
inline constexpr wchar_t kVehicleCameraMemoryFile[] = L"halomccvr_vehicle_cameras_last.txt";
inline constexpr uint64_t kVehicleCameraMemorySaveMs = 2000;

bool VehicleCameraReadText(const std::wstring& path, std::string& out)
{
    out.clear();
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file)
        return false;
    char block[4096];
    size_t got = 0;
    while ((got = fread(block, 1, sizeof(block), file)) > 0 && out.size() < (1u << 20))
        out.append(block, got);
    fclose(file);
    return true;
}

bool VehicleCameraWriteText(const std::wstring& path, const std::string& text)
{
    const std::wstring temporary = path + L".tmp";
    FILE* file = nullptr;
    if (_wfopen_s(&file, temporary.c_str(), L"wb") != 0 || !file)
        return false;
    const bool whole = fwrite(text.data(), 1, text.size(), file) == text.size();
    const bool closed = fclose(file) == 0;
    if (!whole || !closed || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        DeleteFileW(temporary.c_str());
        return false;
    }
    return true;
}

struct VehicleCameraRuntime
{
    VehicleCameraRuntime(std::wstring path, std::wstring memory) : store(std::move(path)), memoryPath(std::move(memory)) {}
    vehicle_camera_store::Store store;
    vehicle_camera_select::Selector selector;
    std::wstring memoryPath;
    uint64_t nextMemorySaveMs = 0;
    bool memoryFailureLogged = false;
    bool wheelSeat = false;
    uint8_t seatBits = 0;
    uint64_t nextFilePollMs = 0;
    VehicleCameraSeatKey seat{};
    vehicle_camera_select::ActiveCamera publishedCamera{};
    VehicleCameraSeatKey publishedSeat{};
    std::string loggedLine;
    std::string readout;
    int frameSourceLogged = -1; // 0 = the drawn root node, 1 = the simulation's frame
    bool standInLogged = false;
    bool noTraceLogged = false;
    float restLoggedWu = 0.0f;
    float restSeenWu = 0.0f;
    uint32_t heldFramesStint = 0;
};
inline constexpr uint64_t kVehicleCameraFilePollMs = 2000;

// WaitThread, every poll. Everything slow about the feature happens here.
void VehicleCameras_ColdTick(uint64_t nowMs)
{
    static std::unique_ptr<VehicleCameraRuntime> runtime;
    static bool wasOn = false;
    if (!g_config.vehicle_cameras)
    {
        if (wasOn)
        {
            wasOn = false;
            VehicleCameraPublish({}, {});
            VehicleCameraSetReadout({});
            g_vehicleCameraStepCamera.store(0, std::memory_order_relaxed);
            g_vehicleCameraStepMode.store(0, std::memory_order_relaxed);
            g_vehicleControlStep.store(0, std::memory_order_relaxed);
            g_vehicleControlEffective.store(-1, std::memory_order_release);
            g_vehicleSeatButtons.store(0, std::memory_order_release);
            if (runtime)
            {
                runtime->selector.Update("", false, "", 0);
                runtime->publishedCamera = {};
                runtime->publishedSeat = {};
                runtime->loggedLine.clear();
                runtime->frameSourceLogged = -1;
                runtime->standInLogged = false;
                runtime->noTraceLogged = false;
            }
            LOG("Vehicle cameras: off (vehicle_cameras = 0); every seat keeps the view it has without them");
        }
        return;
    }
    if (!runtime)
    {
        std::wstring path = LogDirectory();
        if (path.empty())
            return;
        std::wstring memory = path + kVehicleCameraMemoryFile;
        path += vehicle_camera_store::kFileName;
        runtime = std::make_unique<VehicleCameraRuntime>(std::move(path), std::move(memory));
        std::string kept;
        if (VehicleCameraReadText(runtime->memoryPath, kept))
            LOG("Vehicle cameras: your last camera, tethering mode and controls kept for %d seat(s), from "
                "halomccvr_vehicle_cameras_last.txt",
                runtime->selector.LoadMemory(kept));
    }
    VehicleCameraRuntime& r = *runtime;
    if (!wasOn)
    {
        wasOn = true;
        r.nextFilePollMs = 0;
        LOG("Vehicle cameras: on. The cameras of each seat are read from %s in the mod's folder; F1 > Vehicles "
            "steps through them. Halo 3 seats for now, and only while vehicle_first_person is on",
            vehicle_camera_store::kFileNameText);
    }

    if (nowMs >= r.nextFilePollMs)
    {
        r.nextFilePollMs = nowMs + kVehicleCameraFilePollMs;
        const vehicle_camera_store::PollResult polled = r.store.Poll();
        for (const std::string& note : polled.notes)
            LOG("%s", note.c_str());
        if (polled.changed)
            r.selector.SetTable(r.store.Current());
    }

    VehicleCameraSeatRead read = VehicleCameraSeatRead::OnFoot;
    VehicleCameraSeatKey key{};
    const char* name = "";
    uint8_t seatBits = 0;
    const char* gameKey = "";
    bool seatView = true, wheelSeat = false;
    const bool halo3 = TitleAdapter_GetActiveTitle() == GameTitle::Halo3;
    if (halo3)
    {
        gameKey = "halo3";
        read = Halo3VehicleCameraSeat(key, name, seatBits, seatView, wheelSeat);
    }
    if (read == VehicleCameraSeatRead::Seated)
    {
        r.selector.Update(gameKey, true, name, seatBits, seatView);
        r.seat = key;
        r.seatBits = seatBits;
        r.wheelSeat = wheelSeat;
    }
    else if (read == VehicleCameraSeatRead::OnFoot)
    {
        r.selector.Update(gameKey, false, "", 0);
        r.seat = {};
        r.seatBits = 0;
        r.wheelSeat = false;
    }

    // A step asked for on foot is dropped with the rest: the selector has nothing to step.
    const int stepCamera = g_vehicleCameraStepCamera.exchange(0, std::memory_order_relaxed);
    const int stepMode = g_vehicleCameraStepMode.exchange(0, std::memory_order_relaxed);
    if (stepCamera)
        r.selector.StepCamera(stepCamera);
    if (stepMode)
        r.selector.StepMode(stepMode);

    // HOW THE SEAT IS CONTROLLED: the player's choice for it where he made one (the left stick click, kept per
    // vehicle and seat), else the setting.
    const bool inSeat = r.selector.Active().valid;
    const int base = static_cast<int>(vehicle_control::FromConfig(g_config.vehicle_control));
    int control = base;
    if (inSeat)
    {
        const int choice = r.selector.ControlChoice(base);
        if (choice >= 0)
            control = choice;
        if (g_vehicleControlStep.exchange(0, std::memory_order_relaxed) != 0)
        {
            const vehicle_control::Mode next =
                vehicle_control::Next(vehicle_control::FromConfig(control), r.wheelSeat);
            control = static_cast<int>(next);
            r.selector.SetControlChoice(control, base);
            LOG("Vehicle cameras: %s is now controlled by %s (left stick click)", name[0] ? name : "this seat",
                vehicle_control::Name(next));
        }
    }
    else
    {
        g_vehicleControlStep.store(0, std::memory_order_relaxed);
    }
    g_vehicleControlEffective.store(inSeat ? control : -1, std::memory_order_release);
    g_vehicleSeatButtons.store(
        inSeat && halo3 ? kVehicleSeatButtonsLive |
                              ((r.seatBits & vehicle_camera_file::kSeatPassenger) ? kVehicleSeatPassenger : 0u)
                        : 0u,
        std::memory_order_release);

    if (r.selector.MemoryDirty() && nowMs >= r.nextMemorySaveMs)
    {
        r.nextMemorySaveMs = nowMs + kVehicleCameraMemorySaveMs;
        if (VehicleCameraWriteText(r.memoryPath, r.selector.MemoryText()))
        {
            r.selector.MemorySaved();
            r.memoryFailureLogged = false;
        }
        else if (!r.memoryFailureLogged)
        {
            r.memoryFailureLogged = true;
            LOG("Vehicle cameras: halomccvr_vehicle_cameras_last.txt could not be written (%lu); your choices are "
                "kept for this session and tried again",
                GetLastError());
        }
    }

    const vehicle_camera_select::ActiveCamera& active = r.selector.Active();
    if (!(active == r.publishedCamera) || !(r.seat == r.publishedSeat))
    {
        VehicleCameraPublish(active, r.seat);
        r.publishedCamera = active;
        r.publishedSeat = r.seat;
    }

    const bool onFoot = !active.valid && r.selector.StandDownReason() == vehicle_camera_select::kOnFoot;
    std::string line = r.selector.Describe();
    if (line.empty())
    {
        // Not in a seat this feature reaches: say which of the reasons it is.
        if (!onFoot)
            line = "no camera: " + r.selector.StandDownReason();
        else if (!halo3)
            line = "no camera: vehicle cameras are in Halo 3 only for now";
        else if (!g_config.vehicle_first_person)
            line = "no camera: first-person vehicles are off, so a seat keeps the game's own camera";
        else
            line = "no camera: on foot";
    }
    else
    {
        line += std::string("; controls: ") + vehicle_control::Name(vehicle_control::FromConfig(control));
        if (const std::string missing = Halo3VehicleCameraNotApplied(active); !missing.empty())
            line += ". Not applied in Halo 3 yet: " + missing;
    }
    if (line != r.readout)
    {
        r.readout = line;
        VehicleCameraSetReadout(line);
    }
    // Getting out is not news; a seat's camera, a step, and a seat the file gives no camera are.
    if (!onFoot && line != r.loggedLine)
    {
        r.loggedLine = line;
        LOG("Vehicle cameras: %s", line.c_str());
    }
    if (onFoot)
    {
        r.loggedLine.clear();
        r.frameSourceLogged = -1;
        r.standInLogged = false;
        r.noTraceLogged = false;
        if (r.heldFramesStint)
            LOG("Vehicle cameras (Halo 3): in that seat the camera was held short of something on %u frames",
                r.heldFramesStint);
        r.heldFramesStint = 0;
        r.restLoggedWu = 0.0f;
        r.restSeenWu = 0.0f;
        g_halo3VehicleCameraRestWu.store(0.0f, std::memory_order_relaxed);
    }

    const uint32_t frames = g_halo3VehicleCameraFrames.exchange(0, std::memory_order_relaxed);
    const uint32_t tickFrames = g_halo3VehicleCameraTickFrames.exchange(0, std::memory_order_relaxed);
    const uint32_t standInFrames = g_halo3VehicleCameraStandInFrames.exchange(0, std::memory_order_relaxed);
    const uint32_t collideFrames = g_halo3VehicleCameraCollideFrames.exchange(0, std::memory_order_relaxed);
    const uint32_t noTraceFrames = g_halo3VehicleCameraNoTraceFrames.exchange(0, std::memory_order_relaxed);
    r.heldFramesStint += g_halo3VehicleCameraHeldFrames.exchange(0, std::memory_order_relaxed);
    // The game's own third-person camera's resting distance: said once it has settled in a seat (the same on two
    // polls running; it grows over the first half second as the game's camera eases out), and again whenever it
    // has settled a tenth further out (the distance is the furthest the game's camera has stood).
    const float restWu = g_halo3VehicleCameraRestWu.load(std::memory_order_relaxed);
    const bool restSettled = restWu == r.restSeenWu;
    r.restSeenWu = restWu;
    if (restWu > 0.0f && restSettled && restWu > r.restLoggedWu * 1.1f)
    {
        r.restLoggedWu = restWu;
        const float upWu = g_halo3VehicleCameraRestUpWu.load(std::memory_order_relaxed);
        const float pivotUpWu = g_halo3VehicleCameraRestPivotUpWu.load(std::memory_order_relaxed);
        LOG("Vehicle cameras (Halo 3): the game's own third-person camera rests %.2f world units (%.0f cm) from "
            "its pivot in this seat, %.0f cm of that above the pivot (it looked %.1f deg up), "
            "and the pivot is %.0f cm above the vehicle's origin",
            static_cast<double>(restWu), static_cast<double>(restWu / vehicle_camera_frame::kWorldUnitsPerCm),
            static_cast<double>(upWu / vehicle_camera_frame::kWorldUnitsPerCm),
            static_cast<double>(g_halo3VehicleCameraRestPitchDeg.load(std::memory_order_relaxed)),
            static_cast<double>(pivotUpWu / vehicle_camera_frame::kWorldUnitsPerCm));
    }
    if (noTraceFrames > 60 && collideFrames == 0 && !r.noTraceLogged)
    {
        r.noTraceLogged = true;
        LOG("Vehicle cameras (Halo 3): this camera asks to be kept out of walls (\"collide\"), but the game's trace is "
            "not running for this seat, so it goes all the way out");
    }
    if (standInFrames && !r.standInLogged)
    {
        r.standInLogged = true;
        LOG("Vehicle cameras (Halo 3): where the game's own third-person camera rests could not be read for "
            "this seat, so the camera stands %.0f cm behind and %.0f cm above the vehicle's origin instead",
            static_cast<double>(vehicle_camera_place::kStandInBehindCm),
            static_cast<double>(vehicle_camera_place::kStandInAboveCm));
    }
    if (frames)
    {
        const int source = tickFrames * 2 > frames ? 1 : 0;
        if (source != r.frameSourceLogged)
        {
            r.frameSourceLogged = source;
            if (source == 0)
                LOG("Vehicle cameras (Halo 3): the camera is placed from the vehicle's root node as it is drawn");
            else
                LOG("Vehicle cameras (Halo 3): the vehicle's drawn root node did not check out against its own "
                    "frame, so the camera is placed from the simulation's frame; it can shake against the vehicle "
                    "at speed");
        }
    }
}
