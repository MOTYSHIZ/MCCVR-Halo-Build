#pragma once
#include <windows.h>

void NativeMenuPointer_Init(HWND window);
// Render thread publishes only a bounded atomic sample and a coalesced message.
void NativeMenuPointer_Publish(bool active, float u = 0, float v = 0, bool pressed = false);
bool NativeMenuPointer_ConsumesTrigger();
void NativeMenuPointer_ReportFailures(); // existing diagnostics worker only
// Called before ImGui/native processing, on the window thread.
bool NativeMenuPointer_Message(UINT message);
