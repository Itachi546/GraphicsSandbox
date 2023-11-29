#include "FrameGraph.h"
#include "../External/json.hpp"

#include <stack>
#include <fstream>

namespace gfx {
	void FrameGraphBuilder::Init(GraphicsDevice* device_)
	{
		device = device_;

		nodePools.Initialize(kMaxNodes, "FrameGraphNodes");
		resourcePools.Initialize(kMaxResourceCount, "FrameGraphResources");
	}

	void FrameGraphBuilder::Shutdown()
	{
		for (auto& [key, value] : resourceCache)
		{
			FrameGraphResource* resource = resourcePools.AccessResource(value);
			if (resource->type == FrameGraphResourceType::Attachment || resource->type == FrameGraphResourceType::StorageImage)
				device->Destroy(resource->info.texture.texture);
			else if (resource->type == FrameGraphResourceType::Buffer)
				device->Destroy(resource->info.buffer.buffer);
		}

		for (auto& [key, value] : nodeCache)
		{
			FrameGraphNode* node = nodePools.AccessResource(value);

			for (uint32_t i = 0; i < node->inputs.size(); ++i)
				resourcePools.ReleaseResource(node->inputs[i].index);
			
			for (uint32_t i = 0; i < node->outputs.size(); ++i)
				resourcePools.ReleaseResource(node->outputs[i].index);

			if(node->renderer)
				node->renderer->Shutdown();
			nodePools.ReleaseResource(value);
		}

		resourceCache.clear();
		nodeCache.clear();
		nodePools.Shutdown();
		resourcePools.Shutdown();
	}

	FrameGraphNodeHandle FrameGraphBuilder::CreateNode(const FrameGraphNodeCreation& creation)
	{
		FrameGraphNodeHandle nodeHandle = { K_INVALID_RESOURCE_HANDLE };
		nodeHandle.index = nodePools.ObtainResource();
		if (nodeHandle.index == K_INVALID_RESOURCE_HANDLE)
			return nodeHandle;

		FrameGraphNode* node = nodePools.AccessResource(nodeHandle.index);
		node->name = creation.name;
		node->enabled = creation.enabled;
		node->inputs.resize(creation.inputs.size());
		node->outputs.resize(creation.outputs.size());
		node->framebuffer = INVALID_FRAMEBUFFER;
		node->renderPass = INVALID_RENDERPASS;
		node->compute = creation.compute;

		nodeCache.insert(std::make_pair(node->name, nodeHandle.index));
		
		for (uint32_t i = 0; i < creation.outputs.size(); ++i)
		{
			const FrameGraphResourceCreation& outputCreation = creation.outputs[i];
			node->outputs[i] = CreateNodeOutput(outputCreation, nodeHandle);
		}


		for (uint32_t i = 0; i < creation.inputs.size(); ++i)
		{
			const FrameGraphResourceCreation& inputCreation = creation.inputs[i];
			node->inputs[i] = CreateNodeInput(inputCreation);
		}

		return nodeHandle;
	}

	FrameGraphResourceHandle FrameGraphBuilder::CreateNodeInput(const FrameGraphResourceCreation& creation)
	{
		FrameGraphResourceHandle resourceHandle = { K_INVALID_RESOURCE_HANDLE };
		resourceHandle.index = resourcePools.ObtainResource();
		if (resourceHandle.index == K_INVALID_RESOURCE_HANDLE)
			return resourceHandle;

		FrameGraphResource* resource = resourcePools.AccessResource(resourceHandle.index);
		resource->info = {};
		resource->name = creation.name;
		resource->type = creation.type;
		resource->producer = { K_INVALID_RESOURCE_HANDLE };
		resource->outputHandle = { K_INVALID_RESOURCE_HANDLE };
		resource->refCount = 0;
		return resourceHandle;
	}

