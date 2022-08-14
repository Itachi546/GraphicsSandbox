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
struct Vertex 
{
	float px, py, pz;
	float nx, ny, nz;
	float tu, tv;
};

struct PerObjectData
{
	mat4 worldMatrix;
};
