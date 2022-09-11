#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_ARB_shader_draw_parameters: require

#include "mesh.glsl"

#define VERTEX_SHADER
#include "bindings.glsl"

layout(location = 0) out VS_OUT 
{
   vec3 normal;
   vec3 worldPos;
   vec3 viewDir;
   vec2 uv;
   flat uint matId;
}vs_out;

void main()
{
   Vertex vertex = aVertices[gl_VertexIndex]; 
   /*
     gl_DrawIDARB is used only when multiDrawIndirect is used
   */
   //PerObjectData perObjectData = aPerObjectData[gl_DrawIDARB];

   vec3 position = vec3(vertex.px, vertex.py, vertex.pz);
   mat4 worldMatrix = aTransformData[gl_DrawIDARB];

   vec4 wP = worldMatrix * vec4(position, 1.0);
   gl_Position = globals.VP * wP;

   // Calculate normal matrix
   // http://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/
   vs_out.worldPos	= wP.xyz;
   vs_out.uv = vec2(vertex.tu, vertex.tv);
   vs_out.normal    = mat3(transpose(inverse(worldMatrix))) * vec3(vertex.nx, vertex.ny, vertex.nz);
   vs_out.viewDir   = globals.cameraPosition - wP.xyz;
   vs_out.matId     = gl_DrawIDARB;
}