	FrameGraphResourceHandle FrameGraphBuilder::CreateNodeOutput(const FrameGraphResourceCreation& creation, FrameGraphNodeHandle producer)
	{
		FrameGraphResourceHandle resourceHandle = { K_INVALID_RESOURCE_HANDLE };
		resourceHandle.index = resourcePools.ObtainResource();
		if (resourceHandle.index == K_INVALID_RESOURCE_HANDLE)
			return resourceHandle;

		FrameGraphResource* resource = resourcePools.AccessResource(resourceHandle.index);
		resource->name = creation.name;
		resource->type = creation.type;

		if (resource->type != FrameGraphResourceType::Reference)
		{
			resource->producer = producer;
			resource->info = creation.info;
			resource->outputHandle = resourceHandle;
			resource->refCount = 0;
			resourceCache.insert(std::make_pair(resource->name, resourceHandle.index));
		}

		return resourceHandle;
	}

	void FrameGraph::Init(FrameGraphBuilder* builder_)
	{
		builder = builder_;
	}

	static FrameGraphResourceType GetResourceTypeFromString(std::string inputType)
	{
		if (inputType == "attachment")
			return FrameGraphResourceType::Attachment;
		else if (inputType == "texture")
			return FrameGraphResourceType::Texture;
		else if (inputType == "buffer")
			return FrameGraphResourceType::Buffer;
		else if (inputType == "reference")
			return FrameGraphResourceType::Reference;
		else if (inputType == "storage-image")
			return FrameGraphResourceType::StorageImage;
		return FrameGraphResourceType::Invalid;
	}

	static void GetTextureFormatAndAspect(const std::string& inputFormat, Format& format, ImageAspect& aspect)
	{
		if (inputFormat == "B8G8R8A8_UNORM")
		{
			format = Format::B8G8R8A8_UNORM;
			aspect = ImageAspect::Color;
		}
		else if (inputFormat == "R16G16B16A16_SFLOAT")
		{
			format = Format::R16B16G16A16_SFLOAT;
			aspect = ImageAspect::Color;
		}
		else if (inputFormat == "R32G32B32_SFLOAT") {
			format = Format::R32G32B32_SFLOAT;
			aspect = ImageAspect::Color;
		}
		else if (inputFormat == "R32G32B32A32_SFLOAT") {
			format = Format::R32G32B32A32_SFLOAT;
			aspect = ImageAspect::Color;
		}
		else if (inputFormat == "D32_SFLOAT")
		{
			format = Format::D32_SFLOAT;
			aspect = ImageAspect::Depth;
		}
		else if (inputFormat == "R16_SFLOAT") {
			format = Format::R16_SFLOAT;
			aspect = ImageAspect::Color;
		}
		else {
			assert(!"Undefined input format");
			format = Format::FormatCount;
			aspect = ImageAspect::Color;
		}
	}

	static RenderPassOperation GetTextureLoadOp(const std::string& loadOp)
	{
		if (loadOp == "VK_ATTACHMENT_LOAD_OP_CLEAR")
			return RenderPassOperation::Clear;
		else if (loadOp == "VK_ATTACHMENT_LOAD_OP_LOAD")
			return RenderPassOperation::Load;
		return RenderPassOperation::DontCare;
	}

	void FrameGraph::Parse(std::string filename)
	{
		using json = nlohmann::json;
		std::ifstream jsonFile(filename);
		if (!jsonFile)
		{
			Logger::Error("Failed to load frameGraph: " + filename);
			return;
		}

		Logger::Info("Parsing FrameGraph: " + filename);

		// Start parsing frame graph
		json data = json::parse(jsonFile);

		this->name = data.value("name", "");
		Logger::Info("FrameGraph Name: " + name);

		json passes = data["passes"];
		Logger::Info("Total passes: " + std::to_string(passes.size()));

		for (std::size_t i = 0; i < passes.size(); ++i)
		{
			json pass = passes[i];
			FrameGraphNodeCreation nodeCreation;
			nodeCreation.name = pass.value("name", "");
			bool enabled = pass.value("enabled", true);
			nodeCreation.compute = pass.value("type", "") == "compute" ? true : false;

			if (!enabled) continue;
			nodeCreation.enabled = enabled;

			json inputs = pass["inputs"];
			json outputs = pass["outputs"];

			nodeCreation.inputs.resize(inputs.size());
			nodeCreation.outputs.resize(outputs.size());

			// Parse Inputs for the pass
			for (std::size_t j = 0; j < inputs.size(); ++j)
			{
				json passInput = inputs[j];
				FrameGraphResourceCreation& resource = nodeCreation.inputs[j];
				std::string name = passInput.value("name", "");
				resource.name = name;

				std::string resourceType = passInput.value("type", "");
				resource.type = GetResourceTypeFromString(resourceType);
			}

			for (std::size_t j = 0; j < outputs.size(); ++j)
			{
				json passOutput = outputs[j];
				FrameGraphResourceCreation& resource = nodeCreation.outputs[j];

				std::string name = passOutput.value("name", "");;
				resource.name = name;

				std::string resourceType = passOutput.value("type", "");
				resource.type = GetResourceTypeFromString(resourceType);
				switch (resource.type)
				{
				case FrameGraphResourceType::Attachment:
				case FrameGraphResourceType::Texture:
				case FrameGraphResourceType::StorageImage:
				{
					json resolution = passOutput["resolution"];
					resource.info.texture.width = resolution[0];
					resource.info.texture.height = resolution[1];
					resource.info.texture.depth = 1;
					GetTextureFormatAndAspect(passOutput["format"], resource.info.texture.format, resource.info.texture.imageAspect);
					resource.info.texture.op = GetTextureLoadOp(passOutput["op"]);
					resource.info.texture.layerCount = passOutput.value("layers", 1);
					break;
				}
				case FrameGraphResourceType::Buffer:
					resource.info.buffer.buffer = gfx::INVALID_BUFFER;
					break;
				}
			}
			nodeHandles.push_back(builder->CreateNode(nodeCreation));
		}
	}

