#include "Camera.h"

namespace Horizon
{
	Camera::Camera(Math::vec3 eye, Math::vec3 at, Math::vec3 up) :mEye(eye), mAt(at), mUp(up)
	{
		mForward = normalize(mAt - mEye);
		mRight = cross(mForward, mUp);
		updateViewMatrix();
		//setLookAt(eye, at, up);
	}
	void Camera::setPerspectiveProjectionMatrix(f32 fov, float aspectRatio, float nearPlane, float farPlane)
	{
		mFov = fov;
		mAspectRatio = aspectRatio;
		mNearPlane = nearPlane;
		mFarPlane = farPlane;
		mProjection = Math::perspective(fov, aspectRatio, nearPlane, farPlane);
	}
	Math::mat4 Camera::getProjectionMatrix() const
	{
		return mProjection;
	}

	Math::vec3 Camera::getFov() const
	{
		return Math::vec3();
	}
	Math::vec2 Camera::getNearFarPlane() const
	{
		return Math::vec2(mNearPlane, mFarPlane);
	}
	void Camera::setCameraSpeed(f32 speed)
	{
		mCameraSpeed = speed;
	}
	f32 Camera::getCameraSpeed() const
	{
		return mCameraSpeed;
	}

	void Camera::move(Direction direction)
	{
		switch (direction)
		{
		case Horizon::Direction::FORWARD:
			mEye += mCameraSpeed * mForward;
			break;
		case Horizon::Direction::BACKWARD:
			mEye -= mCameraSpeed * mForward;
			break;
		case Horizon::Direction::RIGHT:
			mEye += mCameraSpeed * mRight;
			break;
		case Horizon::Direction::LEFT:
			mEye -= mCameraSpeed * mRight;
			break;
		case Horizon::Direction::UP:
			mEye += mCameraSpeed * mUp;
			break;
		case Horizon::Direction::DOWN:
			mEye -= mCameraSpeed * mUp;
			break;
		default:
			break;
		}
	}
	void Camera::rotate(f32 xoffset, f32 yoffset)
	{
		mYaw += xoffset;
		mPicth -= yoffset; // TODO: unify axis in different API

		// prevent locked
		if (mPicth > 89.0f)
			mPicth = 89.0f;
		if (mPicth < -89.0f)
			mPicth = -89.0f;
	}
	void Camera::updateViewMatrix()
	{
		// calculate the new Front std::vector
		Math::vec3 front;
		front.x = cos(Math::radians(mYaw)) * cos(Math::radians(mPicth));
		front.y = sin(Math::radians(mPicth));
		front.z = sin(Math::radians(mYaw)) * cos(Math::radians(mPicth));

		mForward = Math::normalize(front);
		mRight = Math::normalize(Math::cross(mForward, Math::vec3(0.0, 1.0, 0.0)));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
		mUp = Math::normalize(Math::cross(mRight, mForward));

		mView = Math::lookAt(mEye, mEye + mForward, mUp);
	}
	Math::mat4 Camera::getViewMatrix() const
	{
		return mView;
	}
	Math::vec3 Camera::getPosition() const
	{
		return mEye;
	}
} // namespace Horizon
