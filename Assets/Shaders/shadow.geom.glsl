#version 450

layout(triangles, invocations = 5) in;
layout(triangle_strip, max_vertices = 3) out;

#extension GL_GOOGLE_include_directive : require
#include "shadow.glsl"

layout(binding = 2, std140) uniform CascadeInfo
{
   Cascade cascades[5];
};

void main()
{ 
   for(int i = 0; i < gl_in.length(); ++i)
   {
      gl_Layer = gl_InvocationID;
      gl_Position = cascades[gl_InvocationID].VP * gl_in[i].gl_Position;
      EmitVertex();
   }
   EndPrimitive();
}