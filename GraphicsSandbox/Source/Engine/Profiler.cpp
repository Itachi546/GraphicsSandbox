#include "Profiler.h"
#include "Utils.h"
#include <unordered_map>

#include <algorithm>

namespace Profiler
{
	bool initialized = false;
	std::unordered_map<std::size_t, RangeData> gRangeData;
	gfx::QueryPool gQueryPool;
	uint32_t queryIdx;

	// Used for sorting of the data when displaying later
	uint32_t id = 0;

	uint64_t queryResult[128];
	double gpuTimestampFrequency = 0.0;

	void BeginFrameGPU(gfx::CommandList* commandList)
	{
		queryIdx = 0;
		id = 0;
		gfx::GraphicsDevice* device = gfx::GetDevice();
		if (!initialized)
		{
			gpuTimestampFrequency = device->GetTimestampFrequency();
			device->CreateQueryPool(&gQueryPool, 128, gfx::QueryType::TimeStamp);
			initialized = true;
		}
		device->ResetQueryPool(commandList, &gQueryPool, 0, 128);
	}

	RangeId StartRangeCPU(const char* name)
	{
		RangeId rangeId = Utils::StringHash(name);
		gRangeData[rangeId].cpuTimer.record();
		gRangeData[rangeId].name = name;
		gRangeData[rangeId].id = ++id;
		gRangeData[rangeId].inUse = true;
		return rangeId;
	}

	void EndRangeCPU(RangeId id)
	{
		auto found = gRangeData.find(id);
		if (found != gRangeData.end())
		{
			auto& range = found->second;
			auto dt = (float)range.cpuTimer.elapsedMilliseconds();

			range.sampleTime += dt;
			if (found->second.sampleCount == 0)
				range.time = dt;

			found->second.sampleCount++;
			if (found->second.sampleCount > 100)
			{
				range.time = range.sampleTime / range.sampleCount;
				range.sampleCount = 0;
				range.sampleTime = 0.0f;
			}
		}
	}

	RangeId StartRangeGPU(gfx::CommandList* commandList, const char* name)
	{
		RangeId rangeId = Utils::StringHash(name);
		gRangeData[rangeId].name = name;
		gRangeData[rangeId].gpuBegin = queryIdx;
		gRangeData[rangeId].id = ++id;
		gRangeData[rangeId].inUse = true;

		gfx::GetDevice()->Query(commandList, &gQueryPool, queryIdx);
		queryIdx++;
		return rangeId;
	}

	void EndRangeGPU(gfx::CommandList* commandList, RangeId rangeId)
	{
		auto found = gRangeData.find(rangeId);
		if (found != gRangeData.end())
		{
			gRangeData[rangeId].gpuEnd = queryIdx;
			gfx::GetDevice()->Query(commandList, &gQueryPool, queryIdx);
			queryIdx++;
		}
	}

	void EndFrame()
	{
		if (queryIdx == 0)
			return;

		gfx::GetDevice()->ResolveQuery(&gQueryPool, 0, queryIdx, queryResult);

		for (auto& [k, v] : gRangeData)
		{
			v.inUse = false;
			if (v.IsCpuRange())
				continue;

			float dt = static_cast<float>((queryResult[v.gpuEnd] - queryResult[v.gpuBegin]) * gpuTimestampFrequency * 1e-6);
			v.sampleTime += dt;
			if (v.sampleCount == 0)
				v.time = dt;

			v.sampleCount++;
			if (v.sampleCount > 100)
			{
				v.time = v.sampleTime / v.sampleCount;
				v.sampleCount = 0;
				v.sampleTime = 0.0f;
			}

		}
	}

	void GetEntries(std::vector<RangeData>& out)
	{
		for (auto& [k, v] : gRangeData)
			out.push_back(v);

		std::sort(out.begin(), out.end(), [](const RangeData& lhs, const RangeData& rhs) {
			return lhs.id < rhs.id;
			});
	}
}