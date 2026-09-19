#include "scope_logic.h"
#include <algorithm>
#include <cmath>

namespace
{
    void RotateVector(const float q[4], const float v[3], float out[3])
    {
        // q * v * conjugate(q), expanded for a unit quaternion.
        const float tx = 2.0f * (q[1] * v[2] - q[2] * v[1]);
        const float ty = 2.0f * (q[2] * v[0] - q[0] * v[2]);
        const float tz = 2.0f * (q[0] * v[1] - q[1] * v[0]);
        out[0] = v[0] + q[3] * tx + (q[1] * tz - q[2] * ty);
        out[1] = v[1] + q[3] * ty + (q[2] * tx - q[0] * tz);
        out[2] = v[2] + q[3] * tz + (q[0] * ty - q[1] * tx);
    }
}

ScopeToggleUpdate ScopeToggleDetector::Update(bool enabled, bool rightClick,
                                               bool cancelGesture)
{
    if (!enabled)
    {
        const bool changed = m_active;
        m_active = false;
        m_rightDown = rightClick;
        m_cancelled = rightClick;
        return {changed, false};
    }

    if (rightClick && !m_rightDown)
    {
        m_rightDown = true;
        m_cancelled = cancelGesture;
    }
    else if (rightClick && cancelGesture)
    {
        m_cancelled = true;
    }

    if (!rightClick && m_rightDown)
    {
        const bool toggle = !m_cancelled && !cancelGesture;
        m_rightDown = false;
        m_cancelled = false;
        if (toggle)
        {
            m_active = !m_active;
            return {true, m_active};
        }
    }
    return {false, m_active};
}

void ScopeToggleDetector::Reset()
{
    m_active = false;
    m_rightDown = false;
    m_cancelled = false;
}

bool ScopeRefreshScheduler::Advance(bool active, int divisor)
{
    if (!active)
    {
        m_frame = 0;
        return false;
    }
    if (divisor < 1) divisor = 1;
    if (divisor > 4) divisor = 4;
    return (++m_frame % static_cast<unsigned>(divisor)) == 0;
}

float ScopeZoomController::Update(bool active, float stickY,
                                  float deltaSeconds, float defaultZoom)
{
    constexpr float kMinZoom = 6.0f;
    constexpr float kMaxZoom = 24.0f;
    constexpr float kDeadzone = 0.20f;
    constexpr float kZoomUnitsPerSecond = 10.0f;

    if (!std::isfinite(defaultZoom)) defaultZoom = kMinZoom;
    defaultZoom = std::clamp(defaultZoom, kMinZoom, kMaxZoom);
    if (!active)
    {
        m_wasActive = false;
        return m_zoom;
    }
    if (!m_wasActive)
    {
        m_zoom = defaultZoom;
        m_wasActive = true;
        return m_zoom;
    }

    if (!std::isfinite(stickY)) stickY = 0.0f;
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f)
        deltaSeconds = 0.0f;
    deltaSeconds = std::min(deltaSeconds, 0.10f);
    const float magnitude = std::fabs(stickY);
    if (magnitude > kDeadzone)
    {
        const float normalized = std::min(
            (magnitude - kDeadzone) / (1.0f - kDeadzone), 1.0f);
        const float direction = stickY > 0.0f ? 1.0f : -1.0f;
        m_zoom = std::clamp(m_zoom + direction * normalized *
                            kZoomUnitsPerSecond * deltaSeconds,
                            kMinZoom, kMaxZoom);
    }
    return m_zoom;
}

void ScopeZoomResolver::RequestToggle()
{
    if (m_ignoreRequestFrames)
    {
        m_ignoreRequestFrames = 0;
        return;
    }
    m_pendingFrames = 2;
}

bool ScopeZoomResolver::Update(bool enabled, bool nativeZoomed)
{
    if (!enabled)
    {
        Reset();
        return false;
    }
    if (nativeZoomed)
    {
        m_nativeEngaged = true;
        m_fallbackActive = false;
        m_pendingFrames = 0;
        m_ignoreRequestFrames = 0;
        return true;
    }
    if (m_nativeEngaged)
    {
        const bool matchingRequestAlreadyArrived = m_pendingFrames != 0;
        m_nativeEngaged = false;
        m_fallbackActive = false;
        m_pendingFrames = 0;
        // Halo changes zoom on the R3 press; its release request reaches this
        // resolver later. Swallow that matching release even for a long click.
        m_ignoreRequestFrames = matchingRequestAlreadyArrived ? 0 : 120;
        return false;
    }
    if (m_ignoreRequestFrames) --m_ignoreRequestFrames;
    if (m_pendingFrames && --m_pendingFrames == 0)
        m_fallbackActive = !m_fallbackActive;
    return m_fallbackActive;
}

void ScopeZoomResolver::Reset()
{
    m_fallbackActive = false;
    m_nativeEngaged = false;
    m_pendingFrames = 0;
    m_ignoreRequestFrames = 0;
}

