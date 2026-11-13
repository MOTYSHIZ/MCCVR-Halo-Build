#pragma once
// Runtime for the support-hand tracks (src/common/hand_track_logic.h).
//
// Threads:
//   worker (50 ms title poll)  HandTracks_Poll: reads/writes the tracks file, logs.
//   VR input thread            HandTracks_Update: starts and advances the playing track.
//   palette hook (per eye)     HandTracks_Current: lock-free, allocation-free sample.
//   any thread                 HandTracks_Request, HandTracks_GetStatus.
#include <cstdint>
#include "../common/hand_gesture_logic.h"
#include "../common/hand_track_logic.h"
#include "../common/runtime_types.h"

enum class HandTrackRequest : uint32_t { None = 0, Reload = 1, Preview = 2 };

struct HandTracksStatus
{
    bool loaded{};        // a library (file or built-in) is in use
    bool fromFile{};      // false: built-in defaults
    unsigned tracks{}, dropped{}, errors{}, firstErrorLine{};
    char firstError[96]{};
    bool playing{};
    char playingName[hand_track::kNameLength]{};
    char playingWeapon[hand_track::kNameLength]{};
    unsigned plays{};
    bool gestureTouch[2]{};   // that hand reports trigger and thumb touch
    hand_track::Shape gesture[2]{};
};

void HandTracks_SetFolder(const wchar_t* folder);
void HandTracks_Poll(uint64_t nowMs);
void HandTracks_Request(HandTrackRequest request);
void HandTracks_Update(uint64_t nowMs, GameTitle title, uint64_t weaponIdentity, bool dualWield);
bool HandTracks_Current(hand_track::Sampled& out);
// Palette hook: the finger curls (index, middle, ring, pinky, thumb) the playing
// track's hand shapes ask for, blended from `controller`. False: keep the
// ordinary finger input.
bool HandTracks_CurrentCurls(const float controller[hand_track::kFingers], float out[hand_track::kFingers]);
// VR input thread, every input frame: picks and eases each free hand's finger
// gesture (hand_gesture_logic.h) from inputs[0] the support hand and inputs[1]
// the gun hand. timeNs is the frame's predicted display time.
void HandTracks_UpdateGestures(int64_t timeNs, GameTitle title, const hand_gesture::Input inputs[2]);
// Palette hook: side 0 support hand, 1 gun hand. False: no gesture (finger
// gestures off, no thumb sensors, another game): keep the plain trigger/grip curl.
bool HandTracks_GestureCurls(unsigned side, float out[hand_track::kFingers]);
// Palette hook: a gesture shaped that hand this solve (a counter, for the log).
void HandTracks_NoteGestureApplied(unsigned side);
// Palette hook: the track moved the support hand this solve (a counter, for the log).
void HandTracks_NoteApplied();
void HandTracks_GetStatus(HandTracksStatus& out);
