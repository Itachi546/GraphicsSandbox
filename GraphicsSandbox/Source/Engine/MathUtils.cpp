#include "MathUtils.h"

void Frustum::CreateFromPoints(glm::vec3* points)
{
	// Left
	planes[PLANE::Left].CreateFromPoints(points[3], points[7], points[0]);

	// Right
	planes[PLANE::Right].CreateFromPoints(points[1], points[6], points[2]);

	//Top
	planes[PLANE::Top].CreateFromPoints(points[0], points[4], points[1]);

	// Bottom
	planes[PLANE::Bottom].CreateFromPoints(points[3], points[2], points[7]);

	// Near
	planes[PLANE::Near].CreateFromPoints(points[3], points[0], points[2]);

	//Far
	planes[PLANE::Far].CreateFromPoints(points[7], points[6], points[4]);
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