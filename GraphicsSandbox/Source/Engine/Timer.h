#pragma once

#include "Platform.h"

#include <chrono>

struct Timer
{
	inline void record()
	{
		mTimeStamp = std::chrono::high_resolution_clock::now();
	}

	inline double elapsedSeconds()
	{
		auto timeStamp2 = std::chrono::high_resolution_clock::now();
		return std::chrono::duration_cast<std::chrono::duration<double>>(timeStamp2 - mTimeStamp).count();
	}

	inline double elapsedMilliseconds()
	{
		return elapsedSeconds() * 1000.0;
	}

	inline double elapsed() {
		return elapsedMilliseconds();
	}

private:
	std::chrono::high_resolution_clock::time_point mTimeStamp = std::chrono::high_resolution_clock::now();
};