#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_ARB_shader_draw_parameters: require

#include "mesh.h"

layout(binding = 0) readonly uniform Globals
{
   mat4 P;
   mat4 V;
   mat4 VP;
} globals;

layout(binding = 1) readonly buffer Vertices 
{
   Vertex aVertices[];
};

layout(binding = 2) readonly buffer PerObject
{
   PerObjectData aPerObjectData[];
};

layout(location = 0) out vec3 normal;

void main()
{
   Vertex vertex = aVertices[gl_VertexIndex]; 

   /*
     gl_DrawIDARB is used only when multiDrawIndirect is used
   */
   PerObjectData perObjectData = aPerObjectData[gl_DrawIDARB];

   vec3 position = vec3(vertex.px, vertex.py, vertex.pz);
   normal = vec3(vertex.nx, vertex.ny, vertex.nz);

   gl_Position = globals.VP * perObjectData.worldMatrix * vec4(position, 1.0);

}