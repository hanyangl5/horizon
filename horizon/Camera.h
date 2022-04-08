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

		void setPerspectiveProjectionMatrix(f32 fov, float aspectRatio, float near, float far);
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
		Math::vec3 mEye, mAt, mUp;
		Math::vec3 mForward, mRight;
		Math::mat4 mView, mProjection;

		f32 mYaw = -90.0f, mPicth = 0.0f;

		f32 mFov, mAspectRatio, mNearPlane, mFarPlane;
		f32 mCameraSpeed;
	};

}
