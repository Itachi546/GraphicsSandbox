#pragma once

#include "../Engine/GlmIncludes.h"
#include <array>

inline uint32_t ConvertFloat3ToU32Color(const glm::vec3& color) {
	uint32_t r = glm::clamp(static_cast<uint32_t>(color.x * 255), 0u, 255u);
	uint32_t g = glm::clamp(static_cast<uint32_t>(color.y * 255), 0u, 255u);
	uint32_t b = glm::clamp(static_cast<uint32_t>(color.z * 255), 0u, 255u);
	return (b | g << 8 | r << 16);
}

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
	Plane() = default;

	Plane(const glm::vec4& abcd)
	{
		normal = glm::vec3(abcd);
		float invLen = 1.0f / glm::length(normal);
		normal = normal * invLen;
		distance = abcd.w * invLen;
	}
	// Equation of Plane is
	// Ax + By + Cz + d = 0
	// d = -Ax -By -Cz
	// d = -dot(n, p);
	// Points are taken in anticlockwise order
	void CreateFromPoints(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2)
	{
		normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
		distance = -glm::dot(normal, p0);
	}

	// Distance is calculated as
	// A(x - x1) + B(y - y1) + C(z - z1) + D = 0
	// Ax + By + Cz -(Ax1 + By1 + Cz1) + D = 0
	// -d - dot(p, n) + D = 0
	// D = dot(p, n) + d;
	float DistanceToPoint(glm::vec3 p)
	{
		return dot(p, normal) + distance;
	}

	glm::vec3 normal;
	float distance;
};

struct Frustum
{
public:
	//Points order NTL, NTR, NBR, NBL, FTL, FTR, FBR, FBL
	void CreateFromPoints(glm::vec3* points);

	bool Intersect(const BoundingBox& aabb);
	std::array<Plane, 6> planes;

	void GetPlanes(std::array<glm::vec4, 6>& outPlanes) {
		for (uint32_t i = 0; i < 6; ++i) {
			const Plane& plane = planes[i];
			outPlanes[i] = glm::vec4(plane.normal, plane.distance);
		}
	}

	enum PLANE
	{
		Left = 0, 
		Right,
		Top,
		Bottom,
		Near, 
		Far
	};
};

namespace MathUtils
{
	inline float Rand01()
	{
		return rand() / float(RAND_MAX);
	}

	// learnopengl.com/Advanced-Lighting/Normal-Mapping
	inline void GenerateTangentSpaceDirection(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
		                               const glm::vec2& uv0, const glm::vec2& uv1, glm::vec2& uv2, 
		                               glm::vec3& out_tangent, glm::vec3& out_bitangent)
	{
		glm::vec3 e1 = p1 - p0;
		glm::vec3 e2 = p2 - p1;

		glm::vec2 duv1 = uv1 - uv0;
		glm::vec2 duv2 = uv2 - uv1;

		float f = 1.0f / (duv1.x * duv2.y - duv1.y * duv2.x);

		out_tangent.x = f * (duv2.y * e1.x - duv1.y * e2.x);
		out_tangent.y = f * (duv2.y * e1.y - duv1.y * e2.y);
		out_tangent.z = f * (duv2.y * e1.z - duv1.y * e2.z);

		out_bitangent.x = f * (-duv2.x * e1.x + duv1.x * e2.x);
		out_bitangent.y = f * (-duv2.x * e1.y + duv1.x * e2.y);
		out_bitangent.z = f * (-duv2.x * e1.z + duv1.x * e2.z);
	}
}
