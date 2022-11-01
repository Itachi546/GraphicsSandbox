#include "Animation.h"

namespace TrackHelpers
{
	// Hermite helpers
	inline float AdjustHermiteResult(float f) {
		return f;
	}

	inline glm::vec3 AdjustHermiteResult(const glm::vec3& v) {
		return v;
	}

	inline glm::quat AdjustHermiteResult(const glm::quat& q) {
		return glm::normalize(q);
	}

	inline void Neighborhood(const float& a, float& b) { }
	inline void Neighborhood(const glm::vec3& a, glm::vec3& b) { }
	inline void Neighborhood(const glm::quat& a, glm::quat& b) {
		if (glm::dot(a, b) < 0) {
			b = -b;
		}
	}
}

template<typename T, int N>
T Track<T, N>::Hermite(const T& p1, const T& s1, const T& _p2, const T& s2, float t) {
	float tt = t * t;
	float ttt = tt * t;

	T p2 = _p2;
	TrackHelpers::Neighborhood(p1, p2);

	float h1 = 2.0f * ttt - 3.0f * tt + 1.0f;
	float h2 = -2.0f * ttt + 3.0f * tt;
	float h3 = ttt - 2.0f * tt + t;
	float h4 = ttt - tt;

	T result = p1 * h1 + p2 * h2 + s1 * h3 + s2 * h4;
	return TrackHelpers::AdjustHermiteResult(result);
}

template<typename T, int N>
Track<T, N>::Track()
{
	mInterpolation = Interpolation::Linear;
}

template<typename T, int N>
float Track<T, N>::GetStartTime()
{
	return mFrames[0].mTime;
}

template<typename T, int N>
float Track<T, N>::GetEndTime()
{
	return mFrames[mFrames.size() - 1].mTime;
}

template<typename T, int N>
T Track<T, N>::Sample(float time, bool looping)
{
	if (mInterpolation == Interpolation::Constant)
		return SampleConstant(time, looping);
	else if (mInterpolation == Interpolation::Linear)
		return SampleLinear(time, looping);
	else
		return SampleCubic(time, looping);
}

template<typename T, int N>
Frame<N>& Track<T, N>::operator[](unsigned int index)
{
	return mFrames[index];
}

template<typename T, int N>
T Track<T, N>::SampleConstant(float time, bool looping)
{
	int frame = FrameIndex(time, looping);
	if (frame < 0 || frame >= (int)mFrames.size())
		return T();
	return Cast(&mFrames[frame].mValue[0]);
}

template<typename T, int N>
T Track<T, N>::SampleLinear(float time, bool looping)
{
	int thisFrame = FrameIndex(time, looping);
	if (thisFrame < 0 || thisFrame >= mFrames.size() - 1)
		return T();

	int nextFrame = thisFrame + 1;

	float trackTime = AdjustTimeToFitTrack(time, looping);
	float thisTime = mFrames[thisFrame].mTime;
	float frameDelta = mFrames[nextFrame].mTime - thisTime;

	if (frameDelta <= 0.0f)
		return T();

	float t = (trackTime - thisTime) / frameDelta;

	T start = Cast(&mFrames[thisFrame].mValue[0]);
	T end = Cast(&mFrames[nextFrame].mValue[0]);
	return Interpolator::Lerp(start, end, t);
}

template<typename T, int N>
T Track<T, N>::SampleCubic(float time, bool looping)
{
	int thisFrame = FrameIndex(time, looping);
	if (thisFrame < 0 || thisFrame >= mFrames.size() - 1)
		return T();

	int nextFrame = thisFrame + 1;

	float trackTime = AdjustTimeToFitTrack(time, looping);
	float thisTime = mFrames[thisFrame].mTime;
	float frameDelta = mFrames[nextFrame].mTime - thisTime;

	if (frameDelta <= 0.0f)
		return T();

	float t = (trackTime - thisTime) / frameDelta;
	std::size_t fltSize = sizeof(float);

	T point1 = Cast(&mFrames[thisFrame].mValue[0]);
	T slope1;
	memcpy(&slope1, mFrames[thisFrame].mOut, N * fltSize);
	slope1 = slope1 * frameDelta;

	T point2 = Cast(&mFrames[nextFrame].mValue[0]);
	T slope2;
	memcpy(&slope2, mFrames[nextFrame].mIn, N * fltSize);
	slope2 = slope1 * frameDelta;
	return Hermite(point1, slope1, point2, slope2, t);
}

template<typename T, int N>
int Track<T, N>::FrameIndex(float time, bool looping)
{
	unsigned int size = (unsigned int)mFrames.size();
	if (size <= 1)
		return -1;

	if (looping)
	{
		// Convert the time to time within the start time and endtime
		float startTime = mFrames[0].mTime;
		float endTime = mFrames[size - 1].mTime;
		float duration = endTime - startTime;
		time = fmodf(time - startTime, duration);

		if (time < 0.0f)
			time += duration;
		time += startTime;
	}
	else {
		if (time <= mFrames[0].mTime)
			return 0;
		if (time >= mFrames[size - 2].mTime)
			return (int)size - 2;
	}

	for (int i = size - 1; i >= 0; --i)
	{
		if (time >= mFrames[i].mTime)
			return i;
	}

	return -1;
}

template<typename T, int N>
float Track<T, N>::AdjustTimeToFitTrack(float time, bool loop)
{
	unsigned int size = (unsigned int)mFrames.size();
	// if frame is lt or eq to 1 it is invalid track
	if (size <= 1)
		return 0.0f;

	float startTime = mFrames[0].mTime;
	float endTime = mFrames[size - 1].mTime;

	float duration = endTime - startTime;
	if (duration <= 0.0f)
		return 0.0f;

	if (loop)
	{
		// Ajust time
		time = fmodf(time - startTime, duration);
		if (time <= 0.0f)
			time += duration;
		time += startTime;
	}
	else {
		if (time <= mFrames[0].mTime)
			time = startTime;
		if (time >= mFrames[size - 1].mTime)
			time = endTime;
	}

	return time;
}



template<>
float Track<float, 1>::Cast(float* value)
{
	return value[0];
}

template<>
glm::vec3 Track<glm::vec3, 3>::Cast(float* value)
{
	return glm::vec3(value[0], value[1], value[2]);
}

template<>
glm::fquat Track<glm::fquat, 4>::Cast(float* value)
{
	// glm takes in form w,x,y,z
	return glm::fquat(value[3], value[0], value[1], value[2]);
}