#pragma once

#include <vector>
#include <string>
#include "../TransformComponent.h"

struct TransformComponent;
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

	TransformComponent GetLocalTransform(uint32_t index) {
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
	Skeleton();
	Skeleton(const Pose& rest, const Pose& bind, const std::vector<std::string>& names);
	void Set(const Pose& rest, const Pose& bind, const std::vector<std::string>& names);

	Pose& GetBindPose() { return mBindPose; }
	Pose& GetRestPose() { return mRestPose; }

	std::string& GetJointName(unsigned int index) {
		return mJointNames[index];
	}

	std::vector<std::string>& GetJointArray() {
		return mJointNames;
	}
};