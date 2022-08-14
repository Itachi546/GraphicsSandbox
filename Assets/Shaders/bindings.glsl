#ifdef VERTEX_SHADER
layout(binding = 0) readonly uniform Globals
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
#endif

#ifdef FRAGMENT_SHADER
   layout(binding = 3) uniform samplerCube uEnvMap;
#endif
