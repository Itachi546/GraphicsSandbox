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
	Skeleton();
	Skeleton(const Pose& rest, const Pose& bind, const std::vector<std::string>& names);
	void Set(const Pose& rest, const Pose& bind, const std::vector<std::string>& names);

	void Resize(uint32_t size)
	{
		mInvBindPose.resize(size);
		mJointNames.resize(size);
		mLocalTransform.resize(size);
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