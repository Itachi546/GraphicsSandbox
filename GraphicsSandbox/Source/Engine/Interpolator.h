#pragma once

#include "GlmIncludes.h"

class Interpolator
{
public:
	template<typename T>
	static T Lerp(T a, T b, T t)
	{
		return a * (1.0f - t) + t * b;
	}

	template<typename T>
	static T BezierSpline(T p0, T p1, T c0, T c1, T t)
	{
		T t0 = 1 - t;
		return p0 * t0 * t0 * t0 +
			3 * c0 * t * t0 * t0 +
			3 * c1 * t * t * t0 +
			p1 * t * t * t;
	}

	static glm::vec3 Lerp(const glm::vec3& a, const glm::vec3& b, float t)
	{
		return glm::vec3(
			Lerp(a.x, b.x, t),
			Lerp(a.y, b.y, t),
			Lerp(a.z, b.z, t));
	}

	static glm::vec3 BezierSpline(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& c0, const glm::vec3& c1, float t)
	{
		return glm::vec3(
			BezierSpline(p0.x, p1.x, c0.x, c1.x, t),
			BezierSpline(p0.y, p1.y, c0.y, c1.y, t),
			BezierSpline(p0.z, p1.z, c0.z, c1.z, t));
	}
};