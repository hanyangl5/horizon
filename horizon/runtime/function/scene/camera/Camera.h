#pragma once

#include <runtime/core/math/Math.h>

namespace Horizon {

enum class Direction { FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };

class Camera {
  public:
    Camera(Math::float3 position, Math::float3 at, Math::float3 up) noexcept;

    ~Camera() noexcept = default;

    Camera(const Camera &rhs) noexcept = delete;

    Camera &operator=(const Camera &rhs) noexcept = delete;

    Camera(Camera &&rhs) noexcept = delete;

    Camera &operator=(Camera &&rhs) noexcept = delete;

  public:
    void SetPerspectiveProjectionMatrix(f32 fov, f32 aspect_ratio, f32 near, f32 far) noexcept;

    Math::float4x4 GetProjectionMatrix() const noexcept;

    Math::float4x4 GetInvProjectionMatrix() const noexcept;

    //void setLookAt(Math::float3 position, Math::float3 at, Math::float3 up = Math::float3(0.0f, 1.0f, 0.0f));

    Math::float4x4 GetViewMatrix() const noexcept;

    Math::float3 GetPosition() const noexcept;

    Math::float3 GetFov() const noexcept;

    Math::float2 GetNearFarPlane() const noexcept;

    void SetCameraSpeed(f32 speed) noexcept;
    f32 GetCameraSpeed() const noexcept;

    void Move(Direction direction) noexcept;
    void Rotate(f32 yaw, f32 pitch) noexcept;

    void UpdateViewMatrix() noexcept;

    Math::float4x4 GetViewProjectionMatrix() const noexcept;

    Math::float4x4 GetInvViewProjectionMatrix() const noexcept;

    Math::float3 GetForwardDir() const noexcept;

    f32 GetEv100() const noexcept { return ev100; }
    f32 GetExposure() const noexcept;

    void SetExposure(f32 aperture, f32 shutter_speed, f32 iso);

  private:
    Math::float3 m_eye, m_at, m_up;
    Math::float3 m_forward, m_right;
    Math::float4x4 m_view, m_projection;

    f32 m_yaw = -90.0f, m_pitch = 0.0f;

    f32 m_fov, m_aspect_ratio, m_near_plane, m_far_plane;
    f32 m_camera_speed{};

    //exposure settings
    f32 aperture;
    f32 shutter_speed;
    f32 iso;
    f32 ev100;
    f32 exposure;
};



} // namespace Horizon
