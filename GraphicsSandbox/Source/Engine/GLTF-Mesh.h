#pragma once

#include "ECS.h"

#include <string>
#define TINYGLTF_IMPLEMENTATION
//#define TINYGLTF_NO_EXTERNAL_IMAGE
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION
#include <tiny_gltf.h>

namespace gltfMesh {
	bool loadFile(const std::string& filename, tinygltf::Model* out)
	{
		tinygltf::TinyGLTF loader;
		std::string err{};
		std::string warn{};

		std::string extension = filename.substr(filename.find_last_of('.') + 1, filename.length());
		bool res = false;
		if (extension == "gltf")
			res = loader.LoadASCIIFromFile(out, &err, &warn, filename);
		else
			res = loader.LoadBinaryFromFile(out, &err, &warn, filename);

		if (!warn.empty())
			Logger::Warn(warn);
		if (!err.empty())
			Logger::Error(err);

		if (!res)
		{
			Logger::Warn("Failed to load models: " + filename);
			return false;
		}
		return true;
	}

	uint8_t* getBufferPtr(tinygltf::Model* model, const tinygltf::Accessor& accessor) {
		tinygltf::BufferView& bufferView = model->bufferViews[accessor.bufferView];
		return model->buffers[bufferView.buffer].data.data() + accessor.byteOffset + bufferView.byteOffset;
	}
};