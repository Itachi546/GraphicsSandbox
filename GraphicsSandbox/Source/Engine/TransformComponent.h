#pragma once

#include "GlmIncludes.h"

struct TransformComponent
{
	glm::vec3 position{ 0.0f, 0.0f, 0.0f };
	glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };
	glm::vec3 scale{ 1.0f, 1.0f, 1.0f };

	glm::mat4 defaultMatrix{ 1.0f };
	glm::mat4 localMatrix{ 1.0f };
	glm::mat4 worldMatrix{ 1.0f };

	bool dirty = true;
	void CalculateWorldMatrix()
	{
		if (dirty)
		{
			localMatrix = glm::translate(glm::mat4(1.0f), position) *
				glm::toMat4(rotation) *
				glm::scale(glm::mat4(1.0f), scale) * defaultMatrix;
			worldMatrix = localMatrix;
			dirty = false;
		}
	}

	glm::mat4 GetRotationMatrix()
	{
		return glm::toMat4(rotation);
	}
};

