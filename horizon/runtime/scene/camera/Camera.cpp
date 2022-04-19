#include "Camera.h"

namespace Horizon
{
	Camera::Camera(Math::vec3 eye, Math::vec3 at, Math::vec3 up) :m_eye(eye), m_at(at), m_up(up)
	{
		m_forward = normalize(m_at - m_eye);
		m_right = cross(m_forward, m_up);
		UpdateViewMatrix();
		//setLookAt(eye, at, up);
	}
	void Camera::SetPerspectiveProjectionMatrix(f32 fov, f32 aspect_ratio, f32 nearPlane, f32 farPlane)
	{
		m_fov = fov;
		m_aspect_ratio = aspect_ratio;
		m_near_plane = nearPlane;
		m_far_plane = farPlane;
		m_projection = Math::perspective(fov, aspect_ratio, nearPlane, farPlane);
	}
	Math::mat4 Camera::GetProjectionMatrix() const
	{
		return m_projection;
	}

	Math::vec3 Camera::GetFov() const
	{
		return Math::vec3();
	}

	Math::vec2 Camera::GetNearFarPlane() const
	{
		return Math::vec2(m_near_plane, m_far_plane);
	}

	void Camera::SetCameraSpeed(f32 speed)
	{
		m_camera_speed = speed;
	}

	f32 Camera::GetCameraSpeed() const
	{
		return m_camera_speed;
	}

	void Camera::Move(Direction direction)
	{
		switch (direction)
		{
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
	void Camera::Rotate(f32 xoffset, f32 yoffset)
	{
		m_yaw += xoffset;
		m_pitch -= yoffset; // TODO: unify axis in different API

		// prevent locked
		if (m_pitch > 89.0f)
			m_pitch = 89.0f;
		if (m_pitch < -89.0f)
			m_pitch = -89.0f;
	}
	void Camera::UpdateViewMatrix()
	{
		// calculate the new Front std::vector
		Math::vec3 front;
		front.x = cos(Math::radians(m_yaw)) * cos(Math::radians(m_pitch));
		front.y = sin(Math::radians(m_pitch));
		front.z = sin(Math::radians(m_yaw)) * cos(Math::radians(m_pitch));

		m_forward = Math::normalize(front);
		m_right = Math::normalize(Math::cross(m_forward, Math::vec3(0.0, 1.0, 0.0)));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower Movement.
		m_up = Math::normalize(Math::cross(m_right, m_forward));

		m_view = Math::lookAt(m_eye, m_eye + m_forward, m_up);
	}
	Math::mat4 Camera::GetViewMatrix() const
	{
		return m_view;
	}
	Math::vec3 Camera::GetPosition() const
	{
		return m_eye;
	}
} // namespace Horizon
