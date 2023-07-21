#version 450

#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_NV_mesh_shader: require
#extension GL_GOOGLE_include_directive: require
#extension GL_ARB_shader_draw_parameters: require

#include "meshdata.glsl"
#include "globaldata.glsl"

const uint LOCAL_SIZE = 32;
layout(local_size_x = LOCAL_SIZE, local_size_y = 1, local_size_z = 1) in;
layout(triangles, max_vertices = 64, max_primitives = 126) out;

layout(binding = 0) uniform readonly Globals {
    GlobalData globalData;
};

layout(binding = 1) readonly buffer Vertices {
   Vertex aVertices[];
};

layout(binding = 2) readonly buffer TransformData
{
   mat4 aTransformData[];
};

layout(binding = 3) readonly buffer DrawCommands {
  MeshDrawCommand drawCommands[];
};

layout(binding = 4) readonly buffer MeshletData {
  Meshlet meshlets[];
};

layout(binding = 5) readonly buffer MeshletVerticesData {
  uint meshletVertices[];
};

layout(binding = 6) readonly buffer MeshletTrianglesData {
  uint8_t meshletTriangles[];
};

void main() {

    uint ti = gl_LocalInvocationID.x;
    uint mi = gl_WorkGroupID.x;

    MeshDrawCommand drawCommand = drawCommands[gl_DrawIDARB];
    uint drawId = drawCommand.drawId;

    uint vertexCount = meshlets[mi].vertexCount;
    uint triangleCount = meshlets[mi].triangleCount;
    uint vertexOffset = meshlets[mi].vertexOffset;
    uint triangleOffset = meshlets[mi].triangleOffset;

    uint indexCount = triangleCount * 3;
    uint globalVBOffset = drawCommand.vertexOffset;

    mat4 modelMatrix = aTransformData[drawId];
    for(uint i = ti; i < vertexCount; i+= LOCAL_SIZE)
    {
        uint vi = meshletVertices[vertexOffset + i];
        Vertex vertex = aVertices[vi + globalVBOffset];

        vec3 position = vec3(vertex.px, vertex.py, vertex.pz);
        vec4 wP =  modelMatrix *  vec4(position, 1.0);
        gl_MeshVerticesNV[i].gl_Position = globalData.VP * wP;
    }

    for(uint i = ti; i < indexCount; i+= LOCAL_SIZE)
    {
        gl_PrimitiveIndicesNV[i] = uint(meshletTriangles[triangleOffset + i]);
    }

    if(ti == 0)
      gl_PrimitiveCountNV = triangleCount;
}