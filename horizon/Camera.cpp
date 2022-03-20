#include "Camera.h"

namespace Horizon
{
	Camera::Camera(vec3 eye, vec3 at, vec3 up) :mEye(eye), mAt(at), mUp(up)
	{
		mForward = normalize(mAt - mEye);
		mRight = cross(mForward, mUp);
		setLookAt(eye, at, up);
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
	void Camera::setLookAt(vec3 eye, vec3 at, vec3 up)
	{
		mEye = eye;
		mAt = at;
		mUp = up;
		updateViewMatrix();
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
		setLookAt(mEye, mEye + mForward, mUp);
	}
	void Camera::updateViewMatrix()
	{
		mView = glm::lookAt(mEye, mAt, mUp);
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
