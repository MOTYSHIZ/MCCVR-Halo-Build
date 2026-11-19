// PROJECT RECLAIMER: ITS PROMPTS SHOW THE CONTROLLER'S BUTTONS AFTER THE MOD'S OWN CLICKS (included by game.cpp, after
// the VISR camera).
//
// The client draws its button prompts, the game's training hints among them, for the device it last saw: a mouse click
// or a key in its menus makes it "keyboard and mouse", and only a controller button, or a stick pushed past the
// deadzone, in a menu makes it "controller" again (its log: "[menus] input device: ..."). The mod's menu pointer
// clicks with the mouse (native_menu_pointer.cpp), and its head gestures send keys (input.cpp StepHeadKey). So when a
// pointer click was the last thing done in a menu, the game that followed showed keyboard pictures in its prompts
// (user, 2026-11-19, ODST's Mombasa Streets; MCC was fine).
//
// The client keeps the choice in two globals of its main loop (reclaimer_client::FindInputDevice): the device (1
// keyboard and mouse, 2 controller) and a controller flag its prompts read. Once the game is being played again, a
// moment after the mod's own click or key, and only if no real keyboard or mouse was used since, the mod sets both as
// the client's own controller press does. That is the client's choice of pictures, nothing the game simulates. The
// client's next real mouse click or key in a menu sets them back as it always has.

std::atomic<volatile uint8_t*> g_reclaimerInputDevice{nullptr};
std::atomic<volatile uint8_t*> g_reclaimerInputController{nullptr};
// GetTickCount64 of the last mouse click, wheel or key the mod sent itself (0: none waiting).
std::atomic<uint64_t> g_reclaimerSyntheticInputAt{0};
std::atomic<uint32_t> g_reclaimerInputDeviceRestores{0};
// The client takes the mod's event first; then the mod puts the controller back.
constexpr uint64_t kReclaimerInputDeviceSettleMs = 150;
// Windows' own record of the last keyboard or mouse input this long after the mod's: a real one, whose choice stands.
constexpr DWORD kReclaimerRealInputSlackMs = 250;

// Install thread, once per process: the client's input device globals. False with the reason logged when not found;
// the prompts then keep whatever device the client last saw.
bool InstallReclaimerInputDevice()
{
    static bool tried = false;
    if (tried || !g_hostIsProjectReclaimer || !lifted::Active())
        return g_reclaimerInputController.load(std::memory_order_relaxed) != nullptr;
    tried = true;
    reclaimer_client::Image image;
    if (!reclaimer_client::ClientImageInMemory(image))
    {
        LOG("Project Reclaimer prompts: StockFallback: the client executable's headers do not read; a click of the "
            "menu pointer can leave keyboard pictures in the prompts");
        return false;
    }
    const reclaimer_client::InputDevice found = reclaimer_client::FindInputDevice(image);
    if (found.missing)
    {
        LOG("Project Reclaimer prompts: StockFallback: its input device: %s not found; a click of the menu pointer can "
            "leave keyboard pictures in the prompts", found.missing);
        return false;
    }
    const auto exe = reinterpret_cast<uintptr_t>(image.base);
    g_reclaimerInputDevice.store(reinterpret_cast<volatile uint8_t*>(exe + found.device), std::memory_order_relaxed);
    g_reclaimerInputController.store(reinterpret_cast<volatile uint8_t*>(exe + found.controller),
                                     std::memory_order_release);
    LOG("Project Reclaimer prompts: the input device they show is found (device exe+0x%X, controller flag exe+0x%X, set "
        "at exe+0x%X and exe+0x%X): after the mod's own clicks and keys they go back to the controller's buttons",
        found.device, found.controller, found.controllerAt, found.keyboardAt);
    return true;
}

void ReclaimerNoteSyntheticInput() noexcept
{
    if (g_hostIsProjectReclaimer)
        g_reclaimerSyntheticInputAt.store(GetTickCount64(), std::memory_order_relaxed);
}

// The headset's input loop, every poll: `playing` while the game is played (no menu, no pause).
void ReclaimerInputDeviceTick(bool playing) noexcept
{
    const uint64_t at = g_reclaimerSyntheticInputAt.load(std::memory_order_relaxed);
    if (!at || !playing)
        return;
    const uint64_t now = GetTickCount64();
    if (now < at || now - at < kReclaimerInputDeviceSettleMs)
        return;
    uint64_t expected = at;
    if (!g_reclaimerSyntheticInputAt.compare_exchange_strong(expected, 0, std::memory_order_relaxed))
        return;   // the mod sent another since: the next poll looks again
    volatile uint8_t* const controller = g_reclaimerInputController.load(std::memory_order_acquire);
    volatile uint8_t* const device = g_reclaimerInputDevice.load(std::memory_order_relaxed);
    if (!controller || !device || *controller)
        return;
    LASTINPUTINFO last{sizeof(last)};
    if (GetLastInputInfo(&last) && static_cast<int32_t>(last.dwTime - static_cast<DWORD>(at)) >
                                       static_cast<int32_t>(kReclaimerRealInputSlackMs))
        return;   // a real keyboard or mouse was used after the mod's own input
    *device = 2;
    *controller = 1;
    const uint32_t restores = g_reclaimerInputDeviceRestores.fetch_add(1, std::memory_order_relaxed) + 1;
    if (restores <= 8)
        LOG("Project Reclaimer prompts: back to the controller's buttons after the mod's own click or key (%u)", restores);
}
