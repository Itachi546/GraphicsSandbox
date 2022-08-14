#include "Camera.h"

Camera::Camera()
{
	mTargetPosition = mPosition = glm::vec3(0.0f, 1.0f, -3.0f);
	mTargetRotation = mRotation = glm::vec3(0.0f, 0.0f, 0.0f);

	mFov = glm::radians(60.0f);
	mAspect = 4.0f / 3.0f;
	mNearPlane = 0.2f;
	mFarPlane = 5000.0f;

	mTarget = glm::vec3(0.0f, 0.0f, 1.0f);
	mSpeed = 2.0f;
	mSensitivity = 0.3f;

	CalculateProjection();
	CalculateView();
}

void Camera::Update(float dt)
{
	mPosition.x += (mTargetPosition.x - mPosition.x) * 0.35f * dt;
	mPosition.y += (mTargetPosition.y - mPosition.y) * 0.35f * dt;
	mPosition.z += (mTargetPosition.z - mPosition.z) * 0.35f * dt;
	mRotation += (mTargetRotation - mRotation) * 0.35f * dt;

	if (glm::length2(mTargetPosition - mPosition) < 0.00001f)
		mPosition = mTargetPosition;
	if (glm::length2(mTargetRotation - mRotation) < 0.0000001f)
		mRotation = mTargetRotation;

	CalculateView();
//
//
//	
//
//	// Calculate frustum corner
//	static const glm::vec3 frustumCorners[8] =
//	{
//		glm::vec3(-1.0f,  1.0f, -1.0f),
//		glm::vec3(1.0f,  1.0f, -1.0f),
//		glm::vec3(1.0f, -1.0f, -1.0f),
//		glm::vec3(-1.0f, -1.0f, -1.0f),
//		glm::vec3(-1.0f,  1.0f,  1.0f),
//		glm::vec3(1.0f,  1.0f,  1.0f),
//		glm::vec3(1.0f, -1.0f,  1.0f),
//		glm::vec3(-1.0f, -1.0f,  1.0f),
//	};
//
//	// Project frustum corners into world space
//	// inv(view) * inv(projection) * p
//	// inv(A) * inv(B) = inv(BA)
//	std::array<glm::vec3, 8> frustumPoints = {};
//	glm::mat4 invCam = glm::inverse(mProjection * mView);
//	for (uint32_t i = 0; i < 8; i++) 
//	{
//		glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
//		frustumPoints[i] = invCorner / invCorner.w;
//	}
//
//	m_frustum->set_points(frustumPoints);
}

void Camera::CalculateProjection()
{
	mProjection = glm::perspective(mFov, mAspect, mNearPlane, mFarPlane);
	mInvProjection  = glm::inverse(mProjection);
}

void Camera::CalculateView()
{
	glm::mat3 rotate = glm::yawPitchRoll(mRotation.y, mRotation.x, mRotation.z);
	glm::vec3 up = glm::normalize(rotate * glm::vec3(0.0f, 1.0f, 0.0f));
	glm::vec3 forward = glm::normalize(rotate * glm::vec3(0.0f, 0.0f, 1.0f));
	mView = glm::lookAt(mPosition, mPosition + forward , up);
	mInvView = glm::inverse(mView);

	mRight   = glm::vec3(mView[0][0], mView[1][0], mView[2][0]);
	mUp      = glm::vec3(mView[0][1], mView[1][1], mView[2][1]);
	mForward = glm::vec3(mView[0][2], mView[1][2], mView[2][2]);
}
