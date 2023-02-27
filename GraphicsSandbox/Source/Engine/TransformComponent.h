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
	glm::mat4 CalculateWorldMatrix()
	{
		if (dirty)
		{
			localMatrix = glm::translate(glm::mat4(1.0f), position) *
				glm::toMat4(rotation) *
				glm::scale(glm::mat4(1.0f), scale) * defaultMatrix;
			worldMatrix = localMatrix;
			dirty = false;
		}
		return worldMatrix;
	}

	glm::mat4 GetRotationMatrix()
	{
		return glm::toMat4(rotation);
	}

	static TransformComponent Combine(TransformComponent a, TransformComponent b)
	{
		TransformComponent out = {};
		out.scale = a.scale * b.scale;
		out.rotation = a.rotation * b.rotation;

		out.position = a.rotation * (a.scale * b.position);
		out.position += a.position;
		out.dirty = true;
		return out;
	}

};

