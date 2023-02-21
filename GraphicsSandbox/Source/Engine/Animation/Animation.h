#pragma once

#include <vector>
#include <string>
#include "../TransformComponent.h"
#include "../../Shared/MeshData.h"

struct TransformComponent;

constexpr int MAX_BONE_COUNT = 80;
class TransformTrack
{
public:
	TransformTrack()
	{
	}

	void AddPositions(std::vector<Vector3Track>& positions)
	{
		this->positions = std::move(positions);
	}

	void AddRotations(std::vector<QuaternionTrack>& rotations)
	{
		this->rotations = std::move(rotations);
	}

	void AddScale(std::vector<Vector3Track>& scales)
	{
		this->scale = std::move(scales);
	}

	glm::mat4 CalculateTransform(float time) const
	{
		uint32_t positionIndex = ~0u;
		if (positions.size() > 0)
		{
			positionIndex = 0;
			for (uint32_t i = 0; i < positions.size() - 1; ++i)
			{
				if (time < positions[i + 1].time)
				{
					positionIndex = i;
					break;
				}
			}
		}

		uint32_t rotationIndex = ~0u;
		if (rotations.size() > 0)
		{
			rotationIndex = 0;
			for (uint32_t i = 0; i < rotations.size() - 1; ++i)
			{
				if (time < rotations[i + 1].time)
				{
					rotationIndex = i;
					break;
				}
			}
		}
		uint32_t scalingIndex = ~0u;
		if (scale.size() > 0)
		{
			scalingIndex = 0;
			for (uint32_t i = 0; i < scale.size() - 1; ++i)
			{
				if (time < scale[i + 1].time) {
					scalingIndex = i;
					break;
				}
			}
		}

		if (positionIndex == ~0u)
			return glm::mat4(1.0f);

		glm::vec3 position = glm::vec3(0.0);
		position = InterpolatePosition(positionIndex, time);

		glm::fquat rotation = glm::fquat(1.0f, 0.0f, 0.0f, 0.0f);
		rotation = InterpolateRotation(rotationIndex, time);

		glm::vec3 scaling = glm::vec3(1.0f);
		scaling = InterpolateScale(scalingIndex, time);

		return glm::translate(glm::mat4(1.0f), position) *
			glm::toMat4(rotation) *
			glm::scale(glm::mat4(1.0f), scaling);
	}


private:
	std::vector<Vector3Track> positions;
	std::vector<QuaternionTrack> rotations;
	std::vector<Vector3Track> scale;

	float CalculateTimeScale(float start, float end, float time) const
	{
		return (time - start) / (end - start);
	}

	glm::vec3 InterpolatePosition(uint32_t index, float time) const
	{
		if (positions.size() == 1)
			return positions[0].value;

		const uint32_t nextIndex = static_cast<uint32_t>((index + 1) % positions.size());
		const Vector3Track& start = positions[index];
		const Vector3Track& end = positions[nextIndex];

		float t = CalculateTimeScale(start.time, end.time, time);
		return glm::lerp(start.value, end.value, t);
	}

	glm::fquat InterpolateRotation(uint32_t index, float time) const
	{
		if (rotations.size() == 1)
			return rotations[0].value;

		const uint32_t nextIndex = static_cast<uint32_t>((index + 1) % rotations.size());
		QuaternionTrack start = rotations[index];
		QuaternionTrack end = rotations[nextIndex];

		float t = CalculateTimeScale(start.time, end.time, time);
		return glm::slerp(start.value, end.value, t);
	}

	glm::vec3 InterpolateScale(uint32_t index, float time) const
	{
		if (scale.size() == 1)
			return scale[0].value;

		const uint32_t nextIndex = static_cast<uint32_t>((index + 1) % scale.size());
		const Vector3Track& start = scale[index];
		const Vector3Track& end = scale[nextIndex];

		float t = CalculateTimeScale(start.time, end.time, time);
		return glm::lerp(start.value, end.value, t);
	}


};

class Pose
{
protected:
	std::vector<TransformComponent> mJoints;
	std::vector<int> mParents;

public:
	Pose(){}

	void Resize(uint32_t size) {
		mJoints.resize(size);
		mParents.resize(size);
	}

	uint32_t GetSize() { return (uint32_t)mJoints.size(); }

	TransformComponent& GetLocalTransform(uint32_t index) {
		return mJoints[index];
	}

	void SetLocalTransform(uint32_t index, TransformComponent component) {
		mJoints[index] = component;
	}

	TransformComponent operator[](uint32_t index) {
		return mJoints[index];
	}

	void SetParent(unsigned int index, int parent)
	{
		mParents[index] = parent;
	}

	std::vector<uint32_t> FindChildren(uint32_t parent)
	{
		std::vector<uint32_t> childrens;
		for (auto it = mParents.begin(); it != mParents.end(); it++) {
			if (*it == parent) {
				childrens.push_back((uint32_t)std::distance(mParents.begin(), it));
			}
		}
		return childrens;
	}

	unsigned int GetParent(uint32_t index)
	{
		return mParents[index];
	}
};

class Skeleton
{
protected:
	Pose mRestPose;
	Pose mBindPose;

	std::vector<glm::mat4> mInvBindPose;
	std::vector<std::string> mJointNames;

public:
	std::vector<glm::mat4> mLocalTransform;
	std::vector<glm::mat4> mFinalTransform;

	Skeleton();
	Skeleton(const Pose& rest, const Pose& bind, const std::vector<std::string>& names);
	void Set(const Pose& rest, const Pose& bind, const std::vector<std::string>& names);

	void Resize(uint32_t size)
	{
		mInvBindPose.resize(size);
		mJointNames.resize(size);
		mLocalTransform.resize(size);
		mFinalTransform.resize(size);
	}

	Pose& GetBindPose() { return mBindPose; }
	Pose& GetRestPose() { return mRestPose; }

	std::string& GetJointName(unsigned int index) {
		return mJointNames[index];
	}

	glm::mat4& GetInvBindPose(uint32_t index) {
		return mInvBindPose[index];
	}

	void SetJointName(uint32_t index, std::string_view name)
	{
		mJointNames[index] = name;
	}

	void SetInvBindPose(uint32_t index, const glm::mat4& invBindPose)
	{
		mInvBindPose[index] = invBindPose;
	}

	std::vector<std::string>& GetJointArray() {
		return mJointNames;
	}
};