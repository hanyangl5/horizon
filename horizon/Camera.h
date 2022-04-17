#pragma once

#include "utils.h"

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
		Camera(Math::vec3 position, Math::vec3 at, Math::vec3 up);

		void setPerspectiveProjectionMatrix(f32 fov, f32 aspect_ratio, f32 near, f32 far);
		Math::mat4 getProjectionMatrix() const;

		//void setLookAt(vec3 position, vec3 at, vec3 up = vec3(0.0f, 1.0f, 0.0f));
		Math::mat4 getViewMatrix() const;

		Math::vec3 getPosition() const;

		Math::vec3 getFov() const;

		Math::vec2 getNearFarPlane() const;

		void setCameraSpeed(f32 speed);
		f32 getCameraSpeed() const;

		void move(Direction direction);
		void rotate(f32 yaw, f32 pitch);

		void updateViewMatrix();
	private:
		Math::vec3 m_eye, m_at, m_up;
		Math::vec3 m_forward, m_right;
		Math::mat4 m_view, m_projection;

		f32 m_yaw = -90.0f, m_pitch = 0.0f;

		f32 m_fov, m_aspect_ratio, m_near_plane, m_far_plane;
		f32 m_camera_speed;
	};

}
