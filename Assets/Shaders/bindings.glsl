#ifdef VERTEX_SHADER
layout(binding = 0) uniform readonly Globals
{
   mat4 P;
   mat4 V;
   mat4 VP;
   vec3 cameraPosition;
   float dt;
} globals;

layout(binding = 1) readonly buffer Vertices 
{
   Vertex aVertices[];
};

layout(binding = 2) readonly buffer PerObject
{
   PerObjectData aPerObjectData[];
};

layout(binding = 3) readonly buffer TransformData
{
   mat4 aTransformData[];
};
#endif

#ifdef FRAGMENT_SHADER
   layout(binding = 4) readonly buffer MaterialData
   {
       Material aMaterialData[];
   };
   layout(binding = 5) uniform samplerCube uIrradianceMap;
#endif
