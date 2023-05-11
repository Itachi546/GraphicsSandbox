#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_ARB_shader_draw_parameters: require

#include "../includes/mesh.glsl"

#define MAX_WEIGHT 4
#define MAX_BONE_COUNT 80
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

struct LightData
{
    vec3 position;
	float radius;
	vec3 color;
	float type;
};


layout(binding = 0) uniform readonly Globals
{
   mat4 P;
   mat4 V;
   mat4 VP;

   vec3 cameraPosition;
   float dt;

   float bloomThreshold;
   int nLight;
   int enableNormalMapping;
   int enableCascadeDebug;

   LightData lights[128];
} globals;

layout(binding = 1) readonly buffer Vertices 
{
   AnimatedVertex aVertices[];
};

layout(binding = 2) uniform readonly SkinnedMatrix
{
  mat4 jointTransforms[MAX_BONE_COUNT];
};

vec3 BONE_COLORS[MAX_WEIGHT] = vec3[MAX_WEIGHT](
   vec3(1.0, 0.0, 0.0),
   vec3(0.0, 1.0, 0.0),
   vec3(0.0, 0.0, 1.0),
   vec3(1.0)
);

void main()
{
   AnimatedVertex vertex = aVertices[gl_VertexIndex]; 
   /*
     gl_DrawIDARB is used only when multiDrawIndirect is used
   */
   //PerObjectData perObjectData = aPerObjectData[gl_DrawIDARB];

   vec3 position = vec3(vertex.px, vertex.py, vertex.pz);
   vec3 t = UnpackU8toFloat(vertex.tx, vertex.ty, vertex.tz);
   vec3 b = UnpackU8toFloat(vertex.bx, vertex.by, vertex.bz);
   vec3 n = UnpackU8toFloat(vertex.nx, vertex.ny, vertex.nz);

   vec4 wP = vec4(0.0);
   vec4 totalT = vec4(0.0);
   vec4 totalB = vec4(0.0);
   vec4 totalN = vec4(0.0);

   for(int i = 0; i < MAX_WEIGHT; ++i)
   {
      int boneId = vertex.boneId[i];
      if(boneId == -1) continue;
      mat4 jointTransform = jointTransforms[boneId] * vertex.boneWeight[i];

      wP += jointTransform * vec4(position, 1.0);
      totalT += jointTransform * vec4(t, 0.0);
      totalB += jointTransform * vec4(b, 0.0);
      totalN += jointTransform * vec4(n, 0.0);
   }

   //vec4 wP = modelMatrix * vec4(position, 1.0f);
   gl_Position = globals.VP * wP;

   // Calculate normal matrix
   // http://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/
   vs_out.worldPos	= wP.xyz;
   vs_out.tangent = totalT.xyz;
   vs_out.bitangent = totalB.xyz;
   vs_out.normal = totalN.xyz;
   vs_out.uv = vec2(vertex.tu, vertex.tv);
   vs_out.lsPos  = vec3(globals.V * wP);

   vs_out.viewDir   = globals.cameraPosition - wP.xyz;
   vs_out.matId     = gl_DrawIDARB;
}