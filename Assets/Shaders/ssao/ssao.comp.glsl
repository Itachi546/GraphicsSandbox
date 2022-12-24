#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D uDepthTexture;
layout(binding = 1) uniform sampler2D uNoiseTexture;
layout(binding = 1, rgba16f) uniform image2D uOutputTexture;

layout(push_constant) uniform PushConstants {
  vec4 sampleKernel[64];
};

void main()
{
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
}