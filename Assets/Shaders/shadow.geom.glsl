#version 450

layout(triangles, invocations = 6) in;
layout(triangle_strip, max_vertices = 18) out;

layout(binding = 0) uniform CascadeInfo
{
   int cascadeCount;
   mat4 lightVP[6];
};

void main()
{ 
   for(int i = 0; i < cascadeCount; ++i)
   {
      gl_Layer = gl_InvocationID;
      gl_Position = lightVP[i] * gl_in[i].gl_Position;
      EmitVertex();
   }
   EndPrimitive();
}