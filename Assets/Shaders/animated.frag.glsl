#version 450

layout(location = 0) in VS_OUT 
{
   vec3 normal;
   vec3 tangent;
   vec3 bitangent;
   vec3 worldPos;
   vec3 lsPos;
   vec3 viewDir;
   vec2 uv;
   flat uint matId;
   vec3 vertexColor;
}fs_in;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 brightColor;

void main()
{
    fragColor = vec4(fs_in.vertexColor, 1.0);
    brightColor = vec4(0.0);
}