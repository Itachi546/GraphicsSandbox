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
	static T EaseIn(T a, T b, T t)
	{
		T t2 = t * t;
		return a * (1.0f - t2) + t2 * b;
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

	template<typename T>
	static T HermiteSpline(T p1, T s1, T p2, T s2, T t)
	{
		float tt = t * t;
		float ttt = tt * t;

		float h1 = 2.0f * ttt - 3.0f * tt + 1.0f;
		float h2 = -2.0f * ttt + 3.0f * tt;
		float h3 = ttt - 2.0f * tt + t;
		float h4 = ttt - tt;

		return p1 * h1 + p2 * h2 + s1 * h3 + s2 * h4;

	}

	static glm::fquat Lerp(const glm::fquat& a, const glm::fquat& b, float t)
	{
		return glm::normalize(glm::fquat(
			Lerp(a.w, b.w, t),
			Lerp(a.x, b.x, t),
			Lerp(a.y, b.y, t),
			Lerp(a.z, b.z, t)
		));
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
