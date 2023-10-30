#include "StringConstants.h"

#include "../External/json.hpp"
#include "Logger.h"

#include <cassert>

namespace ShaderPath {

	std::unordered_map<std::string, ShaderPathInfo> gShaderMapping;
	using json = nlohmann::json;

	static void parseShader(json& shaders, const std::string& rootPath, std::vector<std::string>& outShaderPath) {
		std::size_t nShader = shaders.size();
		if (nShader == 0) return;
		outShaderPath.resize(nShader);

		for (std::size_t j = 0; j < nShader; ++j)
			outShaderPath[j] = rootPath + std::string(shaders[j]);
	}

	void LoadStringPath(const std::string filename) {

		std::ifstream jsonFile(filename);
		if (!jsonFile) {
			Logger::Error("Failed to load shader json file: " + filename);
			return;
		}

		Logger::Info("Parsing Shader Paths: " + filename);

		json data = json::parse(jsonFile);
		std::string rootPath = data.value("root", "");

		json shaders = data["shaders"];

		for (std::size_t i = 0; i < shaders.size(); ++i) {
			json shader = shaders[i];

			std::string name = shader.value("name", "");
			if (name.length() == 0) {
				Logger::Warn("Empty shader name");
				continue;
			}
			ShaderPathInfo pathInfo = {};

			json shaderPath = shader["shaders"];
			parseShader(shaderPath, rootPath, pathInfo.shaders);

			json meshShaderPath = shader["meshShaders"];
			parseShader(meshShaderPath, rootPath, pathInfo.meshShaders);

			gShaderMapping[name] = pathInfo;
		}
	}

	ShaderPathInfo* ShaderPath::get(const std::string& name)
	{
		auto found = gShaderMapping.find(name);
		if (found != gShaderMapping.end())
			return &found->second;
		else
			Logger::Error("Cannot find shaderPath: " + name);
		return nullptr;
	}
}