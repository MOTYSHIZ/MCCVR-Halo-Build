#include "native_menu_pointer.h"
#include "../common/game_menu_pointer.h"
#include "../common/config.h"
#include "../common/log.h"
#include "menu.h"
#include "title_adapter.h"
#include "vr.h"
#include <atomic>

namespace {
HWND window = nullptr;
UINT messageId = 0;
std::atomic<uint64_t> sample{0};
std::atomic<bool> queued{false};
std::atomic<bool> delivered{false};
std::atomic<unsigned> postFailures{0};
std::atomic<uint64_t> visual{0};
std::atomic<unsigned> moveCount{0}, clickCount{0};
std::atomic<unsigned> cursorFailures{0};
std::atomic<const char*> frameStatus{"no-screen"}, deliveryStatus{"idle"};
// All remaining state belongs exclusively to MCC's window thread.
game_menu_pointer::Pointer pointer;
UINT_PTR timer = 0;
bool mouseDown = false;
bool faultLogged = false;

bool Allowed()
{
    return g_config.game_menu_pointer && !Menu_IsOpen() &&
        game_menu_pointer::MenuMode(TitleAdapter_GetRuntimeMode(),
            VR_IsPausePresentation() && VR_IsPausePresentationTarget()) &&
        !VR_IsCutsceneTheaterActive() && window &&
        GetForegroundWindow() == window && !IsIconic(window);
}
bool Button(bool down)
{
    if (mouseDown == down) return true;
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    if (SendInput(1, &input, sizeof(input)) != 1) return false;
    mouseDown = down;
    if (down) clickCount.fetch_add(1, std::memory_order_relaxed);
    return true;
}
void Fault()
{
    deliveryStatus.store("Windows-input-failed");
    if (!faultLogged) {
        LOG("game menu pointer: Windows mouse delivery unavailable; native menu controls retained");
        faultLogged = true;
    }
}
void Cancel()
{
    delivered.store(false, std::memory_order_release);
    visual.store(0, std::memory_order_release);
    pointer.Reset();
    if (!Button(false)) Fault();
    // Retry a failed release on the existing timer. Never forget a sent down.
    if (timer && !mouseDown) { KillTimer(nullptr, timer); timer = 0; }
}
void CALLBACK Watchdog(HWND, UINT, UINT_PTR, DWORD)
{
    if (!Allowed() || !game_menu_pointer::FreshActive(sample.load(), GetTickCount()))
        Cancel();
}
void Apply()
{
    RECT client{};
    const bool allowed = Allowed() && GetClientRect(window, &client);
    const auto result = pointer.Update(sample.load(std::memory_order_acquire),
        GetTickCount(), allowed, client.right-client.left, client.bottom-client.top);
    if (!result.move) { deliveryStatus.store(allowed ? "inactive-ray" : "focus-or-menu-gate"); Cancel(); return; }
    // Do not take over a physical mouse drag. An injected down is tracked
    // separately and always balanced on miss, focus loss, F1 or stale input.
    if (!mouseDown && (GetAsyncKeyState(VK_LBUTTON) & 0x8000)) { deliveryStatus.store("physical-drag"); Cancel(); return; }
    POINT target{result.x, result.y};
    if (!ClientToScreen(window, &target)) { Cancel(); Fault(); return; }
    const HWND under = WindowFromPoint(target);
    if (under != window && !IsChild(window, under)) { deliveryStatus.store("window-covered"); Cancel(); return; }
    if (!timer) timer = SetTimer(nullptr, 0, 50, Watchdog);
    if (!timer) { Cancel(); Fault(); return; }
    // Our DLL caller bypasses MCC's fitted-window coordinate remap. Send
    // physical client coordinates once; the existing MCC read hook scales them.
    POINT actual{};
    if (!SetCursorPos(target.x, target.y) || !GetCursorPos(&actual) ||
        std::abs(actual.x-target.x)>2 || std::abs(actual.y-target.y)>2) {
        Cancel(); Fault(); return;
    }
    if (!Button(result.pressed)) { Cancel(); Fault(); }
    else {
        moveCount.fetch_add(1, std::memory_order_relaxed);
        deliveryStatus.store("delivered");
        visual.store(game_menu_pointer::Pack(GetTickCount(), true,
            float(result.x) / (client.right-client.left),
            float(result.y) / (client.bottom-client.top), mouseDown), std::memory_order_release);
        delivered.store(true, std::memory_order_release);
    }
}
}

