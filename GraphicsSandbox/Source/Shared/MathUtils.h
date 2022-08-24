#pragma once

#include "../Engine/GlmIncludes.h"

struct BoundingBox
{
	glm::vec3 min;
	glm::vec3 max;

	BoundingBox() = default;

	BoundingBox(const glm::vec3& min, const glm::vec3& max) : min(min), max(max) {}

	BoundingBox(const glm::vec3* points, std::size_t numPoints)
	{
		glm::vec3 vmin{ std::numeric_limits<float>::max()};
		glm::vec3 vmax{ std::numeric_limits<float>::min()};

		for (std::size_t i = 0; i < numPoints; ++i)
		{
			vmin = glm::min(vmin, points[i]);
			vmax = glm::max(vmax, points[i]);
		}

		min = vmin;
		max = vmax;
	}

	glm::vec3 GetSize() const {
		return max - min;
	}

	glm::vec3 GetCenter() const {
		return (max + min) * 0.5f;
	}

	void Transform(const glm::mat4& transform)
	{
		glm::vec3 vmin{ std::numeric_limits<float>::max() };
		glm::vec3 vmax{ std::numeric_limits<float>::min() };

		glm::vec3 corners[] = {
			glm::vec3(min.x, min.y, min.z),
			glm::vec3(min.x, max.y, min.z),
			glm::vec3(min.x, min.y, max.z),
			glm::vec3(min.x, max.y, max.z),
			glm::vec3(max.x, min.y, min.z),
			glm::vec3(max.x, max.y, min.z),
			glm::vec3(max.x, min.y, max.z),
			glm::vec3(max.x, max.y, max.z),
		};

		for (auto& v : corners)
		{
			v = transform * glm::vec4(v, 1.0f);
			vmin = glm::min(v, vmin);
			vmax = glm::max(v, vmax);
		}

		min = vmin;
		max = vmax;
	}


};
