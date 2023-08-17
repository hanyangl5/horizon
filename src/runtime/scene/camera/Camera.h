/*****************************************************************/ /**
 * \file   Camera.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#include <runtime/core/math/Math.h>
#include <runtime/core/window/WindowInput.h>

namespace Horizon {

enum class ProjectionMode { PERSPECTIVE, ORTHOGRAPHIC };

enum class CameraType { FLY };

struct CameraSetting {
    ProjectionMode project_mode{};
    CameraType camera_type{};
    bool moveable = true;
};

class Camera {
  public:
    Camera(const CameraSetting &setting, const Math::float3 &position, const Math::float3 &at,
           const Math::float3 &up) noexcept;

    ~Camera() noexcept = default;

    Camera(const Camera &rhs) noexcept = delete;

    Camera &operator=(const Camera &rhs) noexcept = delete;

    Camera(Camera &&rhs) noexcept = delete;

    Camera &operator=(Camera &&rhs) noexcept = delete;

  public:
    void SetPerspectiveProjectionMatrix(f32 fov, f32 aspect_ratio, f32 near, f32 far) noexcept;

    // milimeters
    void SetLensProjectionMatrix(f32 focal_length, f32 aspect_ratio, f32 near, f32 far) noexcept;

    void SetOrthoProjectionMatrix(f32 w, f32 h, f32 near_plane, f32 far_plane) noexcept;

    Math::float4x4 GetProjectionMatrix() const noexcept;

    Math::float4x4 GetInvProjectionMatrix() const noexcept;

    //void setLookAt(Math::float3 position, Math::float3 at, Math::float3 up = Math::float3(0.0f, 1.0f, 0.0f));

    Math::float4x4 GetViewMatrix() const noexcept;

    Math::float4x4 GetInvViewMatrix() const noexcept;

    Math::float3 GetPosition() const noexcept;

    f32 GetFov() const noexcept;

    Math::float2 GetNearFarPlane() const noexcept;

    void SetCameraSpeed(f32 speed) noexcept;
    f32 GetCameraSpeed() const noexcept;

    void Move(Input::Direction direction) noexcept;
    void Rotate(f32 yaw, f32 pitch) noexcept;

    void UpdateViewMatrix() noexcept;

    Math::float4x4 GetViewProjectionMatrix() const noexcept;

    Math::float4x4 GetInvViewProjectionMatrix() const noexcept;

    Math::float3 GetForwardDir() const noexcept;

    f32 GetEv100() const noexcept;
    f32 GetExposure() const noexcept;

    void SetExposure(f32 aperture, f32 shutter_speed, f32 iso);

    const Math::float2 GetSensitivity() const noexcept;

  private:
    CameraSetting m_settings;
    Math::float3 m_eye, m_at, m_up;
    Math::float3 m_forward, m_right;
    Math::float4x4 m_view, m_projection;

    f32 m_yaw = 0.0f, m_pitch = 0.0f;

    f32 m_fov;
    f32 m_near_plane, m_far_plane;
    f32 m_camera_speed{};

    //exposure settings
    f32 m_aperture;
    f32 m_shutter_speed;
    f32 m_iso;
    f32 m_ev100;
    f32 m_exposure;

    // a 35mm camera has a 36x24mm wide frame size
    static constexpr const float SENSOR_SIZE = 0.024f; // 24mm

    // controller setting
    Math::float2 m_sensitivity = Math::float2(1.0f);
};

} // namespace Horizon
