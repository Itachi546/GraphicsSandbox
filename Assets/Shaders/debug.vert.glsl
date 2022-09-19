#version 450

struct DebugData
{
   float x, y, z;
   uint color;
};

layout(binding = 0) readonly buffer Vertices 
{
   DebugData aVertices[];
};

layout(location = 0) out vec3 color;

layout(push_constant) uniform Matrices 
{ 
   mat4 VP;
};

void main()
{
   DebugData data = aVertices[gl_VertexIndex];
   gl_Position = VP * vec4(data.x, data.y, data.z, 1.0f);

   color.b = (data.color & 0xFF) / 255.0f;
   color.g = ((data.color >> 8) & 0xFF) / 255.0f;
   color.r = ((data.color >> 16) & 0xFF) / 255.0f;
}