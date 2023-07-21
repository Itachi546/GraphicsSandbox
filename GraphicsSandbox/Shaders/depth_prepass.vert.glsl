#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_ARB_shader_draw_parameters: require

#include "meshdata.glsl"
#include "globaldata.glsl"

// Bindings
layout(binding = 0) uniform readonly Globals
{
   GlobalData globalData;
};

layout(binding = 1) readonly buffer Vertices 
{
   Vertex aVertices[];
};

layout(binding = 2) readonly buffer TransformData
{
   mat4 aTransformData[];
};

layout(binding = 3) readonly buffer DrawCommands {
   MeshDrawCommand drawCommands[];
};

void main()
{
   MeshDrawCommand drawCommand = drawCommands[gl_DrawIDARB];
   uint drawId = drawCommand.drawId;

   Vertex vertex = aVertices[gl_VertexIndex]; 

   vec3 position = vec3(vertex.px, vertex.py, vertex.pz);
   mat4 worldMatrix = aTransformData[drawId];
   gl_Position = globalData.VP * worldMatrix * vec4(position, 1.0);
}
