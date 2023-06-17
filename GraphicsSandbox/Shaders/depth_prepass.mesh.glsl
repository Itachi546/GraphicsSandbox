#version 450

#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_NV_mesh_shader: require
#extension GL_GOOGLE_include_directive: require

#include "meshdata.glsl"
#include "globaldata.glsl"

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
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

layout(binding = 3) readonly buffer MeshletData {
  Meshlet meshlets[];
};

layout(binding = 4) readonly buffer MeshletVerticesData {
  uint meshletVertices[];
};

layout(binding = 5) readonly buffer MeshletTrianglesData {
  uint8_t meshletTriangles[];
};

void main() {
    uint ti = gl_LocalInvocationID.x;
    uint mi = gl_WorkGroupID.x;

    uint vertexCount = meshlets[mi].vertexCount;
    uint triangleCount = meshlets[mi].triangleCount;
    uint vertexOffset = meshlets[mi].vertexOffset;
    uint triangleOffset = meshlets[mi].triangleOffset;

    uint indexCount = triangleCount * 3;

    for(uint i = ti; i < vertexCount; i+= 32)
    {
        uint vi = meshletVertices[vertexOffset + i];
        Vertex vertex = aVertices[vi];

        vec3 position = vec3(vertex.px, vertex.py, vertex.pz);
        vec4 wP =  vec4(position, 1.0);
        gl_MeshVerticesNV[i].gl_Position = globalData.VP * wP;
    }

    for(uint i = ti; i < indexCount; i+= 32)
    {
        gl_PrimitiveIndicesNV[i] = meshletTriangles[triangleOffset + i];
    }

    if(ti == 0)
      gl_PrimitiveCountNV = triangleCount;
}