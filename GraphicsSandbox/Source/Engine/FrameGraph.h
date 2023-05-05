#pragma once

#include "GraphicsDevice.h"
#include "ResourcePool.h"

#include <stdint.h>
#include <string>
#include <map>

namespace gfx
{
	using FrameGraphHandle = uint32_t;
	struct FrameGraphNodeHandle {
		FrameGraphHandle handle;
	};

	enum class FrameGraphResourceType
	{
		Invalid,
		Buffer,
		Texture,
		Attachment,
		Reference
	};

	struct FrameGraphResource {
		FrameGraphResourceType type;
		uint32_t width;
		uint32_t height;
		uint32_t size;
		gfx::Format format;
		std::string name;
	};

	struct FrameGraphNode
	{
		std::string name;
		std::vector<FrameGraphResource> inputs;
		std::vector<FrameGraphResource> outputs;
	};

	struct FrameGraph
	{
		void Init(gfx::GraphicsDevice* device);
		void Parse(std::string filename);
		void Compile();
		void Shutdown();

		gfx::GraphicsDevice* device;
		ResourcePool<FrameGraphNode> nodes;
		std::string name;

		static const uint32_t kMaxRenderpass = 128;
		static const uint32_t kMaxFramebuffer = 128;
		static const uint32_t kMaxNodes = 128;
	};

}