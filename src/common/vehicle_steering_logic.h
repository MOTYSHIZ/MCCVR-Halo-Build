#pragma once
// HOW THE DRIVER OF A LOOK-STEERED VEHICLE STEERS (vehicle_steering): its own choice, apart from how the view
// turns.
//
// A look-steered vehicle (Warthog, Mongoose, Ghost, Prowler, Chopper, Banshee, Hornet) turns toward the driver's
// aim. Three ways to give it one:
//   Motion  the hand aims, and the vehicle turns toward where it points;
//   Stick   the turn stick steers, as on a flat screen;
//   Wheel   two hands on an invisible wheel steer (ground vehicles; an aircraft takes the stick instead).
// Whether the VIEW turns with the vehicle is a separate matter -- the view-follow setting, or the tethering mode
// of a camera from the vehicle camera file -- and any of the three may be had with either.
//
// Automatic keeps the two tied as they were before the choice existed: the hand steers a view the world holds
// still, and the stick or wheel steers a view that turns with the vehicle.
//
// With the hand steering a view that turns with the vehicle, the hand's angle off straight ahead is a turn RATE,
// not a heading: the vehicle keeps turning for as long as the hand points aside, and straightens when the hand
// does.
//
// Pure: no engine.

#include "halo3_vehicle_logic.h"

namespace vehicle_steering
{
enum class Mode : int { Automatic = 0, Motion = 1, Stick = 2, Wheel = 3 };
inline constexpr int kModeCount = 4;

inline constexpr Mode FromConfig(int value) noexcept
{
    return value >= 0 && value < kModeCount ? static_cast<Mode>(value) : Mode::Automatic;
}

// Does the stick or wheel steer this seat, in place of the hand's aim? `lookSteeredDriver`: the driver's seat
// of a look-steered vehicle. `viewFollows`: the view turns with the vehicle in this seat right now.
inline constexpr bool Authored(Mode mode, bool lookSteeredDriver, bool viewFollows) noexcept
{
    if (!lookSteeredDriver)
        return false;
    switch (mode)
    {
    case Mode::Motion:
        return false;
    case Mode::Stick:
    case Mode::Wheel:
        return true;
    default:
        return viewFollows;
    }
}

// WHEEL AS THE CHOSEN WAY TO STEER. The wheel is put in the player's hands when a seat first offers one, once
// per offer: the double-click that lets go of it (Halo3UpdateWheelToggle) is not undone a moment later, and
// takes it again as before. `available`: the seat can use a wheel this poll. Call after the toggle. True when
// this call took the wheel.
struct WheelOffer
{
    bool made = false;
};
inline bool TakeWheelOnOffer(Halo3Wheel& wheel, WheelOffer& offer, bool available, bool wheelChosen) noexcept
{
    if (!available || !wheelChosen)
    {
        offer.made = false;
        return false;
    }
    if (offer.made)
        return false;
    offer.made = true;
    if (wheel.engaged)
        return false;
    wheel.engaged = true;
    wheel.clicks = 0;
    return true;
}
} // namespace vehicle_steering
