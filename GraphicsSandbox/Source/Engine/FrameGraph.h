#pragma once

#include "GraphicsDevice.h"
#include "ResourcePool.h"

#include <stdint.h>
#include <string>
#include <unordered_map>

namespace gfx
{
	using FrameGraphHandle = uint32_t;
	struct FrameGraphNodeHandle {
		FrameGraphHandle index;
	};

	struct FrameGraphResourceHandle {
		FrameGraphHandle index;
	};

	enum class FrameGraphResourceType
	{
		Invalid,
		Buffer,
		Texture,
		Attachment,
		Reference
	};
	
	struct FrameGraphResourceInfo {

		union {
			struct {
				std::size_t size;
				Usage usage;
				BufferHandle buffer;
			} buffer;

			struct {
				uint32_t width;
				uint32_t height;
				uint32_t depth;

				Format format;
				Usage usage;
				TextureHandle texture;
			} texture;
		};
	};

	struct FrameGraphResourceCreation {
		FrameGraphResourceType type;
		FrameGraphResourceInfo info;
		std::string name;
	};

	struct FrameGraphResource {
		int refCount;
		std::string name;
		FrameGraphResourceType type;
		FrameGraphResourceInfo info;

		// Reference to the node that produces the resources, later 
		// used to define the edge of framegraph
		FrameGraphNodeHandle producer;
		FrameGraphResourceHandle outputHandle;
	};

	struct FrameGraphNodeCreation
	{
		bool enabled = true;
		std::string name;
		std::vector<FrameGraphResourceCreation> inputs;
		std::vector<FrameGraphResourceCreation> outputs;
	};

	struct FrameGraphNode
	{
		bool enabled;
		std::string name;
		int refCount;

		RenderPassHandle renderPass;
		FramebufferHandle framebuffer;
		std::vector<FrameGraphNodeHandle> edges;

		std::vector<FrameGraphResourceHandle> inputs;
		std::vector<FrameGraphResourceHandle> outputs;
	};


	struct FrameGraphBuilder {
		void Init(gfx::GraphicsDevice* device);
		void Shutdown();

		FrameGraphNodeHandle CreateNode(const FrameGraphNodeCreation& creation);
		FrameGraphResourceHandle CreateNodeInput(const FrameGraphResourceCreation& creation);
		FrameGraphResourceHandle CreateNodeOutput(const FrameGraphResourceCreation& creation, FrameGraphNodeHandle producer);

		gfx::GraphicsDevice* device;
		static const uint32_t kMaxRenderpass = 128;
		static const uint32_t kMaxFramebuffer = 128;
		static const uint32_t kMaxNodes = 128;
		static const uint32_t kMaxResourceCount = 1024;

		ResourcePool<FrameGraphNode> nodePools;
		ResourcePool<FrameGraphResource> resourcePools;

		std::unordered_map<std::string, uint32_t> nodeCache;
		std::unordered_map<std::string, uint32_t> resourceCache;
	};

	struct FrameGraph
	{
		void Init(FrameGraphBuilder* builder);
		void Parse(std::string filename);
		void Compile();
		void Shutdown();

		FrameGraphBuilder* builder;
		std::vector<FrameGraphNodeHandle> nodeHandles;
		std::string name;

	private:
		//void ComputeEdges(FrameGraphNode* node, uint32_t index);
		void TopologicalSort(std::vector<FrameGraphNodeHandle> inputs, std::vector<FrameGraphNodeHandle>& output);
	};

}