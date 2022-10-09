#include "Camera.h"
#include "../Shared/MathUtils.h"
#include "DebugDraw.h"

Camera::Camera()
{
	mTargetPosition = mPosition = glm::vec3(0.0f, 1.0f, 3.0f);
	mTargetRotation = mRotation = glm::vec3(0.0f, 0.0f, 0.0f);

	mFov = glm::radians(60.0f);
	mAspect = 4.0f / 3.0f;
	mNearPlane = 0.1f;
	mFarPlane = 1000.0f;

	mSpeed = 2.0f;
	mSensitivity = 0.3f;

	mFrustum = std::make_shared<Frustum>();

	CalculateProjection();
	CalculateView();
}

void Camera::Update(float dt)
{
	//mPosition = mTargetPosition;
	//mRotation = mTargetRotation;

	mPosition.x += (mTargetPosition.x - mPosition.x) * 0.8f * dt;
	mPosition.y += (mTargetPosition.y - mPosition.y) * 0.8f * dt;
	mPosition.z += (mTargetPosition.z - mPosition.z) * 0.8f * dt;
	mRotation += (mTargetRotation - mRotation) * 0.8f * dt;

	CalculateView();
	//DebugDraw::AddFrustum(mFrustumPoints, 8, 0x00ff00);
}

void Camera::CalculateProjection()
{
	mProjection = glm::perspective(mFov, mAspect, mNearPlane, mFarPlane);
	mInvProjection  = glm::inverse(mProjection);
}

void Camera::CalculateView()
{
	glm::mat3 rotation = glm::yawPitchRoll(mRotation.y, mRotation.x, mRotation.z);

	mForward = glm::normalize(rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	mUp = glm::normalize(rotation * glm::vec3(0.0f, 1.0f, 0.0f));

	mView = glm::lookAt(mPosition, mPosition + mForward, mUp);
	mInvView = glm::inverse(mView);

	mRight   = glm::vec3(mView[0][0], mView[1][0], mView[2][0]);
	mUp      = glm::vec3(mView[0][1], mView[1][1], mView[2][1]);
	mForward = glm::vec3(mView[0][2], mView[1][2], mView[2][2]);

	if (!mFreezeFrustum)
	{
		mFreezeVP = mProjection * mView;
		CalculateFrustumPoints();
		CalculateFrustumFast();
	}
}

/*
* Calculate Worldspace frustum plane using 
* Fast Extraction of Viewing Frustum Planes from the 
  View-Projection Matrix (Gribb and Hartmann 2001)
*/

void Camera::CalculateFrustumFast()
{
	glm::vec4 col0 = glm::row(mFreezeVP, 0);
	glm::vec4 col1 = glm::row(mFreezeVP, 1);
	glm::vec4 col2 = glm::row(mFreezeVP, 2);
	glm::vec4 col3 = glm::row(mFreezeVP, 3);

	// Left
	mFrustum->planes[Frustum::Plane::Left] = Plane(col3 + col0);
	// Right
	mFrustum->planes[Frustum::Plane::Right] = Plane(col3 - col0);
    // Bottom
	mFrustum->planes[Frustum::Plane::Bottom] = Plane(col3 + col1);
	// Top
	mFrustum->planes[Frustum::Plane::Top] = Plane(col3 - col1);
	// Near
	mFrustum->planes[Frustum::Plane::Near] = Plane(col3 + col2);
	// Far
	mFrustum->planes[Frustum::Plane::Far] = Plane(col3 - col2);
}

void Camera::CalculateFrustumPoints()
{
	static const glm::vec3 frustumCorners[8] =
	{
		glm::vec3(-1.0f,  1.0f, -1.0f),  // NTL
		glm::vec3(1.0f,  1.0f, -1.0f),   // NTR
		glm::vec3(1.0f, -1.0f, -1.0f),   // NBR
		glm::vec3(-1.0f, -1.0f, -1.0f),  // NBL
		glm::vec3(-1.0f,  1.0f,  1.0f),  // FTL
		glm::vec3(1.0f,  1.0f,  1.0f),   // FTR
		glm::vec3(1.0f, -1.0f,  1.0f),   // FBR
		glm::vec3(-1.0f, -1.0f,  1.0f),  // FBL
	};

	// Project frustum corners into world space
	// inv(view) * inv(projection) * p
	// inv(A) * inv(B) = inv(BA)
	std::array<glm::vec3, 8> frustumPoints = {};
	glm::mat4 invCam = glm::inverse(mFreezeVP);
	for (uint32_t i = 0; i < 8; i++)
	{
		glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
		mFrustumPoints[i] = invCorner / invCorner.w;
	}
}