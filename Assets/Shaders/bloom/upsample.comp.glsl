#version 460
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D uInputTexture;
layout(binding = 1, rgba16f) uniform image2D uOutputTexture;

layout(push_constant, std430) uniform PushConstant
{
  float width;
  float height;
  float radius;
};

// Upsample
//https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom
void main()
{
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    vec2 invRes = 1.0f / vec2(width, height);
    vec2 uv = vec2(coord.x, coord.y) * invRes; 

    // Sample around the specified radius 
    float dx = radius * invRes.x;
    float dy = radius * invRes.y;

    vec4 a = texture(uInputTexture, vec2(uv.x - dx, uv.y + dy));
    vec4 b = texture(uInputTexture, vec2(uv.x,      uv.y + dy));
    vec4 c = texture(uInputTexture, vec2(uv.x + dx, uv.y + dy));

    vec4 d = texture(uInputTexture, vec2(uv.x - dx, uv.y));
    vec4 e = texture(uInputTexture, vec2(uv.x,      uv.y));
    vec4 f = texture(uInputTexture, vec2(uv.x + dx, uv.y));

    vec4 g = texture(uInputTexture, vec2(uv.x - dx, uv.y - dy));
    vec4 h = texture(uInputTexture, vec2(uv.x,      uv.y - dy));
    vec4 i = texture(uInputTexture, vec2(uv.x + dx, uv.y - dy));

    // 3x3 tent filter
    // 1    |1 2 1|
    // -- * |2 4 2|
    // 16   |1 2 1|
    vec4 col = imageLoad(uOutputTexture, coord);
    col += e * 4.0f;
    col += (b + d + f + h) * 2.0f;
    col += (a + c + g + i);
    col *= (1.0f / 16.0f);

    imageStore(uOutputTexture, coord, vec4(col.rgb, 1.0f));
}