#include "FrameGraph.h"
#include "../External/json.hpp"

#include <stack>
#include <fstream>

namespace gfx {
	void FrameGraphBuilder::Init(GraphicsDevice* device_)
	{
		device = device_;

		nodePools.Initialize(kMaxNodes);
		resourcePools.Initialize(kMaxResourceCount);
	}

	void FrameGraphBuilder::Shutdown()
	{
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
		return FrameGraphResourceType::Invalid;
	}

	static Format GetTextureFormat(std::string inputFormat)
	{
		if (inputFormat == "B8G8R8A8_UNORM")
			return Format::B8G8R8A8_UNORM;
		else if (inputFormat == "R16G16B16A16_SFLOAT")
			return Format::R16B16G16A16_SFLOAT;
		else if (inputFormat == "D32_SFLOAT")
			return Format::D32_SFLOAT;
		else
			assert(0);
		return Format::FormatCount;
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
			FrameGraphNodeCreation node;
			node.name = pass.value("name", "");
			node.enabled = pass.value("enabled", true);

			json inputs = pass["inputs"];
			json outputs = pass["outputs"];

			node.inputs.resize(inputs.size());
			node.outputs.resize(outputs.size());

			// Parse Inputs for the pass
			Logger::Info("Pass: " + node.name);

			Logger::Info("Parsing Pass Inputs");
			for (std::size_t j = 0; j < inputs.size(); ++j)
			{
				json passInput = inputs[j];
				FrameGraphResourceCreation& resource = node.inputs[j];
				std::string name = passInput.value("name", "");
				resource.name = name;

				std::string resourceType = passInput.value("type", "");
				resource.type = GetResourceTypeFromString(resourceType);
				Logger::Info("Name: " + name + " Type: " + resourceType);
			}

			Logger::Info("Parsing Pass Outputs");
			for (std::size_t j = 0; j < outputs.size(); ++j)
			{
				json passOutput = outputs[j];
				FrameGraphResourceCreation& resource = node.outputs[j];

				std::string name = passOutput.value("name", "");;
				resource.name = name;

				std::string resourceType = passOutput.value("type", "");
				resource.type = GetResourceTypeFromString(resourceType);
				Logger::Info("Name: " + name + " Type: " + resourceType);
				switch (resource.type)
				{
				case FrameGraphResourceType::Attachment:
				case FrameGraphResourceType::Texture:
				{
					json resolution = passOutput["resolution"];
					resource.info.texture.width = resolution[0];
					resource.info.texture.height = resolution[1];
					resource.info.texture.format = GetTextureFormat(passOutput["format"]);
					break;
				}
				case FrameGraphResourceType::Buffer:
					assert(0);
					break;
				}
			}
		}
	}

	void FrameGraph::Compile()
	{
		/*
		// Initialize edges
		for (std::size_t i = 0; nodeHandles.size(); ++i)
		{
			FrameGraphNodeCreation* node = nodePools.AccessResource(nodeHandles[i].index);
			node->edges.clear();
		}

		for (std::size_t i = 0; i < nodeHandles.size(); ++i)
		{
			FrameGraphNodeCreation* node = nodePools.AccessResource(nodeHandles[i].index);
			if (!node->enabled) continue;
			ComputeEdges(node, i);
		}
		*/
	}

	void FrameGraph::Shutdown()
	{
	}

	/*
	void FrameGraph::ComputeEdges(FrameGraphNode* node, uint32_t index)
	{

	}
	*/
	void FrameGraph::TopologicalSort(std::vector<FrameGraphNodeHandle> inputs, std::vector<FrameGraphNodeHandle>& output)
	{
		std::stack<std::size_t> visitedNodes;
	}
}