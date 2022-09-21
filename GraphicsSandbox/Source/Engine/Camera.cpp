#include "Camera.h"
#include "../Shared/MathUtils.h"
#include "DebugDraw.h"

Camera::Camera()
{
	mTargetPosition = mPosition = glm::vec3(0.0f, 1.0f, 3.0f);
	mTargetRotation = mRotation = glm::vec3(0.0f, 0.0f, 0.0f);

	mFov = glm::radians(60.0f);
	mAspect = 4.0f / 3.0f;
	mNearPlane = 0.2f;
	mFarPlane = 500.0f;

	mSpeed = 2.0f;
	mSensitivity = 0.3f;

	mFrustum = std::make_shared<Frustum>();

	CalculateProjection();
	CalculateView();
}

void Camera::Update(float dt)
{
	mPosition.x += (mTargetPosition.x - mPosition.x) * 0.8f * dt;
	mPosition.y += (mTargetPosition.y - mPosition.y) * 0.8f * dt;
	mPosition.z += (mTargetPosition.z - mPosition.z) * 0.8f * dt;
	mRotation += (mTargetRotation - mRotation) * 0.8f * dt;
	/*
	if (glm::length2(mTargetPosition - mPosition) < 0.00001f)
		mPosition = mTargetPosition;
	if (glm::length2(mTargetRotation - mRotation) < 0.0000001f)
		mRotation = mTargetRotation;
		*/
	CalculateView();

	if (!mFreezeFrustum)
	{
		mFreezeVP = mView;
		CalculateFrustum();
	}
	DebugDraw::AddFrustum(mFrustumPoints);
}

void Camera::CalculateProjection()
{
	mProjection = glm::perspective(mFov, mAspect, mNearPlane, mFarPlane);
	mInvProjection  = glm::inverse(mProjection);
}

void Camera::CalculateView()
{
	glm::mat3 rotation = glm::yawPitchRoll(mRotation.y, mRotation.x, mRotation.z);
	mForward = rotation * glm::vec3(0.0f, 0.0f, 1.0f);

	mView = glm::lookAt(mPosition, mPosition + mForward, glm::vec3(0.0f, 1.0f, 0.0f));
	mInvView = glm::inverse(mView);

	mRight   = glm::vec3(mView[0][0], mView[1][0], mView[2][0]);
	mUp      = glm::vec3(mView[0][1], mView[1][1], mView[2][1]);
}

/*
* Calculate Worldspace frustum plane using 
* Fast Extraction of Viewing Frustum Planes from the 
  View-Projection Matrix (Gribb and Hartmann 2001)
*/
/*
void Camera::CalculateFrustum()
{
	mFrustum->frustumPlanes[Frustum::PlaneLocation::Left]   = Plane(mFreezeVP[3] + mFreezeVP[0]);
	mFrustum->frustumPlanes[Frustum::PlaneLocation::Right]  = Plane(mFreezeVP[3] - mFreezeVP[0]);
	mFrustum->frustumPlanes[Frustum::PlaneLocation::Bottom] = Plane(mFreezeVP[3] + mFreezeVP[1]);
	mFrustum->frustumPlanes[Frustum::PlaneLocation::Top]    = Plane(mFreezeVP[3] - mFreezeVP[1]);
	mFrustum->frustumPlanes[Frustum::PlaneLocation::Near]   = Plane(mFreezeVP[3] + mFreezeVP[2]);
	mFrustum->frustumPlanes[Frustum::PlaneLocation::Far]    = Plane(mFreezeVP[3] - mFreezeVP[2]);
}
*/
void Camera::CalculateFrustum()
{
	const float tanHFov = std::tan(mFov * 0.5f);
	// calculate the point in the middle of near plane
	const glm::vec3 nearPoint = mPosition + mNearPlane * mForward;

	// calculate near half width as tan(fov) * zNear_
	const float nhh = tanHFov * mNearPlane;
	const float nhw = mAspect * nhh;

	// calculate the right and up offset to reach the edge of near plane
	const glm::vec3 nearRight = nhw * mRight;
	const glm::vec3 nearUp = nhh * mUp;

	mFrustumPoints[Frustum::PointLocation::NTL] = nearPoint - nearRight + nearUp;
	mFrustumPoints[Frustum::PointLocation::NTR] = nearPoint + nearRight + nearUp;
	mFrustumPoints[Frustum::PointLocation::NBL] = nearPoint - nearRight - nearUp;
	mFrustumPoints[Frustum::PointLocation::NBR] = nearPoint + nearRight - nearUp;

	// same with the far plane
	const glm::vec3 farPoint = mPosition + mFarPlane * mForward;
	const float fhh = tanHFov * mFarPlane;
	const float fhw = mAspect * fhh;

	const glm::vec3 farRight = fhw * mRight;
	const glm::vec3 farUp = fhh * mUp;

	mFrustumPoints[Frustum::PointLocation::FTL] = farPoint - farRight + farUp;
	mFrustumPoints[Frustum::PointLocation::FTR] = farPoint + farRight + farUp;
	mFrustumPoints[Frustum::PointLocation::FBL] = farPoint - farRight - farUp;
	mFrustumPoints[Frustum::PointLocation::FBR] = farPoint + farRight - farUp;
	mFrustum->GenerateFromPoints(mFrustumPoints, 8);
}
