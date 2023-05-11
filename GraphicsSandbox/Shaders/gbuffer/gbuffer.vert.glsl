#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_ARB_shader_draw_parameters: require

#include "../includes/mesh.glsl"

#define VERTEX_SHADER
#include "../includes/bindings.glsl"

layout(location = 0) out VS_OUT 
{
   vec3 normal;
   vec3 tangent;
   vec3 bitangent;
   vec3 worldPos;
   vec3 lsPos;
   vec3 viewDir;
   vec2 uv;
   flat uint matId;
}vs_out;

void main()
{
   Vertex vertex = aVertices[gl_VertexIndex]; 

   vec3 position = vec3(vertex.px, vertex.py, vertex.pz);
   mat4 worldMatrix = aTransformData[gl_DrawIDARB];

   vec4 wP = worldMatrix * vec4(position, 1.0);
   gl_Position = globals.VP * wP;

   // Calculate normal matrix
   // http://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/
   vs_out.worldPos	= wP.xyz;
   vs_out.uv = vec2(vertex.tu, vertex.tv);

   mat3 normalTransform = mat3(transpose(inverse(worldMatrix)));
   vs_out.normal    = normalTransform * UnpackU8toFloat(vertex.nx, vertex.ny, vertex.nz);
   vs_out.tangent   = normalTransform * UnpackU8toFloat(vertex.tx, vertex.ty, vertex.tz);
   vs_out.bitangent = normalTransform * UnpackU8toFloat(vertex.bx, vertex.by, vertex.bz);
   vs_out.lsPos     = vec3(globals.V * wP);

   vs_out.viewDir   = globals.cameraPosition - wP.xyz;
   vs_out.matId     = gl_DrawIDARB;
}
