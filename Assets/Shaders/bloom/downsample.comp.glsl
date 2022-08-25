#version 460
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D uInputTexture;
layout(binding = 1, rgba16f) uniform image2D uOutputTexture;

layout(push_constant, std430) uniform PushConstant
{
  float width;
  float height;
};
// Downsample
//https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom
void main()
{
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = vec2(coord.x / width, coord.y / height);

	float dx = 1.0f	/ width;
    float dy = 1.0f / height;

    // Center pixel
    vec4 a = texture(uInputTexture, uv);

    // Neighbouring 4 sample
    vec4 b = texture(uInputTexture, uv + vec2(-dx, -dy));
    vec4 c = texture(uInputTexture, uv + vec2( dx, -dy));
    vec4 d = texture(uInputTexture, uv + vec2(-dx,  dy));
    vec4 e = texture(uInputTexture, uv + vec2( dx,  dy));

    // Outer sample
    vec4 f = texture(uInputTexture, uv + vec2(-dx * 2.0f, -dy * 2.0f));
    vec4 g = texture(uInputTexture, uv + vec2( 0.0f     , -dy * 2.0f));
    vec4 h = texture(uInputTexture, uv + vec2( dx * 2.0f, -dy * 2.0f));

    vec4 i = texture(uInputTexture, uv + vec2(-dx * 2.0f, 0.0f));
    vec4 j = texture(uInputTexture, uv + vec2( dx * 2.0f, 0.0f));

    vec4 k = texture(uInputTexture, uv + vec2(-dx * 2.0f, dy * 2.0f));
    vec4 l = texture(uInputTexture, uv + vec2( 0.0f,      dy * 2.0f));
    vec4 m = texture(uInputTexture, uv + vec2( dx * 2.0f, dy * 2.0f));

    vec4 col = a * 0.125f;
    col += (f + h + k + m) * 0.03125f;
    col += (g + j + i + l) * 0.0625f; 
    col += (b + c + d + e) * 0.125f;

    imageStore(uOutputTexture, coord, vec4(col.rgb, 1.0f));
}