void NativeMenuPointer_Init(HWND hwnd)
{
    window = hwnd;
    messageId = RegisterWindowMessageW(L"HaloMCCVR.GameMenuPointer.20260916");
    if (!messageId) LOG("game menu pointer: message registration failed; native controls retained");
}
void NativeMenuPointer_Publish(bool active, float u, float v, bool pressed)
{
    const uint64_t next = game_menu_pointer::Pack(GetTickCount(), active, u, v, pressed);
    const uint64_t previous = sample.exchange(next, std::memory_order_acq_rel);
    // Default-off gameplay performs no Windows input or window messaging.
    if (!(next & (1ull<<31)) && !(previous & (1ull<<31))) return;
    if (messageId && window && !queued.exchange(true, std::memory_order_acq_rel)) {
        if (!PostMessageW(window, messageId, 0, 0)) {
            queued.store(false);
            delivered.store(false);
            postFailures.fetch_add(1, std::memory_order_relaxed);
        }
    }
}
bool NativeMenuPointer_ConsumesTrigger()
{
    return delivered.load(std::memory_order_acquire) && Allowed() &&
        game_menu_pointer::FreshActive(sample.load(), GetTickCount());
}
void NativeMenuPointer_ReportFailures()
{
    if(cursorFailures.exchange(0))
        LOG("game menu cursor: frame submission rejected; optional ring disabled for this session, native input and core VR retained");
    const unsigned failures = postFailures.exchange(0, std::memory_order_relaxed);
    if (failures) LOG("game menu pointer: %u window-message delivery failures; native controls retained", failures);
    static DWORD lastReport = 0;
    static int lastEnabled = -1;
    const int enabled = g_config.game_menu_pointer ? 1 : 0;
    const DWORD now = GetTickCount();
    if (lastEnabled != enabled || (enabled && DWORD(now-lastReport) >= 2000)) {
        lastReport = now; lastEnabled = enabled;
        LOG("game menu pointer: enabled=%d hand=%s mode=%u frame=%s delivery=%s active=%d moves=%u clicks=%u",
            enabled, g_config.left_handed ? "left" : "right", unsigned(TitleAdapter_GetRuntimeMode()),
            frameStatus.load(), deliveryStatus.load(),
            int(game_menu_pointer::FreshActive(visual.load(), now)), moveCount.load(), clickCount.load());
    }
}
void NativeMenuPointer_FrameStatus(const char* status) { frameStatus.store(status, std::memory_order_relaxed); }
void NativeMenuPointer_CursorFailed() { cursorFailures.fetch_add(1, std::memory_order_relaxed); }
bool NativeMenuPointer_ReadVisual(float& u, float& v, bool& pressed)
{
    const auto packet = visual.load(std::memory_order_acquire);
    if (!delivered.load(std::memory_order_acquire) ||
        !game_menu_pointer::FreshActive(packet, GetTickCount())) return false;
    u = float(packet & 32767) / 32767.0f;
    v = float((packet >> 15) & 32767) / 32767.0f;
    pressed = (packet & (1ull << 30)) != 0;
    return true;
}
bool NativeMenuPointer_Message(UINT msg)
{
    if (messageId && msg == messageId) {
        queued.store(false, std::memory_order_release);
        Apply();
        return true;
    }
    if (msg == WM_KILLFOCUS || msg == WM_DESTROY) {
        sample.store(0, std::memory_order_release);
        Cancel();
    }
    return false;
}
