#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0) uniform samplerCube uEnvMap;
layout(binding = 1, rgba16f) writeonly uniform imageCube  uIrradianceTexture;

#extension GL_GOOGLE_include_directive : require
#include "cubemap.glsl"

layout(push_constant) uniform PushConstants
{
  float cubemapSize;
};

const float PI = 3.141593;
const float PI2 = 6.283185;
const float PIH = 1.570796;
void main()
{
   ivec3 uv = ivec3(gl_GlobalInvocationID.xyz);
   vec3 p = uvToXYZ(uv, vec2(cubemapSize));

   // Generate Coordinate system
   vec3 normal = normalize(p);
   vec3 right = normalize(cross(vec3(0.0f, 1.0f, 0.0f), normal));
   vec3 up = normalize(cross(normal, right));

   const int nStep = 100;
   const float dT = PIH / nStep;
   const float dP = PI2 / nStep;

   int nSample = 0;
   vec3 irradiance = vec3(0.0f);
   for(float phi = 0; phi <= PI2; phi += dP)
   {
       const float cosPhi =	cos(phi);
	   const float sinPhi =	sin(phi);
	   for(float theta = 0;	theta <= PIH; theta += dT)
	   {
    	   const float cosTheta	= cos(theta);
		   const float sinTheta	= sin(theta);
		   vec3	sphereCoord	= vec3(cosPhi *	sinTheta, sinPhi * sinTheta, cosTheta);
		   vec3	dir	= sphereCoord.x * right + sphereCoord.y * up + sphereCoord.z * normal;
		   irradiance += texture(uEnvMap, dir).rgb	* cosTheta * sinTheta;
		   nSample ++;
	  }
	}

   irradiance = PI * irradiance * (1.0f / float(nSample));
   imageStore(uIrradianceTexture, uv, vec4(irradiance, 1.0f));
}


