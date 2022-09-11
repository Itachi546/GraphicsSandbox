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
   vec2 unused_;

   LightData lights[128];
} globals;

#ifdef VERTEX_SHADER

layout(binding = 1) readonly buffer Vertices 
{
   Vertex aVertices[];
};

layout(binding = 2) readonly buffer TransformData
{
   mat4 aTransformData[];
};
#endif

#ifdef FRAGMENT_SHADER
   layout(binding = 3) readonly buffer MaterialData
   {
       Material aMaterialData[];
   };
   layout(binding = 4) uniform samplerCube uIrradianceMap;
   layout(binding = 5) uniform samplerCube uPrefilterEnvMap;
   layout(binding = 6) uniform sampler2D   uBRDFLUT;
   layout(binding = 7) uniform sampler2D   uTextures[64];
#endif
