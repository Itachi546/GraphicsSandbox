#version 450

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

layout(binding = 0) uniform sampler2D uInputTexture;

void main()
{
    vec3 col = texture(uInputTexture, vec2(uv.x, 1.0 - uv.y)).rgb;
    fragColor = vec4(col, 1.0f);
}