	void FrameGraph::onResize(uint32_t width, uint32_t height)
	{
		for (uint32_t i = 0; i < nodeHandles.size(); ++i)
		{
			FrameGraphNode* node = builder->AccessNode(nodeHandles[i]);
			if (!node->enabled) continue;
			node->renderer->OnResize(gfx::GetDevice(), width, height);
		}
	}

	void FrameGraph::Compile()
	{
		// Initialize edges
		for (std::size_t i = 0; i < nodeHandles.size(); ++i)
		{
			FrameGraphNode* node = builder->AccessNode(nodeHandles[i]);
			node->edges.clear();
		}

		for (std::size_t i = 0; i < nodeHandles.size(); ++i)
		{
			FrameGraphNode* node = builder->AccessNode(nodeHandles[i]);
			if (!node->enabled) continue;
			ComputeEdges(node, (uint32_t)i);
		}

		nodeHandles = TopologicalSort(nodeHandles);

		// Allocate all the resources for framegraph
		// keep track of allocation, deallocation and free texture
		uint32_t resourceCount = builder->resourcePools.usedIndices;
		std::vector<FrameGraphNodeHandle> allocations(resourceCount, { K_INVALID_RESOURCE_HANDLE });

		// Iterate through all the nodes and increase the reference of the resource
		// each time they are taken as input
		for (std::size_t i = 0; i < nodeHandles.size(); ++i)
		{
			FrameGraphNode* node = builder->AccessNode(nodeHandles[i]);
			if (!node->enabled) continue;

			for (std::size_t j = 0; j < node->inputs.size(); ++j)
			{
				FrameGraphResource* input = builder->AccessResource(node->inputs[j]);
				FrameGraphResource* resource = builder->AccessResource(input->name);
				if(resource)
					resource->refCount++;
			}
		}

		for (std::size_t i = 0; i < nodeHandles.size(); ++i)
		{
			FrameGraphNode* node = builder->AccessNode(nodeHandles[i]);
			if (!node->enabled) continue;

			for (std::size_t j = 0; j < node->outputs.size(); ++j)
			{
				uint32_t resourceIndex = node->outputs[j].index;
				FrameGraphResourceHandle resourceHandle = node->outputs[j];
				FrameGraphResource* resource = builder->AccessResource(resourceHandle);
				if (allocations[resourceIndex].index == K_INVALID_RESOURCE_HANDLE)
				{
					allocations[resourceIndex] = nodeHandles[i];

					if (resource->type == FrameGraphResourceType::Attachment || 
						resource->type == FrameGraphResourceType::StorageImage)
					{
						GPUTextureDesc textureDesc;
						textureDesc.format = resource->info.texture.format;
						textureDesc.width = resource->info.texture.width;
						textureDesc.height = resource->info.texture.height;
						textureDesc.depth = resource->info.texture.depth;
						textureDesc.imageAspect = resource->info.texture.imageAspect;

						if(textureDesc.imageAspect == ImageAspect::Depth)
							textureDesc.bindFlag = BindFlag::DepthStencil | BindFlag::ShaderResource;
						else
							textureDesc.bindFlag = (resource->type == FrameGraphResourceType::Attachment ? BindFlag::RenderTarget : BindFlag::StorageImage) | BindFlag::ShaderResource;

						textureDesc.imageType = ImageType::I2D;
						if (resource->info.texture.layerCount > 1)
							textureDesc.imageViewType = gfx::ImageViewType::IV2DArray;

						textureDesc.bCreateSampler = true;
						textureDesc.bAddToBindless = true;
						textureDesc.mipLevels = 1;
						textureDesc.arrayLayers = resource->info.texture.layerCount;

						TextureHandle handle = builder->device->CreateTexture(&textureDesc);
						resource->info.texture.texture = handle;
					}
				}
			}
		}

		for (uint32_t i = 0; i < nodeHandles.size(); ++i)
		{
			FrameGraphNode* node = builder->AccessNode(nodeHandles[i]);
			if (!node->enabled || node->compute)
				continue;

			if (node->renderPass.handle = K_INVALID_RESOURCE_HANDLE && node->enabled)
				node->renderPass = CreateRenderPass(node);
			if (node->framebuffer.handle = K_INVALID_RESOURCE_HANDLE && node->enabled)
				node->framebuffer = CreateFramebuffer(node);
		}
	}

