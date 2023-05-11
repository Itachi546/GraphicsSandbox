#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_ARB_shader_draw_parameters: require

#include "../includes/mesh.glsl"

#define MAX_WEIGHT 4
#define MAX_BONE_COUNT 80

layout(binding = 1) readonly buffer Vertices 
{
   AnimatedVertex aVertices[];
};

layout(binding = 2) uniform readonly SkinnedMatrix
{
  mat4 jointTransforms[MAX_BONE_COUNT];
};

void main()
{
   AnimatedVertex vertex = aVertices[gl_VertexIndex]; 
   vec3 position = vec3(vertex.px, vertex.py, vertex.pz);
   vec4 totalLocalPosition = vec4(0.0);
   for(int i = 0; i < MAX_WEIGHT; ++i)
   {
      int boneId = vertex.boneId[i];
      if(boneId == -1) continue;
      mat4 jointTransform = jointTransforms[boneId];
      vec4 posePosition = jointTransform * vec4(position, 1.0);
      totalLocalPosition += posePosition * vertex.boneWeight[i];
   }
   gl_Position = totalLocalPosition;
}