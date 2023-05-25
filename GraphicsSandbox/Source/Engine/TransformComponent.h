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

	void SetRotationFomEuler(float x, float y, float z)
	{
		rotation = glm::vec3(x, y, z);
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

	static void From(const glm::mat4& matrix, TransformComponent& result)
	{
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(matrix, result.scale, result.rotation, result.position, skew, perspective);
	}

};