	void FrameGraph::Shutdown()
	{
		for (uint32_t i = 0; i < nodeHandles.size(); ++i)
		{
			FrameGraphNodeHandle handle = nodeHandles[i];
			FrameGraphNode* node = builder->AccessNode(handle);
			builder->device->Destroy(node->renderPass);
			builder->device->Destroy(node->framebuffer);
		}
	}

	void FrameGraph::ComputeEdges(FrameGraphNode* node, uint32_t index)
	{
		for (uint32_t i = 0; i < node->inputs.size(); ++i)
		{
			// Access the input resource of this node
			FrameGraphResource* resource = builder->AccessResource(node->inputs[i]);

			// Access the output resource from the global output cache
			FrameGraphResource* outputResource = builder->AccessResource(resource->name);

			if (resource == nullptr || outputResource == nullptr)
			{
				Logger::Warn("Requested resource is not produced by any other node");
				continue;
			}

			resource->producer = outputResource->producer;
			resource->info = outputResource->info;
			resource->outputHandle = outputResource->outputHandle;

			FrameGraphNode* parent = builder->AccessNode(outputResource->producer);
			parent->edges.push_back(nodeHandles[index]);
		}
	}

	std::vector<FrameGraphNodeHandle> FrameGraph::TopologicalSort(std::vector<FrameGraphNodeHandle> inputs)
	{
		/*
		* Randomly choose an unvisited node
		* Visit all the nodes in the depth first order
		* Push the node in final list on every recursive call
		*/
		std::vector<uint8_t> visitedNodes(inputs.size(), 0);
		std::stack<FrameGraphNodeHandle> stack;
		std::vector<FrameGraphNodeHandle> sortedNodes;

		const uint8_t kVisited = 1;
		const uint8_t kPushed = 2;

		for (uint32_t i = 0; i < inputs.size(); ++i)
		{
			FrameGraphNode* currentNode = builder->AccessNode(inputs[i]);
			if (!currentNode->enabled)
				continue;

			stack.push(inputs[i]);

			while (stack.size() > 0)
			{
				FrameGraphNodeHandle nodeHandle = stack.top();

				// If node is already pushed we skip it
				if (visitedNodes[nodeHandle.index] == kPushed)
				{
					stack.pop();
					continue;
				}

				// If node is visited already we pop it from stack and
				// add it to sorted node
				if (visitedNodes[nodeHandle.index] == kVisited)
				{
					visitedNodes[nodeHandle.index] = kPushed;
					sortedNodes.push_back(nodeHandle);
					stack.pop();
					continue;
				}

				// if it is processed for first time we mark it as visited
				visitedNodes[nodeHandle.index] = kVisited;

				// Push the edges to the stack
				FrameGraphNode* node = builder->AccessNode(nodeHandle);
				if (node->edges.size() == 0)
					continue;

				for (FrameGraphNodeHandle edges : node->edges)
				{
					if (visitedNodes[edges.index] != kVisited)
						stack.push(edges);
				}
			}
		}
		return std::vector<FrameGraphNodeHandle>(sortedNodes.rbegin(), sortedNodes.rend());
	}

