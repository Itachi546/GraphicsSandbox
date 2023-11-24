#version 450

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

layout(binding = 0) uniform sampler2D uInputTexture;

layout(push_constant) uniform TextureInfo {
  uint depth;
};

void main()
{
    vec3 col = vec3(1.0f);
    if(depth == 1)
      col = vec3(texture(uInputTexture, vec2(uv.x, 1.0f - uv.y)).r);
    else
      col = texture(uInputTexture, vec2(uv.x, 1.0f - uv.y)).rgb;

    fragColor = vec4(col, 1.0f);
}