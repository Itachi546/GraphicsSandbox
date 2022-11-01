#pragma once

#include "../Engine/GlmIncludes.h"
#include <cstdint>

namespace TransformGizmo
{
	enum class Operation {
		Translate,
		Rotate,
		Scale
	};

	glm::vec2 ComputeScreenSpacePos(const glm::vec3& p);
	void AddLine(glm::vec2 start, glm::vec2 end);
	void BeginFrame();
	void Manipulate(const glm::mat4& view, const glm::mat4& projection, Operation operation, glm::mat4& matrix);
};