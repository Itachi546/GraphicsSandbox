#pragma once

#include <unordered_map>
#include <fstream>

struct ShaderPathInfo {
	std::vector<std::string> shaders;
	std::vector<std::string> meshShaders;
};

namespace StringConstants {
	// HDRI
	constexpr const char* HDRI_PATH = "Assets/EnvironmentMap/daytime.hdr";
}

namespace ShaderPath {
	void LoadStringPath(const std::string filename = "GraphicsSandbox/Shaders/shaders.json");
	ShaderPathInfo* get(const std::string& name);
}