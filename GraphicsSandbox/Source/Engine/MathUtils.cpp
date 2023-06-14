#include "MathUtils.h"

void Frustum::CreateFromPoints(glm::vec3* points)
{
	// Left
	planes[Plane::Left].CreateFromPoints(points[3], points[7], points[0]);

	// Right
	planes[Plane::Right].CreateFromPoints(points[1], points[6], points[2]);

	//Top
	planes[Plane::Top].CreateFromPoints(points[0], points[4], points[1]);

	// Bottom
	planes[Plane::Bottom].CreateFromPoints(points[3], points[2], points[7]);

	// Near
	planes[Plane::Near].CreateFromPoints(points[3], points[0], points[2]);

	//Far
	planes[Plane::Far].CreateFromPoints(points[7], points[6], points[4]);
}

bool Frustum::Intersect(const BoundingBox& aabb)
{
	const glm::vec3& min = aabb.min;
	const glm::vec3& max = aabb.max;

	for (int i = 0; i < 6; ++i)
	{
		glm::vec3 p = min;

		const glm::vec3& normal = planes[i].normal;
		if (normal.x >= 0.0f)
			p.x = max.x;
		if (normal.y >= 0.0f)
			p.y = max.y;
		if (normal.z >= 0.0f)
			p.z = max.z;

		if (planes[i].DistanceToPoint(p) < 0.0f)
			return false;
	}
	return true;
}