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
} vs_out[];

uint hash(uint a)
{
   a = (a+0x7ed55d16) + (a<<12);
   a = (a^0xc761c23c) ^ (a>>19);
   a = (a+0x165667b1) + (a<<5);
   a = (a+0xd3a2646c) ^ (a<<9);
   a = (a+0xfd7046c5) + (a<<3);
   a = (a^0xb55a4f09) ^ (a>>16);
   return a;
}

void main() {
    uint ti = gl_LocalInvocationID.x;
    uint mi = gl_WorkGroupID.x;

    uint vertexCount = meshlets[mi].vertexCount;
    uint triangleCount = meshlets[mi].triangleCount;
    uint vertexOffset = meshlets[mi].vertexOffset;
    uint triangleOffset = meshlets[mi].triangleOffset;

    uint indexCount = triangleCount * 3;
    uint mhash = hash(mi);
    vec3 mColor = vec3(float(mhash & 255), float((mhash >> 8) & 255), float((mhash >> 16) & 255)) / 255.0;

    for(uint i = ti; i < vertexCount; i+= 32)
    {
        uint vi = meshletVertices[vertexOffset + i];
        Vertex vertex = aVertices[vi];

        vec3 position = vec3(vertex.px, vertex.py, vertex.pz);
        vec4 wP =  vec4(position, 1.0);
        gl_MeshVerticesNV[i].gl_Position = globalData.VP * wP;

        mat3 normalTransform = mat3(1.0);
		vs_out[i].normal	 = normalTransform * UnpackU8toFloat(vertex.nx,	vertex.ny, vertex.nz);
		vs_out[i].tangent	 = normalTransform * UnpackU8toFloat(vertex.tx,	vertex.ty, vertex.tz);
		//vs_out[i].bitangent  = normalTransform * UnpackU8toFloat(vertex.bx,	vertex.by, vertex.bz);
        vs_out[i].bitangent  = mColor;
		vs_out[i].lsPos	     = vec3(globalData.V * wP);
		vs_out[i].viewDir	 = globalData.cameraPosition - wP.xyz;
		vs_out[i].matId	     = mi;
		vs_out[i].worldPos   = wP.xyz;
    }

    for(uint i = ti; i < indexCount; i+= 32)
    {
        gl_PrimitiveIndicesNV[i] = meshletTriangles[triangleOffset + i];
    }

    if(ti == 0)
      gl_PrimitiveCountNV = triangleCount;
}