#pragma once

#include "GlmIncludes.h"

namespace gfx {
	struct RenderPass;
	struct CommandList;
}

namespace DebugDraw
{
	void Initialize(gfx::RenderPass* renderPass);

	void SetEnable(bool state);

	void AddLine(const glm::vec3& start, const glm::vec3& end, uint32_t color = 0xffffffff);

	void Draw(gfx::CommandList* commandList, glm::mat4 VP);

	void Free();

}