ScopeQuadTransform ComputeScopeQuadTransform(const float orientation[4],
                                             const float origin[3],
                                             float rightMeters,
                                             float upMeters,
                                             float forwardMeters,
                                             float widthMeters)
{
    const float localOffset[3] = {rightMeters, upMeters, -forwardMeters};
    float worldOffset[3]{};
    RotateVector(orientation, localOffset, worldOffset);

    ScopeQuadTransform result{};
    result.position[0] = origin[0] + worldOffset[0];
    result.position[1] = origin[1] + worldOffset[1];
    result.position[2] = origin[2] + worldOffset[2];
    result.width = widthMeters;
    result.height = widthMeters;
    return result;
}

bool ComputeScopeCameraPose(const float controllerBasis[9],
                            const float cameraOrigin[3],
                            const float bulletForward[3],
                            ScopeCameraPose& result)
{
    if (!controllerBasis || !cameraOrigin || !bulletForward)
        return false;

    const float* controllerUp = controllerBasis + 6;
    for (int i = 0; i < 3; ++i)
    {
        if (!std::isfinite(cameraOrigin[i])) return false;
        result.position[i] = cameraOrigin[i];
        result.forward[i] = bulletForward[i];
    }

    float length = std::sqrt(result.forward[0] * result.forward[0] +
                             result.forward[1] * result.forward[1] +
                             result.forward[2] * result.forward[2]);
    if (!std::isfinite(length) || length < 1e-4f)
        return false;
    for (float& component : result.forward) component /= length;

    // Preserve the rifle's roll while keeping up perpendicular to the actual
    // bullet direction used by the remote camera.
    const float along = controllerUp[0] * result.forward[0] +
                        controllerUp[1] * result.forward[1] +
                        controllerUp[2] * result.forward[2];
    for (int i = 0; i < 3; ++i)
        result.up[i] = controllerUp[i] - result.forward[i] * along;
    length = std::sqrt(result.up[0] * result.up[0] +
                       result.up[1] * result.up[1] +
                       result.up[2] * result.up[2]);
    if (!std::isfinite(length) || length < 1e-4f)
        return false;
    for (float& component : result.up) component /= length;
    return true;
}

ScopeProjectionTangents ComputeScopeProjectionTangents(float zoom,
                                                        float sourceAspect)
{
    if (!std::isfinite(zoom) || zoom < 1.0f) zoom = 1.0f;
    if (!std::isfinite(sourceAspect) || sourceAspect < 0.5f || sourceAspect > 4.0f)
        sourceAspect = 4.0f / 3.0f;
    constexpr float kBaseHorizontalTangent = 0.70020754f; // tan(70 degrees / 2)
    const float finalHorizontal = kBaseHorizontalTangent / zoom;
    const float finalVertical = finalHorizontal / std::min(sourceAspect,1.f);
    return {finalVertical * sourceAspect, finalVertical};
}

bool ExpandScopeCullTangents(const float headForward[3], const float headUp[3],
                            const ScopeCameraPose& scope,
                            const ScopeProjectionTangents& lens,
                            ScopeProjectionTangents& head)
{
    if (!headForward || !headUp || !std::isfinite(head.horizontal) ||
        !std::isfinite(head.vertical) || head.horizontal <= 0 || head.vertical <= 0 ||
        !std::isfinite(lens.horizontal) || !std::isfinite(lens.vertical) ||
        lens.horizontal <= 0 || lens.vertical <= 0) return false;
    auto orthonormal = [](const float* f, const float* u) {
        float ff=0, uu=0, fu=0;
        for(int i=0;i<3;++i){ff+=f[i]*f[i];uu+=u[i]*u[i];fu+=f[i]*u[i];}
        return std::isfinite(ff)&&std::isfinite(uu)&&std::isfinite(fu)&&
            std::fabs(ff-1)<.002f&&std::fabs(uu-1)<.002f&&std::fabs(fu)<.002f;
    };
    if(!orthonormal(headForward,headUp)||!orthonormal(scope.forward,scope.up))return false;
    const float hr[]{headForward[1]*headUp[2]-headForward[2]*headUp[1],
        headForward[2]*headUp[0]-headForward[0]*headUp[2],
        headForward[0]*headUp[1]-headForward[1]*headUp[0]};
    const float sr[]{scope.forward[1]*scope.up[2]-scope.forward[2]*scope.up[1],
        scope.forward[2]*scope.up[0]-scope.forward[0]*scope.up[2],
        scope.forward[0]*scope.up[1]-scope.forward[1]*scope.up[0]};
    auto expanded=head;
    for(int x=-1;x<=1;x+=2)for(int y=-1;y<=1;y+=2)
    {
        float depth=0,right=0,up=0;
        for(int i=0;i<3;++i)
        {
            const float ray=scope.forward[i]+x*lens.horizontal*sr[i]+y*lens.vertical*scope.up[i];
            depth+=ray*headForward[i];right+=ray*hr[i];up+=ray*headUp[i];
        }
        if(!std::isfinite(depth)||!std::isfinite(right)||!std::isfinite(up)||depth<=.001f)return false;
        expanded.horizontal=std::fmax(expanded.horizontal,std::fabs(right/depth));
        expanded.vertical=std::fmax(expanded.vertical,std::fabs(up/depth));
    }
    if(!std::isfinite(expanded.horizontal)||!std::isfinite(expanded.vertical))return false;
    head=expanded;return true;
}
