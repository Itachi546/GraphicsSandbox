#pragma once

#include "../Interpolator.h"
#include <vector>

enum class Interpolation
{
	Constant,
	Linear,
	Cubic
};

template <unsigned int N>
class Frame {

public:
	float mValue[N];
	float mIn[N];
	float mOut[N];
	float mTime;
};

typedef Frame<1> ScalarFrame;
typedef Frame<3> VectorFrame;
typedef Frame<4> QuaternionFrame;

template<typename T, int N>
class Track
{
public:
	Track();

	// Resize the mFrames container
	void Resize(unsigned int size);

	// Get Total Size of Track
	unsigned int Size() { return mFrames.size(); }

	// Get Interpolation 
	Interpolation GetInterpolation() { return mInterpolation; }

	// Set Interpolation
	void SetInterpolation(Interpolation interpolation) { mInterpolation = interpolation; }

	// Get StartTime
	float GetStartTime();
	
	// Get EndTime
	float GetEndTime();

	// Sample the track
	T Sample(float time, bool looping);

	// Get a specific frame from the track
	Frame<N>& operator[](unsigned int index);


protected:
	std::vector<Frame<N>> mFrames;
	Interpolation mInterpolation;

	T SampleConstant(float time, bool looping);

	T SampleLinear(float time, bool looping);

	T SampleCubic(float time, bool looping);

	// Take time as input and return the frame immediately before the time
	int FrameIndex(float time, bool looping);

	float AdjustTimeToFitTrack(float time, bool loop);

	T Cast(float* value);

	T Hermite(const T& p1, const T& s1, const T& p2, const T& s2, float t);
};

typedef Track<float, 1> ScalarTrack;
typedef Track<float, 3> VectorTrack;
typedef Track<float, 4> QuaternionTrack;

