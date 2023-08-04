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

	void AddLine(const glm::vec3& start, const glm::vec3& end, uint32_t color = 0xffffffff);
	void AddAABB(const glm::vec3& min, const glm::vec3& max, uint32_t color = 0xff0000ff);
	void AddSphere(const glm::vec3& p, float radius, uint32_t color = 0xff0000ff);

	void AddQuadPrimitive(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, uint32_t color = 0xffffffff);

	/*
	* Points should be in given order
    * NTL, NTR, NBR, NBL, FTL, FTR, FBR, FBL
	*/
	void AddFrustum(const glm::vec3* points, uint32_t count = 8, uint32_t color = 0xff0000);

	void Draw(gfx::CommandList* commandList, glm::mat4 VP, const glm::vec3& camPos);

	void Shutdown();

}