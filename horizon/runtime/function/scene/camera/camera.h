/*****************************************************************//**
 * \file   Camera.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

// standard libraries

// third party libraries

// project headers

#include <runtime/core/math/math.h>
#include <runtime/core/window/window_input.h>

namespace Horizon {

enum class ProjectionMode {
    PERSPECTIVE,
    ORTHOGRAPHIC
};

enum class CameraType {
    FLY,
    ORBIT,
    CINEMETIC
};

struct CameraSetting {
    ProjectionMode project_mode{};
    CameraType camera_type{};
    bool moveable = true;
};

class Camera {
  public:
    Camera(const CameraSetting& setting, const math::Vector3f &position, const math::Vector3f &at,
           const math::Vector3f &up) noexcept;

    ~Camera() noexcept = default;

    Camera(const Camera &rhs) noexcept = delete;

    Camera &operator=(const Camera &rhs) noexcept = delete;

    Camera(Camera &&rhs) noexcept = delete;

    Camera &operator=(Camera &&rhs) noexcept = delete;

  public:
    void SetPerspectiveProjectionMatrix(f32 fov, f32 aspect_ratio, f32 near, f32 far) noexcept;

    // milimeters
    void SetLensProjectionMatrix(f32 focal_length, f32 aspect_ratio, f32 near, f32 far) noexcept;

    math::Matrix44f GetProjectionMatrix() const noexcept;

    math::Matrix44f GetInvProjectionMatrix() const noexcept;

    //void setLookAt(math::Vector3f position, math::Vector3f at, math::Vector3f up = math::Vector3f(0.0f, 1.0f, 0.0f));

    math::Matrix44f GetViewMatrix() const noexcept;

    math::Matrix44f GetInvViewMatrix() const noexcept;

    math::Vector3f GetPosition() const noexcept;

    math::Vector3f GetFov() const noexcept;

    math::Vector2f GetNearFarPlane() const noexcept;

    void SetCameraSpeed(f32 speed) noexcept;
    f32 GetCameraSpeed() const noexcept;

    void Move(Input::Direction direction) noexcept;
    void Rotate(f32 yaw, f32 pitch) noexcept;

    void UpdateViewMatrix() noexcept;

    math::Matrix44f GetViewProjectionMatrix() const noexcept;

    math::Matrix44f GetInvViewProjectionMatrix() const noexcept;

    math::Vector3f GetForwardDir() const noexcept;

    f32 GetEv100() const noexcept { return ev100; }
    f32 GetExposure() const noexcept;

    void SetExposure(f32 aperture, f32 shutter_speed, f32 iso);

    const math::Vector2f GetSensitivity() const noexcept;
  private:
    math::Vector3f m_eye, m_at, m_up;
    math::Vector3f m_forward, m_right;
    math::Matrix44f m_view, m_projection;

    f32 m_yaw = -90.0f, m_pitch = 0.0f;

    f32 m_fov, m_aspect_ratio, m_near_plane, m_far_plane;
    f32 m_camera_speed{};

    //exposure settings
    f32 aperture;
    f32 shutter_speed;
    f32 iso;
    f32 ev100;
    f32 exposure;

    // a 35mm camera has a 36x24mm wide frame size
    static constexpr const float SENSOR_SIZE = 0.024f; // 24mm
    
    // controller setting
    math::Vector2f m_sensitivity = math::Vector2f(1.0f, 1.0);
};



} // namespace Horizon
