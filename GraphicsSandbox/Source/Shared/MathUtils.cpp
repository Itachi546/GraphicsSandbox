#include "MathUtils.h"
/***************************************************************************************************************************/
/*
* Checks whether the box is inside or outside the frustum
* It doesn't check whether it is partially inside/outside.
* Partially inside is also considered inside.
*/
bool Frustum::Intersect(const BoundingBox& boundingBox)
{
	const glm::vec3 min = boundingBox.min;
	const glm::vec3 max = boundingBox.max;
	/*
	* Find the minimum and maximum vertex in the direction of the 
	* normal vector.(positive far vertex(P) and negative far vertex(N))
	* If the maximum vertex is in the opposite side of normal
	* then the box is out of frustum.
	*/
	for (int i = 0; i < 6; ++i)
	{
		glm::vec3 p = min;
		const Plane& plane = frustumPlanes[i];
		const glm::vec3& n = plane.normal;
		if (n.x >= 0)
			p.x = max.x;
		if (n.y >= 0)
			p.y = max.y;
		if (n.z >= 0)
			p.z = max.z;

		if (plane.GetDistance(p) < 0.0f)
			return false;
	}

	return true;
}

void Frustum::GenerateFromPoints(glm::vec3* frustumPoints, uint32_t count)
{
	// Generate Frustum Planes
	frustumPlanes[Top].GenerateFromPoints(frustumPoints[FTL], frustumPoints[NTL], frustumPoints[NTR]);
	frustumPlanes[Bottom].GenerateFromPoints(frustumPoints[FBR], frustumPoints[NBR], frustumPoints[NBL]);
	frustumPlanes[Left].GenerateFromPoints(frustumPoints[FBL], frustumPoints[NBL], frustumPoints[NTL]);
	frustumPlanes[Right].GenerateFromPoints(frustumPoints[FBR], frustumPoints[NTR], frustumPoints[NBR]);
	frustumPlanes[Near].GenerateFromPoints(frustumPoints[NBR], frustumPoints[NTR], frustumPoints[NTL]);
	frustumPlanes[Far].GenerateFromPoints(frustumPoints[FBL], frustumPoints[FTL], frustumPoints[FTR]);
}
