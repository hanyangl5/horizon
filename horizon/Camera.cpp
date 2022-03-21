#include "Camera.h"

namespace Horizon
{
	Camera::Camera(vec3 eye, vec3 at, vec3 up) :mEye(eye), mAt(at), mUp(up)
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
		mProjection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);
	}
	mat4 Camera::getProjectionMatrix() const
	{
		return mProjection;
	}

	vec3 Camera::getFov() const
	{
		return vec3();
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
		mPicth += yoffset;

		// prevent locked
		if (mPicth > 89.0f)
			mPicth = 89.0f;
		if (mPicth < -89.0f)
			mPicth = -89.0f;
	}
	void Camera::updateViewMatrix()
	{
		// calculate the new Front vector
		glm::vec3 front;
		front.x = cos(glm::radians(mYaw)) * cos(glm::radians(mPicth));
		front.y = sin(glm::radians(mPicth));
		front.z = sin(glm::radians(mYaw)) * cos(glm::radians(mPicth));

		mForward = glm::normalize(front);
		mRight = glm::normalize(glm::cross(mForward, vec3(0.0, 1.0, 0.0)));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
		mUp = glm::normalize(glm::cross(mRight, mForward));

		mView = glm::lookAt(mEye, mEye + mForward, mUp);
	}
	mat4 Camera::getViewMatrix() const
	{
		return mView;
	}
	vec3 Camera::getPosition() const
	{
		return mEye;
	}
} // namespace Horizon
