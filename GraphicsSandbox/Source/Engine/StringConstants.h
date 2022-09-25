#pragma once

namespace StringConstants
{
	// Shaders
	constexpr const char* MAIN_VERT_PATH = "Assets/SPIRV/main.vert.spv";
	constexpr const char* MAIN_FRAG_PATH = "Assets/SPIRV/main.frag.spv";

	constexpr const char* CUBEMAP_VERT_PATH = "Assets/SPIRV/cubemap.vert.spv";
	constexpr const char* CUBEMAP_FRAG_PATH = "Assets/SPIRV/cubemap.frag.spv";

	constexpr const char* COPY_VERT_PATH = "Assets/SPIRV/copy.vert.spv";
	constexpr const char* COPY_FRAG_PATH = "Assets/SPIRV/copy.frag.spv";

	// EnvironmentMap
	constexpr const char* HDRI_CONVERTER_COMP_PATH = "Assets/SPIRV/hdri_converter.comp.spv";
	constexpr const char* IRRADIANCE_COMP_PATH = "Assets/SPIRV/irradiance.comp.spv";
	constexpr const char* PREFILTER_COMP_PATH = "Assets/SPIRV/prefilter_env.comp.spv";
	constexpr const char* BRDF_LUT_COMP_PATH = "Assets/SPIRV/brdf_lut.comp.spv";

	// FX
	constexpr const char* BLOOM_UPSAMPLE_COMP_PATH = "Assets/SPIRV/upsample.comp.spv";
	constexpr const char* BLOOM_DOWNSAMPLE_COMP_PATH = "Assets/SPIRV/downsample.comp.spv";
	constexpr const char* BLOOM_COMPOSITE_COMP_PATH = "Assets/SPIRV/bloom-final.comp.spv";

	// HDRI
	constexpr const char* HDRI_PATH = "Assets/EnvironmentMap/sunset.hdr";



}