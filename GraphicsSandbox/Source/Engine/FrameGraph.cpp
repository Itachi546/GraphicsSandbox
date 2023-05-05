#include "FrameGraph.h"
#include "../External/json.hpp"

#include <fstream>

void gfx::FrameGraph::Init(gfx::GraphicsDevice* device_)
{
	device = device_;
	nodes.Initialize(kMaxNodes);
}

static gfx::FrameGraphResourceType GetResourceTypeFromString(std::string inputType)
{
	if (inputType == "attachment")
		return gfx::FrameGraphResourceType::Attachment;
	else if (inputType == "texture")
		return gfx::FrameGraphResourceType::Texture;
	else if (inputType == "buffer")
		return gfx::FrameGraphResourceType::Buffer;
	else if (inputType == "reference")
		return gfx::FrameGraphResourceType::Reference;
	return gfx::FrameGraphResourceType::Invalid;
}

static gfx::Format GetTextureFormat(std::string inputFormat)
{
	if (inputFormat == "B8G8R8A8_UNORM")
		return gfx::Format::B8G8R8A8_UNORM;
	else if (inputFormat == "R16G16B16A16_SFLOAT")
		return gfx::Format::R16B16G16A16_SFLOAT;
	else if (inputFormat == "D32_SFLOAT")
		return gfx::Format::D32_SFLOAT;
	else
		assert(0);
	return gfx::Format::FormatCount;
}

void gfx::FrameGraph::Parse(std::string filename)
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
		FrameGraphNodeHandle handle = { nodes.ObtainResource() };
		FrameGraphNode* node = nodes.AccessResource(handle.handle);
		node->name = pass.value("name", "");

		json inputs = pass["inputs"];
		json outputs = pass["outputs"];

		node->inputs.resize(inputs.size());
		node->outputs.resize(outputs.size());

		// Parse Inputs for the pass
		Logger::Info("Pass: " + node->name);
		Logger::Info("Parsing Pass Inputs");
		for (std::size_t j = 0; j < inputs.size(); ++j)
		{
			json passInput = inputs[j];
			FrameGraphResource& resource = node->inputs[j];
			resource.name = passInput.value("name", "");

			std::string resourceType = passInput.value("type", "");
			resource.type = GetResourceTypeFromString(resourceType);
			Logger::Info("Name: " + resource.name + " Type: " + resourceType);
		}
		Logger::Info("Parsing Pass Outputs");
		for (std::size_t j = 0; j < outputs.size(); ++j)
		{
			json passOutput = outputs[j];
			FrameGraphResource& resource = node->outputs[j];
			resource.name = passOutput.value("name", "");

			std::string resourceType = passOutput.value("type", "");
			resource.type = GetResourceTypeFromString(resourceType);
			Logger::Info("Name: " + resource.name + " Type: " + resourceType);
			switch (resource.type)
			{
			case FrameGraphResourceType::Attachment:
			case FrameGraphResourceType::Texture:
			{
				json resolution = passOutput["resolution"];
				resource.width = resolution[0];
				resource.height = resolution[1];
				resource.format = GetTextureFormat(passOutput["format"]);
				break;
			}
			case FrameGraphResourceType::Buffer:
				assert(0);
				break;
			}
		}
	}
}

void gfx::FrameGraph::Compile()
{
}

void gfx::FrameGraph::Shutdown()
{
	nodes.Shutdown();
}
