#version 450

#extension GL_GOOGLE_include_directive: require
#include "utils.glsl"

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 brightColor;

layout(binding = 2) uniform samplerCube uCubemap;

layout(location = 0) in vec3 uv;

layout(push_constant) uniform PushConstants {
  vec3 cameraPosition;
  float bloomThreshold;
  float height;
  float radius;
};

// From: https://www.shadertoy.com/view/4tsBD7
float diskIntersectWithBackFaceCulling(vec3 ro, vec3 rd, vec3 c, vec3 n, float r)
{

	float d = dot(rd, n);

	if (d > 0.0) { return 1e6; }

	vec3 o = ro - c;
	float t = -dot(n, o) / d;
	vec3 q = o + rd * t;

	return (dot(q, q) < r * r) ? t : 1e6;

}

// From: https://www.iquilezles.org/www/articles/intersectors/intersectors.htm
float sphereIntersect(vec3 ro, vec3 rd, vec3 ce, float ra) {

	vec3 oc = ro - ce;
	float b = dot(oc, rd);
	float c = dot(oc, oc) - ra * ra;
	float h = b * b - c;

	if (h < 0.0) { return -1.0; }

	h = sqrt(h);

	return -b + h;

}
vec3 project(vec3 worldPos, float height, float radius) {

	vec3 p = normalize(worldPos);
	vec3 camPos = cameraPosition;
	camPos.y -= height;

	float intersection = sphereIntersect(camPos, p, vec3(0.0), radius);
	if (intersection > 0.0) {

		vec3 h = vec3(0.0, -height, 0.0);
		float intersection2 = diskIntersectWithBackFaceCulling(camPos, p, h, vec3(0.0, 1.0, 0.0), radius);
		p = (camPos + min(intersection, intersection2) * p) / radius;

	}
	else {

		p = vec3(0.0, 1.0, 0.0);

	}
	return p;
}
/*
vec3 project(vec3 p, float height, float radius) {
	p = normalize(p);
	vec3 camPos = cameraPosition - vec3(0.0, height, 0.0);

	float intersection = sphereIntersect(camPos, p, vec3(0.0), radius);
	if (intersection > 0.0) {
		float intersection2 = diskIntersectWithBackFaceCulling(camPos, p, vec3(0.0, -height, 0.0), vec3(0.0f, 1.0f, 0.0f), radius);
		return (camPos + min(intersection, intersection2) * p) / radius;
	}
	else return vec3(0.0, 1.0, 0.0);
}
*/

void main()
{
	vec3 projectedPos = project(uv, height, radius);
	vec3 col = texture(uCubemap, projectedPos).rgb;

	fragColor = vec4(col, 1.0f);
	if (rgb2Luma(col) > bloomThreshold)
		brightColor = fragColor;
	else
		brightColor = vec4(0.0f);
}