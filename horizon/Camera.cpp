#include "Camera.h"

namespace Horizon
{
	Camera::Camera(vec3 position, vec3 at, vec3 up) :mEye(position), mCenter(at), mUp(up)
	{
		setLookAt(position, at, up);
	}
	void Camera::setProjectionMatrix(f32 fov, float aspectRatio, float nearPlane, float farPlane)
	{
		mFov = fov;
		mAspectRatio = aspectRatio;
		mNearPlane = nearPlane;
		mFarPlane = farPlane;
		mProjection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);
	}
	mat4 Camera::getProjectionMatrix()
	{
		return mProjection;
	}
	void Camera::setLookAt(vec3 position, vec3 at, vec3 up)
	{
		mView = glm::lookAt(position, at, up);
	}
	mat4 Camera::getViewMatrix()
	{
		return mView;
	}
	vec3 Camera::getPosition()
	{
		return mEye;
	}
} // namespace Horizon
