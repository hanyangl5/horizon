#include "Camera.h"

namespace Horizon {
Camera::Camera(Math::float3 eye, Math::float3 at, Math::float3 up) noexcept : m_eye(eye), m_at(at), m_up(up) {
    m_forward = Math::Normalize(m_at - m_eye);
    m_right = Math::Cross(m_forward, m_up);
    UpdateViewMatrix();
    //setLookAt(eye, at, up);
}

void Camera::SetPerspectiveProjectionMatrix(f32 fov, f32 aspect_ratio, f32 nearPlane, f32 farPlane) noexcept {
    m_fov = fov;
    m_aspect_ratio = aspect_ratio;
    m_near_plane = nearPlane;
    m_far_plane = farPlane;
    m_projection = Math::Perspective(fov, aspect_ratio, nearPlane, farPlane);
    // m_projection = ReversePerspective(fov, aspect_ratio, nearPlane,
    // farPlane);
}

Math::float4x4 Camera::GetProjectionMatrix() const noexcept {
    auto p = m_projection;
    p = p.Transpose();
    return p;
}

Math::float4x4 Camera::GetInvProjectionMatrix() const noexcept {
    auto inv_p = m_projection.Invert();
    inv_p = inv_p.Transpose();
    return inv_p;
}

Math::float3 Camera::GetFov() const noexcept { return Math::float3(); }

Math::float2 Camera::GetNearFarPlane() const noexcept { return Math::float2(m_near_plane, m_far_plane); }

void Camera::SetCameraSpeed(f32 speed) noexcept { m_camera_speed = speed; }

f32 Camera::GetCameraSpeed() const noexcept { return m_camera_speed; }

void Camera::Move(Direction direction) noexcept {
    switch (direction) {
    case Horizon::Direction::FORWARD:
        m_eye += GetCameraSpeed() * m_forward;
        break;
    case Horizon::Direction::BACKWARD:
        m_eye -= GetCameraSpeed() * m_forward;
        break;
    case Horizon::Direction::RIGHT:
        m_eye += GetCameraSpeed() * m_right;
        break;
    case Horizon::Direction::LEFT:
        m_eye -= GetCameraSpeed() * m_right;
        break;
    case Horizon::Direction::UP:
        m_eye += GetCameraSpeed() * m_up;
        break;
    case Horizon::Direction::DOWN:
        m_eye -= GetCameraSpeed() * m_up;
        break;
    default:
        break;
    }
}
void Camera::Rotate(f32 xoffset, f32 yoffset) noexcept {
    m_yaw += xoffset;
    m_pitch -= yoffset; // TODO: unify axis in different API

    // prevent locked
    if (m_pitch > 89.0f)
        m_pitch = 89.0f;
    if (m_pitch < -89.0f)
        m_pitch = -89.0f;
}
void Camera::UpdateViewMatrix() noexcept {
    // calculate the new Front std::vector
    Math::float3 front;
    front.x = cos(Math::Radians(m_yaw)) * cos(Math::Radians(m_pitch));
    front.y = sin(Math::Radians(m_pitch));
    front.z = sin(Math::Radians(m_yaw)) * cos(Math::Radians(m_pitch));

    m_forward = Math::Normalize(front);
    m_right = Math::Normalize(Math::Cross(m_forward, Math::float3(0.0, 1.0,
                                                                  0.0))); // normalize the vectors, because their length
                                                                          // gets closer to 0 the more you look up or
                                                                          // down which results in slower Movement.
    m_up = Math::Normalize(Math::Cross(m_right, m_forward));

    m_view = Math::LookAt(m_eye, m_eye + m_forward, m_up);
}

Math::float4x4 Camera::GetViewProjectionMatrix() const noexcept {
    auto vp = m_view * m_projection;
    vp = vp.Transpose();
    return vp;
}

Math::float4x4 Camera::GetInvViewProjectionMatrix() const noexcept {
    auto vp = m_view * m_projection;
    vp = vp.Invert();
    vp = vp.Transpose();
    return vp;
}
Math::float3 Camera::GetForwardDir() const noexcept { return m_forward; }

f32 Camera::GetExposure() const noexcept { return exposure; }

void Camera::SetExposure(f32 aperture, f32 shutter_speed, f32 iso) {
    aperture = aperture;
    shutter_speed = shutter_speed;
    iso = iso;

    // With N = aperture, t = shutter speed and S = sensitivity,
    // we can compute EV100 knowing that:
    //
    // EVs = log2(N^2 / t)
    // and
    // EVs = EV100 + log2(S / 100)
    //
    // We can therefore find:
    //
    // EV100 = EVs - log2(S / 100)
    // EV100 = log2(N^2 / t) - log2(S / 100)
    // EV100 = log2((N^2 / t) * (100 / S))
    //
    // Reference: https://en.wikipedia.org/wiki/Exposure_value
    ev100 = std::log2((aperture * aperture) / shutter_speed * 100.0f / iso);
    exposure = 1.0 / (pow(2.0, ev100) * 1.2);
}

Math::float4x4 Camera::GetViewMatrix() const noexcept {
    auto v = m_view;
    v = v.Transpose();
    return v;
}

Math::float3 Camera::GetPosition() const noexcept { return m_eye; }
} // namespace Horizon
