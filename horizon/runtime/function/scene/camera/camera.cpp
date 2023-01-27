/*****************************************************************//**
 * \file   Camera.cpp
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#include "Camera.h"

// standard libraries

// third party libraries

// project headers

namespace Horizon {

using namespace Input;

Camera::Camera(const CameraSetting& setting, const math::Vector3f &eye, const math::Vector3f &at, const math::Vector3f &up) noexcept
    : m_eye(eye), m_at(at), m_up(up),m_setting(setting) {
    m_forward = math::Normalize(m_at - m_eye);
    m_right = math::Cross(m_forward, m_up);
    UpdateViewMatrix();
}

void Camera::SetPerspectiveProjectionMatrix(f32 fov, f32 aspect_ratio, f32 near_plane, f32 far_plane) noexcept {
    m_fov = fov;
    m_aspect_ratio = aspect_ratio;
    m_near_plane = near_plane;
    m_far_plane = far_plane;
    m_projection = math::CreatePerspectiveProjectionMatrix2(fov, aspect_ratio, near_plane, far_plane);
    // m_projection = ReversePerspective(fov, aspect_ratio, nearPlane,
    // farPlane);
}

void Camera::SetLensProjectionMatrix(f32 focal_length, f32 aspect_ratio, f32 near_plane, f32 far_plane) noexcept {

    m_aspect_ratio = aspect_ratio;
    m_near_plane = near_plane;
    m_far_plane = far_plane;

    f32 h = (0.5f * near_plane) * ((SENSOR_SIZE * 1000.0f) / focal_length);
    f32 w = h * aspect_ratio;
    m_projection =  math::CreatePerspectiveProjectionMatrix1(w, h, near_plane, far_plane);
}

math::Matrix44f Camera::GetProjectionMatrix() const noexcept {
    auto p = m_projection;
    return p;
}

math::Matrix44f Camera::GetInvProjectionMatrix() const noexcept {
    return math::Invert(m_projection);
}

math::Vector3f Camera::GetFov() const noexcept {
     return math::Vector3f(); 
}

math::Vector2f Camera::GetNearFarPlane() const noexcept {
     return math::Vector2f(m_near_plane, m_far_plane); 
}

void Camera::SetCameraSpeed(f32 speed) noexcept {
     m_camera_speed = speed; 
}

f32 Camera::GetCameraSpeed() const noexcept {
     return m_camera_speed; 
}

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
    m_yaw += xoffset * m_sensitivity.x();
    m_pitch -= yoffset * m_sensitivity.y(); // TODO(hylu): unify axis in different API

    // prevent locked
    if (m_pitch > 89.0f)
        m_pitch = 89.0f;
    if (m_pitch < -89.0f)
        m_pitch = -89.0f;
}

void Camera::UpdateViewMatrix() noexcept {
    // calculate the new Front vector
    math::Vector3f front;
    front.x() = math::Cos(math::Radians(m_yaw)) * math::Cos(math::Radians(m_pitch));
    front.y() = math::Sin(math::Radians(m_pitch));
    front.z() = math::Sin(math::Radians(m_yaw)) * math::Cos(math::Radians(m_pitch));

    m_forward = math::Normalize(front);
    m_right = math::Normalize(math::Cross(m_forward, math::Vector3f(0.0, 1.0,
                                                                  0.0))); // normalize the vectors, because their length
                                                                          // gets closer to 0 the more you look up or
                                                                          // down which results in slower Movement.
    m_up = math::Normalize(math::Cross(m_right, m_forward));

    m_view = math::CreateLookAt(m_eye, m_eye + m_forward, m_up);
}

math::Matrix44f Camera::GetViewProjectionMatrix() const noexcept {
    return m_view * m_projection;
}

math::Matrix44f Camera::GetInvViewProjectionMatrix() const noexcept {
    return math::Invert(m_view * m_projection);
}

math::Vector3f Camera::GetForwardDir() const noexcept {
    return m_forward; 
}

f32 Camera::GetExposure() const noexcept {
    return exposure; 
}

void Camera::SetExposure(f32 _aperture, f32 _shutter_speed, f32 _iso) {
    this->aperture = _aperture;
    this->shutter_speed = _shutter_speed;
    this->iso = _iso;

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
    exposure = 1.0f/ (pow(2.0f, ev100) * 1.2f);
}

const math::Vector2f Camera::GetSensitivity() const noexcept {
    return m_sensitivity; 
}

math::Matrix44f Camera::GetViewMatrix() const noexcept {
    return m_view;
}

math::Matrix44f Camera::GetInvViewMatrix() const noexcept {
    return math::Invert(m_view);
}

math::Vector3f Camera::GetPosition() const noexcept {
    return m_eye; 
}

} // namespace Horizon
