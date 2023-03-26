#pragma once

#include "GlmIncludes.h"

namespace gfx {
	struct RenderPassHandle;
	struct CommandList;
}

namespace DebugDraw
{
	void Initialize(gfx::RenderPassHandle renderPass);

	void SetEnable(bool state);

	void AddLine(const glm::vec3& start, const glm::vec3& end, uint32_t color = 0xffffff);
	void AddAABB(const glm::vec3& min, const glm::vec3& max, uint32_t color = 0xff0000);

	/*
	* Points should be in given order
    * NTL, NTR, NBR, NBL, FTL, FTR, FBR, FBL
	*/
	void AddFrustum(const glm::vec3* points, uint32_t count = 8, uint32_t color = 0xff0000);

	void Draw(gfx::CommandList* commandList, glm::mat4 VP);

	void Free();

}