#version 450

#extension GL_GOOGLE_include_directive: require
#extension GL_ARB_shader_draw_parameters: require

#include "globaldata.glsl"
#include "meshdata.glsl"

layout(binding = 0) readonly uniform Globals
{
   GlobalData globalData;
};

layout(binding = 1) readonly buffer Vertices 
{
   Vertex aVertices[];
};

layout(location = 0) out vec3 uv;

void main()
{
    Vertex vertex = aVertices[gl_VertexIndex]; 
    vec3 position = vec3(vertex.px, vertex.py, vertex.pz);
    uv = position;

    vec4 worldPos = globalData.P * globalData.V * vec4(position, 0.0f);
    gl_Position = worldPos.xyww;

}