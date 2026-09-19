#pragma once

// Effective mode only: never overwrite the user's saved on-foot preference.
inline bool UseSmoothVrTurn(bool savedSmooth,bool vehicleSmooth,bool seated) noexcept
{
    return savedSmooth||(vehicleSmooth&&seated);
}
