#version 450

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

layout(binding = 0) uniform sampler2D uInputTexture;

const float exposure = 1.8f;

vec3 ACESFilm(vec3 x)
{
   float a = 2.51f;
   float b = 0.03f;
   float c = 2.43f;
   float d = 0.59f;
   float e = 0.14f;
   return clamp((x*(a*x+b))/(x*(c*x+d)+e), vec3(0.0f), vec3(1.0f));
}
void main()
{
    vec3 col = texture(uInputTexture, vec2(uv.x, 1.0 - uv.y)).rgb;
    col = ACESFilm(col * exposure);
    col = pow(col, vec3(0.4545));
    fragColor = vec4(col, 1.0f);
}