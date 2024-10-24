#ifndef MESHDATA_GLSL
#define MESHDATA_GLSL

/*
* We cannot use the 
* struct Vertex 
* {
*   vec3 position;
*   vec3 normal;
*   vec3 texCoord;
* }
*  due to alignment issues. It tries to convert the
*  vec3 into vec4 due to alignment requirement.
*/
#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

struct Vertex 
{
	float px, py, pz;
	uint8_t nx, ny, nz;
	float tu, tv;
	uint8_t tx, ty, tz;
	uint8_t bx, by, bz;
};

struct AnimatedVertex
{
	float px, py, pz;
	uint8_t nx, ny, nz;
	float tu, tv;
	uint8_t tx, ty, tz;
	uint8_t bx, by, bz;
	int boneId[4];
	float boneWeight[4];
};

struct PerObjectData
{
	uint transformIndex;
	uint materialIndex;
};

vec3 UnpackU8toFloat(uint nx, uint ny, uint nz)
{
   return vec3((nx - 127.0f) / 127.0f, (ny - 127.0f) / 127.0f, (nz - 127.0f) / 127.0f);
}


struct Meshlet {
	// Offset specified in element count
	uint vertexOffset;
	uint vertexCount;

	// Offset specified in element count
	uint triangleOffset;
	uint triangleCount;

	vec4 cone;
};


struct MeshDrawData {
	vec4 boundingSphere;

    uint vertexOffset;
	uint vertexCount;
	uint indexOffset;
	uint indexCount;

	uint meshletOffset;
	uint meshletCount;
	uint meshletTriangleOffset;
	uint meshletTriangleCount;
};

struct MeshDrawCommand {
   uint indexCount;
   uint instanceCount;
   uint firstIndex;

   uint vertexOffset;
   uint firstInstance;
   uint taskCount;
   uint firstTask;
   uint drawId;
};

#endif