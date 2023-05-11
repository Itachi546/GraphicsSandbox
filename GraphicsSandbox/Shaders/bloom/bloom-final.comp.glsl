#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0, rgba16f) uniform image2D uHdrTexture;
layout(binding = 1, rgba16f) readonly uniform image2D uBlurTexture;

layout(push_constant) uniform PushConstants {
  float bloomStrength;
};

void main()
{
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
    vec3 hdrColor  = imageLoad(uHdrTexture, uv).rgb;
    vec3 blurColor = imageLoad(uBlurTexture, uv / 2).rgb;

    vec3 col = mix(hdrColor, blurColor, bloomStrength);
    //vec3 col = hdrColor + blurColor;
    imageStore(uHdrTexture, uv, vec4(col, 1.0f));
}