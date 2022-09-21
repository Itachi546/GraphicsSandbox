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
		glm::vec3 vmin{ FLT_MAX };
		glm::vec3 vmax{ -FLT_MAX };

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

struct Plane
{
	Plane() { }

	// From plane equation
	Plane(float a, float b, float c, float d)
	{
		float mag = std::sqrtf(a * a + b * b + c * c + d * d);

		normal = glm::vec3(a, b, c);
		distance = d;
		// Normalize 
		if (abs(mag) > 0.000001f)
		{
			normal /= mag;
			distance /= mag;
		}
	}

	Plane(glm::vec4 abcd) : Plane(abcd.x, abcd.y, abcd.z, abcd.w)
	{ 
	}

	Plane(const glm::vec3& normal, float distance) : normal(normal), distance(distance) {}

	// Point in clockwise order
	// Ax + By + Cz + d = 0
	// where normal = (A, B, C)
	// d = -Ax - By - Cz
	// d = -dot(N, P) where P - point in the plane
	Plane(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2)
	{
		GenerateFromPoints(p0, p1, p2);
	}

	void GenerateFromPoints(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2)
	{
		glm::vec3 e0 = p1 - p0;
		glm::vec3 e1 = p2 - p0;

		normal = glm::normalize(glm::cross(e1, e0));
		distance = -glm::dot(normal, p0);
	}

	float GetDistance(const glm::vec3& p) const
	{
		return glm::dot(p, normal) + distance;
	}

	glm::vec3 normal;
	float distance;
};

class Frustum {

public:
	Frustum() {}

	bool Intersect(const BoundingBox& boundingBox);

	/*
	* Points should be in given order
	* NTL, NTR,	NBL, NBR, FTL, FTR,	FBL, FBR
	*/
	void GenerateFromPoints(glm::vec3* points, uint32_t count = 8);

	Plane frustumPlanes[6] = {};

	enum PointLocation {
		NTL = 0,
		NTR,
		NBL,
		NBR,

		FTL,
		FTR,
		FBL,
		FBR
	};

	enum PlaneLocation
	{
		Near = 0,
		Far,
		Left,
		Right,
		Top,
		Bottom
	};
};

namespace MathUtils
{
	inline float Rand01()
	{
		return rand() / float(RAND_MAX);
	}
}
