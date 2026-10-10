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
// touched. The view's turning is unchanged: vehicle_view_follow governs it.
// Not applied in this game yet: a mode's rotationTracking, the leash, collide, and an origin naming a marker
// (the vehicle's origin stands in). The cold worker says so in the log for a camera that asks for one.

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
// Frames placed, and those of them placed from the simulation's vehicle frame because the drawn root node did
// not check out. Counted by the camera thread, taken by the cold worker for its log line.
std::atomic<uint32_t> g_halo3VehicleCameraFrames{0};
std::atomic<uint32_t> g_halo3VehicleCameraTickFrames{0};

// Camera thread, once a camera pass. `viewYaw` is the heading the player's room faces. True with `out` the
// world point the seat view is seen from: a chase camera is in use for exactly the seat the snapshot shows.
bool Halo3VehicleCameraPlace(float viewYaw, float out[3])
{
    vehicle_camera_select::ActiveCamera camera;
    VehicleCameraSeatKey chosenFor;
    if (!VehicleCameraRead(camera, chosenFor))
        return false;
    Halo3SeatSnapshot seat;
    if (!Halo3ReadSeatSnapshot(seat) || !seat.anchorValid)
        return false;
    const VehicleCameraSeatKey now{g_halo3RuntimeGeneration.load(std::memory_order_relaxed), seat.parentHandle,
                                   seat.seatIndex, seat.identity, seat.mounted};
    if (!(now == chosenFor))
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
    in.seatValid = seat.anchorBaseValid;
    memcpy(in.seat, seat.anchorBase, sizeof(in.seat));
    in.headValid = seat.anchorValid;
    memcpy(in.head, seat.anchor, sizeof(in.head));
    in.viewYaw = viewYaw;

    float point[3];
    if (!vehicle_camera_place::Place(camera, in, g_halo3VehicleCameraState, point))
        return false;

    Halo3VehicleCameraPoint& published = g_halo3VehicleCameraPoint;
    published.seq.fetch_add(1, std::memory_order_acq_rel);
    memcpy(published.point, point, sizeof(published.point));
    published.ms.store(GetTickCount64(), std::memory_order_relaxed);
    published.seq.fetch_add(1, std::memory_order_release);
    g_halo3VehicleCameraFrames.fetch_add(1, std::memory_order_relaxed);
    if (!drawn)
        g_halo3VehicleCameraTickFrames.fetch_add(1, std::memory_order_relaxed);
    memcpy(out, point, sizeof(point));
    return true;
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

// The seat sampler's question: is this seat's view a chase camera right now? It is once a point was placed, so
// the director keeps its first-person answer for a seat whose camera could not be placed.
bool Halo3VehicleCameraThirdPerson()
{
    float point[3];
    return Halo3VehicleCameraPublishedPoint(point);
}

// The cold worker's reading of the Halo 3 seat. Unknown: seated, but this moment's snapshot could not be read;
// the answer of the last tick stands.
enum class VehicleCameraSeatRead { OnFoot, Seated, Unknown };
VehicleCameraSeatRead Halo3VehicleCameraSeat(VehicleCameraSeatKey& key, const char*& name, uint8_t& seatBits)
{
    if (!Halo3VehicleFpActive())
        return VehicleCameraSeatRead::OnFoot;
    Halo3SeatSnapshot seat;
    if (!Halo3ReadSeatSnapshot(seat) || !seat.anchorValid)
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
    if (camera.rotYaw || camera.rotPitch || camera.rotRoll)
        add("rotationTracking (the view turns as vehicle_view_follow says)");
    if (vehicle_camera_file::LeashSet(camera.leashMinCm) || vehicle_camera_file::LeashSet(camera.leashMaxCm))
        add("the leash");
    if (camera.collide)
        add("collide (the camera is not pulled in from what stands in its way)");
    if (camera.origin == static_cast<uint8_t>(vehicle_camera_file::Origin::Marker))
        add("an origin naming a marker (the vehicle's origin is used)");
    return list;
}

// ---- The cold worker --------------------------------------------------------------------------------------

struct VehicleCameraRuntime
{
    explicit VehicleCameraRuntime(std::wstring path) : store(std::move(path)) {}
    vehicle_camera_store::Store store;
    vehicle_camera_select::Selector selector;
    uint64_t nextFilePollMs = 0;
    VehicleCameraSeatKey seat{};
    vehicle_camera_select::ActiveCamera publishedCamera{};
    VehicleCameraSeatKey publishedSeat{};
    std::string loggedLine;
    std::string readout;
    int frameSourceLogged = -1; // 0 = the drawn root node, 1 = the simulation's frame
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
            if (runtime)
            {
                runtime->selector.Update("", false, "", 0);
                runtime->publishedCamera = {};
                runtime->publishedSeat = {};
                runtime->loggedLine.clear();
                runtime->frameSourceLogged = -1;
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
        path += vehicle_camera_store::kFileName;
        runtime = std::make_unique<VehicleCameraRuntime>(std::move(path));
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
    const bool halo3 = TitleAdapter_GetActiveTitle() == GameTitle::Halo3;
    if (halo3)
    {
        gameKey = "halo3";
        read = Halo3VehicleCameraSeat(key, name, seatBits);
    }
    if (read == VehicleCameraSeatRead::Seated)
    {
        r.selector.Update(gameKey, true, name, seatBits);
        r.seat = key;
    }
    else if (read == VehicleCameraSeatRead::OnFoot)
    {
        r.selector.Update(gameKey, false, "", 0);
        r.seat = {};
    }

    // A step asked for on foot is dropped with the rest: the selector has nothing to step.
    const int stepCamera = g_vehicleCameraStepCamera.exchange(0, std::memory_order_relaxed);
    const int stepMode = g_vehicleCameraStepMode.exchange(0, std::memory_order_relaxed);
    if (stepCamera)
        r.selector.StepCamera(stepCamera);
    if (stepMode)
        r.selector.StepMode(stepMode);

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
    else if (const std::string missing = Halo3VehicleCameraNotApplied(active); !missing.empty())
        line += ". Not applied in Halo 3 yet: " + missing;
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
    }

    const uint32_t frames = g_halo3VehicleCameraFrames.exchange(0, std::memory_order_relaxed);
    const uint32_t tickFrames = g_halo3VehicleCameraTickFrames.exchange(0, std::memory_order_relaxed);
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
