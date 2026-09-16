#include "input_logic.h"

#include <cmath>
#include <algorithm>

bool DpadHeadWithinRadius(const float head[3], const float controller[3],
    float radiusMetres) noexcept
{
    if (!head || !controller) return false;
    const float radius = std::isfinite(radiusMetres)
        ? std::clamp(radiusMetres, 0.10f, 0.50f) : 0.30f;
    const float dx = head[0] - controller[0], dy = head[1] - controller[1],
        dz = head[2] - controller[2];
    return dx*dx + dy*dy + dz*dz < radius*radius;
}

DpadStickInput ConsumeThumbrestDpad(bool enabled, bool touched, bool inputReady,
    float& rightX, float& rightY) noexcept
{
    if (!enabled || !touched || !inputReady) return {};
    DpadStickInput result{true,
        std::isfinite(rightX) ? std::clamp(rightX, -1.0f, 1.0f) : 0.0f,
        std::isfinite(rightY) ? std::clamp(rightY, -1.0f, 1.0f) : 0.0f};
    rightX = rightY = 0.0f;
    return result;
}

uint16_t DpadDirectionButtons(float x, float y) noexcept
{
    // XInput's UP/DOWN/LEFT/RIGHT bits, with the existing gesture threshold.
    if (!std::isfinite(x)) x = 0.0f;
    if (!std::isfinite(y)) y = 0.0f;
    uint16_t result = 0;
    if (y > 0.5f) result |= 0x0001;
    if (y < -0.5f) result |= 0x0002;
    if (x < -0.5f) result |= 0x0004;
    if (x > 0.5f) result |= 0x0008;
    return result;
}

void MenuChordDetector::Reset()
{
    m_firstDownMs = 0;
    m_firstSide = 0;
    m_latched = false;
    m_expired = false;
}

MenuChordResult MenuChordDetector::Update(uint64_t nowMs, bool leftClick, bool rightClick)
{
    if (m_latched)
    {
        if (!leftClick && !rightClick)
            Reset();
        else
            return { false, true };
        return {};
    }

    if (!leftClick && !rightClick)
    {
        Reset();
        return {};
    }

    if (m_firstSide == 0)
    {
        m_firstDownMs = nowMs;
        m_firstSide = leftClick && rightClick ? 3 : (leftClick ? 1 : 2);
    }

    if (!m_expired && nowMs - m_firstDownMs > 250)
        m_expired = true;
    if (!m_expired && leftClick && rightClick)
    {
        m_latched = true;
        return { true, true };
    }
    return {};
}

MenuPointerHit IntersectMenuQuad(const float origin[3], const float direction[3],
    float distance, float width, float height, float centerY, float centerX)
{
    MenuPointerHit result{};
    if (!origin || !direction || distance <= 0.0f || width <= 0.0f || height <= 0.0f)
        return result;
    if (std::fabs(direction[2]) < 1e-5f)
        return result;

    const float t = (-distance - origin[2]) / direction[2];
    if (t <= 0.0f)
        return result;
    const float x = origin[0] + direction[0] * t;
    const float y = origin[1] + direction[1] * t;
    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;
    if (x < centerX - halfWidth || x > centerX + halfWidth ||
        y < centerY - halfHeight || y > centerY + halfHeight)
        return result;

    result.hit = true;
    result.u = (x - centerX) / width + 0.5f;
    result.v = 0.5f - (y - centerY) / height;
    return result;
}

float BlendXInputMotors(uint16_t lowFrequencyMotor, uint16_t highFrequencyMotor)
{
    constexpr float inverseMax = 1.0f / 65535.0f;
    const float low = static_cast<float>(lowFrequencyMotor) * inverseMax;
    const float high = static_cast<float>(highFrequencyMotor) * inverseMax;
    const float blended = low * 0.65f + high * 0.35f;
    return blended > 1.0f ? 1.0f : blended;
}

HapticPeakSample SampleHapticPeak(float peak, float latest)
{
    const float p = peak < 0.0f ? 0.0f : (peak > 1.0f ? 1.0f : peak);
    const float l = latest < 0.0f ? 0.0f : (latest > 1.0f ? 1.0f : latest);
    HapticPeakSample sample;
    sample.apply = p > l ? p : l;
    sample.carry = l;
    return sample;
}

float MergeHapticAmplitude(float game, float contact)
{
    const float g = game < 0.0f ? 0.0f : (game > 1.0f ? 1.0f : game);
    const float c = contact < 0.0f ? 0.0f :
        (contact > 1.0f ? 1.0f : contact);
    return g > c ? g : c;
}

uint32_t NormalizeVirtualXInputSetStateResult(
    uint32_t originalResult, uint32_t userIndex, bool hasVibrationRequest)
{
    return userIndex == 0 && hasVibrationRequest ? 0u : originalResult;
}

bool PausePresentationInputAllowed(bool sharedGameplayOwner)
{
    return sharedGameplayOwner;
}

bool PauseToggleInputAllowed(
    bool sharedGameplayOwner, bool titleSpecificPauseOwner)
{
    return sharedGameplayOwner || titleSpecificPauseOwner;
}

bool UpdateTwoHandHold(bool wasEngaged, bool gripHeld, bool inGrabZone)
{
    return gripHeld && (wasEngaged || inGrabZone);
}

void PauseLevelRecovery::Reset()
{
    m_sawLoading = false;
}

bool PauseLevelRecovery::Update(bool pausePresentation, bool cameraStale,
                                bool levelStable)
{
    if (!pausePresentation)
    {
        Reset();
        return false;
    }
    if (cameraStale)
        m_sawLoading = true;
    if (m_sawLoading && levelStable)
    {
        Reset();
        return true;
    }
    return false;
}
