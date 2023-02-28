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
		this->scaling = std::move(scales);
	}

	void Sample(TransformComponent& out, float time) const;

private:
	void SamplePosition(glm::vec3& position, float time) const;
	void SampleRotation(glm::fquat& rotation, float time) const;
	void SampleScaling(glm::vec3& scale, float time) const;

private:
	std::vector<Vector3Track> positions;
	std::vector<QuaternionTrack> rotations;
	std::vector<Vector3Track> scaling;

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
		if (scaling.size() == 1)
			return scaling[0].value;

		const uint32_t nextIndex = static_cast<uint32_t>((index + 1) % scaling.size());
		const Vector3Track& start = scaling[index];
		const Vector3Track& end = scaling[nextIndex];

		float t = CalculateTimeScale(start.time, end.time, time);
		return glm::lerp(start.value, end.value, t);
	}


};

class Pose
{
protected:
	std::vector<TransformComponent> mJoints;
	std::vector<int> mParents;
	std::vector<glm::mat4> mMatrixPallete;
	bool mDirty;
public:
	Pose(){}

	Pose(const Pose& pose);
	Pose& operator=(const Pose& other);


	void Resize(uint32_t size) {
		mJoints.resize(size);
		mParents.resize(size);
		mMatrixPallete.resize(size);
	}

	uint32_t GetSize() { return (uint32_t)mJoints.size(); }

	TransformComponent GetGlobalTransform(uint32_t index) const
	{
		TransformComponent result = mJoints[index];
		for (int parent = mParents[index]; parent >= 0; parent = mParents[parent]) {
			result = TransformComponent::Combine(mJoints[parent], result);
		}
		return result;
	}

	TransformComponent& GetLocalTransform(uint32_t index) {
		return mJoints[index];
	}

	void SetLocalTransform(uint32_t index, TransformComponent component) {
		mJoints[index] = component;
	}

	TransformComponent operator[](uint32_t index) {
		return mJoints[index];
	}
/*
	void CalculateMatrixPallete(int current, const TransformComponent& rootTransform)
	{
		TransformComponent currentTransform = TransformComponent::Combine(rootTransform, mJoints[current]);
		mMatrixPallete[current] = currentTransform.CalculateWorldMatrix();
		std::vector<int> childrens = FindChildren(current);
		for (auto child : childrens)
			CalculateMatrixPallete(child, currentTransform);
	}
	*/
	void GetMatrixPallete(std::vector<glm::mat4>& out)
	{
		if (mDirty)
		{
			// Find Root Bone
			auto found = std::find(mParents.begin(), mParents.end(), -1);
			int rootBone = (int)std::distance(mParents.begin(), found);

			uint32_t size = (uint32_t)mJoints.size();
			if (size != mMatrixPallete.size())
				mMatrixPallete.resize(size);

			TransformComponent rootTransform = {};
			for (uint32_t i = 0; i < mJoints.size(); ++i)
				mMatrixPallete[i] = GetGlobalTransform(i).CalculateWorldMatrix();

			//CalculateMatrixPallete(rootBone, rootTransform);
			mDirty = false;
		}
		out = mMatrixPallete;
	}

	void SetParent(unsigned int index, int parent)
	{
		mParents[index] = parent;
	}

	std::vector<int> FindChildren(int parent)
	{
		std::vector<int> childrens;
		for (auto it = mParents.begin(); it != mParents.end(); it++) {
			if (*it == parent) {
				childrens.push_back((int)std::distance(mParents.begin(), it));
			}
		}
		return childrens;
	}

	int GetParent(uint32_t index)
	{
		return mParents[index];
	}

	void SetDirty(bool state)
	{
		mDirty = state;
	}
};

class Skeleton
{
protected:
	Pose mRestPose;
	Pose mBindPose;
	Pose mAnimatedPose;

	uint32_t mRootBone;
	std::vector<glm::mat4> mInvBindPose;
	std::vector<std::string> mJointNames;

public:
	std::vector<glm::mat4> mLocalTransform;

	Skeleton();
	Skeleton(const Pose& rest, const Pose& bind, const std::vector<std::string>& names);
	void Set(const Pose& rest, const Pose& bind, const std::vector<std::string>& names);
	void SetRootBone(uint32_t rootBone) {
		mRootBone = rootBone;
	}

	uint32_t GetRootBone()
	{
		return mRootBone;
	}

	void Resize(uint32_t size)
	{
		mInvBindPose.resize(size);
		mJointNames.resize(size);
		mLocalTransform.resize(size);
	}

	Pose& GetBindPose() { return mBindPose; }
	Pose& GetRestPose() { return mRestPose; }
	Pose& GetAnimatedPose() { return mAnimatedPose; }

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

class AnimationClip
{
public:
	AnimationClip() {
		mName = "No name";
		mTickPerSeconds = 1000.0f;
		mStartTime = 0.0f;
		mEndTime = 0.0f;
		mLooping = true;
	}

	float GetDuration() const {
		return mEndTime - mStartTime;
	}

	bool IsLooping() const {
		return mLooping;
	}

	void SetLooping(bool looping)
	{
		mLooping = looping;
	}

	void SetTickPerSeconds(float tickPerSeconds)
	{
		mTickPerSeconds = tickPerSeconds;
	}

	void SetStartTime(float startTime)
	{
		mStartTime = startTime;
	}

	void SetEndTime(float endTime)
	{
		mEndTime = endTime;
	}

	float GetTickPerSeconds() const
	{
		return mTickPerSeconds;
	}

	const TransformTrack& operator[](unsigned int joint) const
	{
		return mTracks[joint];
	}

	std::string& GetName()
	{
		return mName;
	}

	void SetName(const std::string& name)
	{
		mName = name;
	}

	std::vector<TransformTrack>& GetTracks()
	{
		return mTracks;
	}

	void SetTrackAt(uint32_t index, TransformTrack& track)
	{
		mTracks[index] = track;
	}

	uint32_t GetNumTrack() const
	{
		return static_cast<uint32_t>(mTracks.size());
	}

	float Sample(Pose& pose, float time) const;

protected:
	std::string mName;
	float mTickPerSeconds;
	float mStartTime;
	float mEndTime;
	bool mLooping;
	std::vector<TransformTrack> mTracks;

	float AdjustTimeToFitRange(float time) const;
};

