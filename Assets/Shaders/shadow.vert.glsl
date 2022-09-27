#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_ARB_shader_draw_parameters: require

#include "mesh.glsl"

layout(binding = 1) readonly buffer Vertices 
{
   Vertex aVertices[];
};

layout(binding = 2) readonly buffer TransformData
{
   mat4 aTransformData[];
};

void main()
{
   Vertex vertex = aVertices[gl_VertexIndex]; 
   mat4 worldMatrix = aTransformData[gl_DrawIDARB];
   gl_Position = worldMatrix * vec4(vertex.px, vertex.py, vertex.pz, 1.0f);
}