	RenderPassHandle FrameGraph::CreateRenderPass(FrameGraphNode* node)
	{
		RenderPassDesc desc;
		std::vector<Attachment>& attachments = desc.colorAttachments;

		for (uint32_t i = 0; i < node->outputs.size(); ++i)
		{
			FrameGraphResource* output = builder->AccessResource(node->outputs[i]);
			FrameGraphResourceInfo& info = output->info;

			if (output->type == FrameGraphResourceType::Attachment)
			{
				Attachment attachment = {};
				attachment.operation = info.texture.op;
				attachment.format = info.texture.format;
				attachment.imageAspect = info.texture.imageAspect;

				if (output->info.texture.imageAspect == ImageAspect::Depth)
				{
					desc.hasDepthAttachment = true;
					desc.depthAttachment = attachment;
				}
				else
					attachments.push_back(attachment);
			}
		}

		for (uint32_t i = 0; i < node->inputs.size(); ++i) {
			FrameGraphResource* inputResource = builder->AccessResource(node->inputs[i]);

			FrameGraphResourceInfo& info = inputResource->info;
			if (inputResource->type == FrameGraphResourceType::Attachment) {

				if (info.texture.imageAspect == ImageAspect::Depth) {
					desc.hasDepthAttachment = true;
					desc.depthAttachment = Attachment{info.texture.format, RenderPassOperation::Load, info.texture.imageAspect};
				}
				else {
					attachments.emplace_back(Attachment{ info.texture.format, RenderPassOperation::Load, ImageAspect::Color });
				}
			}
		}

		return builder->device->CreateRenderPass(&desc);
	}

	FramebufferHandle FrameGraph::CreateFramebuffer(FrameGraphNode* node)
	{
		FramebufferDesc desc = {};
		desc.renderPass = node->renderPass;

		uint32_t width = 0;
		uint32_t height = 0;

		for (uint32_t r = 0; r < node->outputs.size(); ++r)
		{
			FrameGraphResource* output = builder->AccessResource(node->outputs[r]);
			FrameGraphResourceInfo info = output->info;

			if (output->type == FrameGraphResourceType::Buffer)
				continue;

			if (output->type == FrameGraphResourceType::Reference)
			{
				FrameGraphResource* refResource = builder->AccessResource(output->name);
				width = refResource->info.texture.width;
				height = refResource->info.texture.height;
				continue;
			}

			if (width == 0) width = info.texture.width;
			else assert(width == info.texture.width);

			if (height == 0) height = info.texture.height;
			else assert(height = info.texture.height);

			desc.layers = info.texture.layerCount;

			if (info.texture.imageAspect == ImageAspect::Depth)
			{
				desc.hasDepthStencilAttachment = true;
				desc.depthStencilAttachment = info.texture.texture;
			}
			else {
				desc.outputTextures.push_back(info.texture.texture);
			}
		}

		// Copy the texture handle to the input
		for (uint32_t r = 0; r < node->inputs.size(); ++r)
		{
			FrameGraphResource* input = builder->AccessResource(node->inputs[r]);
			if (input->type == FrameGraphResourceType::Buffer || input->type == FrameGraphResourceType::Reference)
				continue;

			FrameGraphResource* resource = builder->AccessResource(input->name);
			FrameGraphResourceInfo& info = input->info;

			input->info.texture.texture = resource->info.texture.texture;

			if (input->type == FrameGraphResourceType::Texture) continue;

			if (info.texture.imageAspect == ImageAspect::Depth)
			{
				desc.hasDepthStencilAttachment = true;
				desc.depthStencilAttachment = info.texture.texture;
			}
			else 
				desc.outputTextures.push_back(info.texture.texture);
		}

		assert(width != 0);
		assert(height != 0);

		desc.width = width;
		desc.height = height;
		return builder->device->CreateFramebuffer(&desc);
	}
}