#version 450

#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_NV_mesh_shader : require 
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_shader_draw_parameters : require

#include "meshdata.glsl"

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

layout(binding = 3) readonly buffer DrawCommands {
   MeshDrawCommand drawCommands[];
};

layout(binding = 5) readonly buffer MeshletData {
  Meshlet meshlets[];
};

out taskNV block {
  uint meshletIndices[32];
};

void main() {
  uint ti = gl_LocalInvocationID.x;
  uint gi = gl_WorkGroupID.x;
  uint mi = gi * 32 + ti;

  MeshDrawCommand drawCommand = drawCommands[gl_DrawIDARB];

  meshletIndices[ti] = mi;
  if(ti == 0)
     gl_TaskCountNV = 32;
}