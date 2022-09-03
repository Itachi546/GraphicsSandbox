#pragma once

#include "GraphicsDevice.h"
#include "Timer.h"

#include <string>
#include <vector>

using RangeId = std::size_t;

namespace Profiler
{
	struct RangeData
	{
		std::string name;
		Timer cpuTimer;
		float time = 0.0f;
		float sampleTime = 0.0f;
		uint32_t sampleCount = 0;

		uint32_t gpuBegin = ~0u;
		uint32_t gpuEnd = ~0u;
		uint32_t id = 0;
		// Use as flag if the renderPass is disabled
		// Used by UI to remove the profiling info from UI
		bool inUse = true;

		bool IsCpuRange() {
			return gpuBegin == ~0u;
		}
	};

	void BeginFrameGPU(gfx::CommandList* commandList);

	RangeId StartRangeCPU(const char* name);
	void EndRangeCPU(RangeId id);

	RangeId StartRangeGPU(gfx::CommandList* commandList, const char* name);
	void EndRangeGPU(gfx::CommandList* commandList, RangeId rangeId);

	void EndFrame();

	void GetEntries(std::vector<RangeData>& out);
};
