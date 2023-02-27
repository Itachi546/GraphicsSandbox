#include "Animation.h"

void TransformTrack::Sample(TransformComponent& out, float time) const
{
	SamplePosition(out.position, time);
	SampleRotation(out.rotation, time);
	SampleScaling(out.scale, time);
	out.dirty = true;
}

void TransformTrack::SamplePosition(glm::vec3& position, float time) const
{
	uint32_t positionIndex = ~0u;
	if (positions.size() > 0)
	{
		positionIndex = 0;
		for (uint32_t i = 0; i < positions.size() - 1; ++i)
		{
			if (time <= positions[i + 1].time)
			{
				positionIndex = i;
				break;
			}
		}
	}
	if (positionIndex == ~0u)
		position = glm::vec3(0.0f);
	else
		position = InterpolatePosition(positionIndex, time);
}

void TransformTrack::SampleRotation(glm::fquat& rotation, float time) const
{
	uint32_t rotationIndex = ~0u;
	if (rotations.size() > 0)
	{
		rotationIndex = 0;
		for (uint32_t i = 0; i < rotations.size() - 1; ++i)
		{
			if (time <= rotations[i + 1].time)
			{
				rotationIndex = i;
				break;
			}
		}
	}

	if(rotationIndex == ~0u)
		rotation = glm::fquat(1.0f, 0.0f, 0.0f, 0.0f);
	else
		rotation = InterpolateRotation(rotationIndex, time);
}

void TransformTrack::SampleScaling(glm::vec3& scale, float time) const
{
	uint32_t scalingIndex = ~0u;
	if (scaling.size() > 0)
	{
		scalingIndex = 0;
		for (uint32_t i = 0; i < scaling.size() - 1; ++i)
		{
			if (time <= scaling[i + 1].time) {
				scalingIndex = i;
				break;
			}
		}
	}

	if (scalingIndex == ~0u)
		scale = glm::vec3(1.0f);
	else
		scale = InterpolateScale(scalingIndex, time);
}


Skeleton::Skeleton()
{
}

Skeleton::Skeleton(const Pose& rest, const Pose& bind, const std::vector<std::string>& names)
{
}

void Skeleton::Set(const Pose& rest, const Pose& bind, const std::vector<std::string>& names)
{
}

Pose::Pose(const Pose& pose)
{
	*this = pose;
}

Pose& Pose::operator=(const Pose& p)
{
	if (&p == this) {
		return *this;
	}
	if (mParents.size() != p.mParents.size()) {
		mParents.resize(p.mParents.size());
	}
	if (mJoints.size() != p.mJoints.size()) {
		mJoints.resize(p.mJoints.size());
	}
	if (mParents.size() != 0) {
		memcpy(&mParents[0], &p.mParents[0],
			sizeof(int) * mParents.size());
	}
	if (mJoints.size() != 0) {
		memcpy(&mJoints[0], &p.mJoints[0],
			sizeof(TransformComponent) * mJoints.size());
	}
	return *this;
}

float AnimationClip::Sample(Pose& pose, float time) const
{
	if (GetDuration() == 0.0f)
		return 0.0f;
	time = AdjustTimeToFitRange(time);
	
	uint32_t size = (uint32_t)mTracks.size();
	for (uint32_t i = 0; i < size; ++i)
	{
		TransformComponent& local = pose.GetLocalTransform(i);
		mTracks[i].Sample(local, time);
	}
	return time;
}

float AnimationClip::AdjustTimeToFitRange(float time) const
{
	if (mLooping)
	{
		float duration = mEndTime - mStartTime;
		if (duration <= 0.0f) return 0.0f;
		time = std::fmodf(time - mStartTime, duration);
		if (time < 0.0f)
			time += mStartTime;
	}
	else {
		if (time < mStartTime)
			time = mStartTime;
		if (time > mEndTime)
			time = mEndTime;
	}
	return time;
}

