#version 460
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : require

layout(binding = 0) uniform samplerCube uEnvMap;
layout(binding = 1, rgba16f) uniform imageCube uPrefilterMap;

layout(push_constant) uniform PushConstants
{
  float cubemapSize;
  float roughness;
};

#include "cubemap.glsl"
#include "pbr.glsl"

void main()
{
    ivec3 cubeCoord = ivec3(gl_GlobalInvocationID.xyz);

    vec3 N = normalize(uvToXYZ(cubeCoord, vec2(cubemapSize)));
    vec3 R = N;
    vec3 V = R;

    const uint SAMPLE_COUNT = 4096u;
    vec3 prefilteredColor = vec3(0.0f);
    float totalWeight = 0.0;

    for(uint i = 0; i < SAMPLE_COUNT; ++i)
    {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V,H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if(NdotL > 0.0)
        {
           prefilteredColor += texture(uEnvMap, L).rgb * NdotL;
           totalWeight += NdotL;
        }
    }

    prefilteredColor = prefilteredColor / totalWeight;
    imageStore(uPrefilterMap, cubeCoord, vec4(prefilteredColor, 1.0f));
}