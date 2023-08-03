#version 450

layout(triangles, invocations = 5) in;
layout(triangle_strip, max_vertices = 3) out;

struct Cascade {
   mat4 VP;
   vec4 splitDistance;
};

layout(binding = 3, std140) uniform CascadeInfo
{
   Cascade cascades[5];
   vec4 shadowDims;
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