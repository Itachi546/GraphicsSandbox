#pragma once

#include "GlmIncludes.h"
#include "../Shared/MathUtils.h"

#include <memory>

class Camera
{
	friend class Scene;
public:
	Camera();

	void Update(float dt);
	glm::mat4 GetProjectionMatrix() const { return mProjection; }
	glm::mat4 GetViewMatrix()       const { return mView; }

	glm::mat4 GetInvProjectionMatrix() const { return mInvProjection; }
	glm::mat4 GetInvViewMatrix()       const { return mInvView; }

	void SetPosition(glm::vec3 p)
	{
		mPosition = p;
		mTargetPosition = p;
	}

	void SetRotation(glm::vec3 rotation)
	{
		mRotation = mTargetRotation = rotation;
	}

	void SetFreezeFrustum(bool state)
	{
		mFreezeFrustum = state;
	}

	glm::vec3 GetRotation() const 
	{
		return mRotation;
	}

	void SetNearPlane(float d)
	{
		mNearPlane = d;
		CalculateProjection();
	}

	void SetFarPlane(float d)
	{
		mFarPlane = d;
		CalculateProjection();
	}

	void SetAspect(float aspect) 
	{
		mAspect = aspect;
		CalculateProjection();
	}

	void SetFOV(float fov)
	{
		mFov = fov;
		CalculateProjection();
	}

	inline float GetFOV() const { return mFov; }
	inline float GetAspect() const { return mAspect; }
	inline float GetNearPlane() const { return mNearPlane; }
	inline 	float GetFarPlane()  const { return mFarPlane; }

	void Walk(float dt)
	{
		mTargetPosition += mSpeed * dt * mForward;
	}

	void Strafe(float dt)
	{
		mTargetPosition += mSpeed * dt * mRight;
	}

	void Lift(float dt)
	{
		mTargetPosition += mSpeed * dt * mUp;
	}

	void Rotate(float dx, float dy, float dt)
	{
		float m = mSensitivity * dt;
		mTargetRotation.y += dy * m;
		mTargetRotation.x += dx * m;

		constexpr float maxAngle = glm::radians(89.99f);
		mTargetRotation.x = glm::clamp(mTargetRotation.x, -maxAngle, maxAngle);
	}

	glm::vec4 ComputeNDCCoordinate(const glm::vec3& p);

	glm::vec3 GetForward() { return mForward; }
	glm::vec3 GetPosition() const { return mPosition; }
	
	std::shared_ptr<Frustum> mFrustum;
	glm::vec3 mFrustumPoints[8];
private:

	glm::vec3 mPosition;
	glm::vec3 mRotation;

	glm::vec3 mTargetPosition;
	glm::vec3 mTargetRotation;

	glm::mat4 mProjection, mInvProjection;
	float mFov;
	float mAspect;
	float mNearPlane;
	float mFarPlane;

	glm::mat4 mView, mInvView;
	glm::vec3 mRight;
	glm::vec3 mUp;
	glm::vec3 mForward;
	float mSpeed;
	float mSensitivity;

	bool mFreezeFrustum = false;
	glm::mat4 mFreezeVP;

	void CalculateProjection();
	void CalculateView();
	void CalculateFrustumFast();
	void CalculateFrustumPoints();

};