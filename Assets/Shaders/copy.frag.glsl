#version 450

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

layout(binding = 0) uniform sampler2D uInputTexture;

const float exposure = 1.0f;
void main()
{
    vec3 col = texture(uInputTexture, vec2(uv.x, 1.0 - uv.y)).rgb;
    col = 1.0f - exp(-col * exposure);
    col = pow(col, vec3(0.4545));
    fragColor = vec4(col, 1.0f);
}