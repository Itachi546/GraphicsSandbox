#pragma once

#pragma once

#include "../FrameGraph.h"
#include "../GlmIncludes.h"

class Scene;
class Renderer;
struct DrawData;

namespace gfx {
	struct TransparentPass : public FrameGraphPass
	{
		TransparentPass(Renderer* renderer);

		void Initialize(RenderPassHandle renderPass) override;

		void Render(CommandList* commandList, Scene* scene) override;

		void Shutdown() override;

		PipelineHandle pipeline;
		Renderer* renderer;
		DescriptorInfo descriptorInfos[5];

	private:
		struct PushConstantData {
			uint32_t nLight;
			uint32_t irradianceMap;
			uint32_t prefilterEnvMap;
			uint32_t brdfLUT;

			glm::vec3 cameraPosition;
			float exposure;
			float globalAO;
		} mPushConstantData;

	};
}

