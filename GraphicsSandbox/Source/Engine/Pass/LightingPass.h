#pragma once

#include "../FrameGraph.h"
#include "../GlmIncludes.h"

class Scene;
class Renderer;

namespace gfx {
	struct LightingPass : public FrameGraphPass
	{
		LightingPass(RenderPassHandle renderPass, Renderer* renderer);

		void AddUI() override;

		void Render(CommandList* commandList, Scene* scene) override;

		void Shutdown() override;

		PipelineHandle pipeline;
		Renderer* renderer;

		struct LightingData {
			uint32_t nLight;
			uint32_t irradianceMap;
			uint32_t prefilterEnvMap;
			uint32_t brdfLUT;

			uint32_t positionBuffer;
			uint32_t normalBuffer;
			uint32_t pbrBuffer;
			uint32_t colorBuffer;

			glm::vec3 cameraPosition;
			float exposure;
		} lightingInfo;
	};

}

