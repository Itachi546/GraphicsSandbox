#pragma once

#include "Logger.h"

#include <stdint.h>
#include <vector>

template<typename T>
struct ResourcePool
{
	/*
	* Initialize the resource pool with given poolSize and 
	* resourceSize
	* poolSize: Maximum number of resource count
	*/
	void Initialize(uint32_t poolSize)
	{
		this->poolSize = poolSize;
		freeIndices.resize(poolSize);
		resourceMemory.resize(poolSize);
		for (uint32_t i = 0; i < poolSize; ++i)
			freeIndices[i] = poolSize - i - 1;

		usedIndices = 0;
	}

	/*
	* Shutdown resource pool releasing all
	* allocated resources
	*/
	void Shutdown() {
		if (usedIndices > 0) {
			Logger::Warn("Resource pool has unfreed resources.\n");
			resourceMemory.clear();
			freeIndices.clear();
		}
	}

	/*
	* Get the index of free resources from 
	* the free indices list
	*/
	uint32_t ObtainResource() {

		if (freeIndices.size() > 0)
		{
			uint32_t freeIndex = freeIndices.back();
			freeIndices.pop_back();
			usedIndices++;
			return freeIndex;
		}

		Logger::Error("Resource space unavailable");
		return gfx::K_INVALID_RESOURCE_HANDLE;
	}

	/*
	* Free the underlying resources
	*/
	void ReleaseResource(uint32_t handle)
	{
		auto found = std::find(freeIndices.begin(), freeIndices.end(), handle);
		if (found == freeIndices.end())
		{
			freeIndices.push_back(handle);
			usedIndices--;
		}
		else {
			Logger::Warn("Releasing already destroyed resource");
		}
	}

	/*
	* Access the underlying pointer
	* to the resource
	*/
	T* AccessResource(uint32_t handle) {
		if (handle != gfx::K_INVALID_RESOURCE_HANDLE)
			return &resourceMemory[handle];
		return nullptr;
	}


	// Default PoolSize
	uint32_t poolSize = 16;

	// Memory to store resources
	std::vector<T> resourceMemory;

	// List of free indices
	std::vector<uint32_t> freeIndices;

	// Count of used indices
	uint32_t usedIndices;
};


