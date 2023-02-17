#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_ARB_shader_draw_parameters: require

#include "mesh.glsl"

layout(binding = 1) readonly buffer Vertices 
{
   AnimatedVertex aVertices[];
};

layout(push_constant) uniform PushConstants {
  mat4 modelMatrix;
};

void main()
{
   AnimatedVertex vertex = aVertices[gl_VertexIndex]; 
   gl_Position = modelMatrix * vec4(vertex.px, vertex.py, vertex.pz, 1.0f);
}