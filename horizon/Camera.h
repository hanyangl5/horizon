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
		Camera(vec3 position, vec3 at, vec3 up);

		void setPerspectiveProjectionMatrix(f32 fov, float aspectRatio, float near, float far);
		mat4 getProjectionMatrix() const;

		//void setLookAt(vec3 position, vec3 at, vec3 up = vec3(0.0f, 1.0f, 0.0f));
		mat4 getViewMatrix() const;

		vec3 getPosition() const;

		vec3 getFov() const;

		void setCameraSpeed(f32 speed);
		f32 getCameraSpeed() const;

		void move(Direction direction);
		void rotate(f32 yaw, f32 pitch);

		void updateViewMatrix();
	private:
		vec3 mEye, mAt, mUp;
		vec3 mForward, mRight;
		mat4 mView, mProjection;

		f32 mYaw = -90.0f, mPicth = 0.0f;

		f32 mFov, mAspectRatio, mNearPlane, mFarPlane;
		f32 mCameraSpeed;
	};

}


// Defines several possible options for camera movement. Used as abstraction to
// stay away from window-system specific input methods
//enum class Direction
//{
//	FORWARD,
//	BACKWARD,
//	LEFT,
//	RIGHT,
//	UP,
//	DOWN
//};

//// Default camera values
//const float YAW = -90.0f;
//const float PITCH = 0.0f;
//const float SPEED = 0.1f;
//const float SENSITIVITY = 0.1f;
//const float ZOOM = 90.0f;
// 
// //An abstract camera class that processes input and calculates the
// //corresponding Euler Angles, Vectors and Matrices for use in OpenGL
//class Camera
//{
//public:
//	// camera Attributes
//	vec3 Position;
//	vec3 Front;
//	vec3 Up;
//	vec3 Right;
//	vec3 WorldUp;
//	// euler Angles
//	float Yaw;
//	float Pitch;
//	// camera options
//	float MovementSpeed;
//	float MouseSensitivity;
//	float Zoom;
//	mat4 view, projection;
//	// constructor with vectors
//	Camera(vec3 position = vec3(0.0f, 0.0f, 0.0f),
//		   vec3 up = vec3(0.0f, 1.0f, 0.0f), float yaw = YAW,
//		   float pitch = PITCH)
//		: Front(vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED),
//		  MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
//	{
//		Position = position;
//		WorldUp = up;
//		Yaw = yaw;
//		Pitch = pitch;
//		UpdateCameraVectors();
//	}
//	// constructor with scalar values
//	Camera(float posX, float posY, float posZ, float upX, float upY, float upZ,
//		   float yaw, float pitch)
//		: Front(vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED),
//		  MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
//	{
//		Position = vec3(posX, posY, posZ);
//		WorldUp = vec3(upX, upY, upZ);
//		Yaw = yaw;
//		Pitch = pitch;
//		UpdateCameraVectors();
//	}

//	// returns the view matrix calculated using Euler Angles and the LookAt Matrix
//	mat4 GetViewMatrix()
//	{
//		view = glm::lookAt(Position, Position + Front, Up);
//		return view;
//	}
//   mat4 getProjectionMatrix() {
//       return projection;
//   }
//	void SetProjectionMatrix(mat4 proj) { projection = proj; }
//	// processes input received from any keyboard-like input system. Accepts input
//	// parameter in the form of camera defined ENUM (to abstract it from windowing
//	// systems)
//	void ProcessKeyboard(Direction direction, float deltaTime)
//	{
//		float velocity = MovementSpeed * deltaTime;
//		switch (direction)
//		{
//		case Direction::FORWARD:
//			Position += Front * velocity;
//			break;
//		case Direction::BACKWARD:
//			Position -= Front * velocity;
//			break;
//		case Direction::LEFT:
//			Position -= Right * velocity;
//			break;
//		case Direction::RIGHT:
//			Position += Right * velocity;
//			break;
//		case Direction::UP:
//			Position += Up * velocity;
//			break;
//		case Direction::DOWN:
//			Position -= Up * velocity;
//			break;
//		default:
//			break;
//		}
//	}

//	// processes input received from a mouse input system. Expects the offset
//	// value in both the x and y direction.
//	void ProcessMouseMovement(float xoffset, float yoffset,
//							  bool constrainPitch = true)
//	{
//		xoffset *= MouseSensitivity;
//		yoffset *= MouseSensitivity;

//		Yaw += xoffset;
//		Pitch += yoffset;

//		// prevent locked
//		// make sure that when pitch is out of bounds, screen doesn't get flipped
//		if (constrainPitch)
//		{
//			if (Pitch > 89.0f)
//				Pitch = 89.0f;
//			if (Pitch < -89.0f)
//				Pitch = -89.0f;
//		}

//		// update Front, Right and Up Vectors using the updated Euler angles
//		UpdateCameraVectors();
//	}

//	// processes input received from a mouse scroll-wheel event. Only requires
//	// input on the vertical wheel-axis
//	void ProcessMouseScroll(float yoffset)
//	{
//		Zoom -= (float)yoffset;
//		if (Zoom < 1.0f)
//			Zoom = 1.0f;
//		if (Zoom > 45.0f)
//			Zoom = 45.0f;
//	}

//private:
//	void UpdateCameraVectors()
//	{
//		// calculate the new Front vector
//		vec3 front;
//		front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
//		front.y = sin(glm::radians(Pitch));
//		front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
//		Front = glm::normalize(front);
//		// also re-calculate the Right and Up vector
//		Right = glm::normalize(glm::cross(
//			Front, WorldUp)); // normalize the vectors, because their length gets
//							  // closer to 0 the more you look up or down which
//							  // results in slower movement.
//		Up = glm::normalize(glm::cross(Right, Front));
//	}
//};
