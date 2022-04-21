#pragma once

#include <runtime/function/render/RenderContext.h>

namespace Horizon {

	enum class Direction
	{
		FORWARD,
		BACKWARD,
		LEFT,
		RIGHT,
		UP,
		DOWN
	};

	class Camera {
	public:
		Camera(Math::vec3 position, Math::vec3 at, Math::vec3 up) noexcept;

		void SetPerspectiveProjectionMatrix(f32 fov, f32 aspect_ratio, f32 near, f32 far) noexcept;
		Math::mat4 GetProjectionMatrix() const noexcept;

		//void setLookAt(vec3 position, vec3 at, vec3 up = vec3(0.0f, 1.0f, 0.0f));
		Math::mat4 GetViewMatrix() const noexcept;

		Math::vec3 GetPosition() const noexcept;

		Math::vec3 GetFov() const noexcept;

		Math::vec2 GetNearFarPlane() const noexcept;

		void SetCameraSpeed(f32 speed) noexcept;
		f32 GetCameraSpeed() const noexcept;

		void Move(Direction direction) noexcept;
		void Rotate(f32 yaw, f32 pitch) noexcept;

		void UpdateViewMatrix() noexcept;

		Math::mat4 GetInvViewProjectionMatrix() const noexcept;

		Math::vec3 GetForwardDir() const noexcept;
	private:
		Math::vec3 m_eye, m_at, m_up;
		Math::vec3 m_forward, m_right;
		Math::mat4 m_view, m_projection;

		f32 m_yaw = -90.0f, m_pitch = 0.0f;

		f32 m_fov, m_aspect_ratio, m_near_plane, m_far_plane;
		f32 m_camera_speed;
	};

}
