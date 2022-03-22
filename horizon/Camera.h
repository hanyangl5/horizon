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
