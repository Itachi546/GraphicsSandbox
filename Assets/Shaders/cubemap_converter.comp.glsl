#version 450

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D uHDRI;
layout(binding = 1, rgba16f) uniform writeonly imageCube uCubeMap;

//layout(push_constant) uniform UniformData
//{ 
//  vec2 uCubemapSize;
//};

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 sphericalCoord(vec3 p)
{
   vec2 uv = vec2(atan(p.z, p.x), asin(p.y)); // x[-pi, pi], y[-pi/2, pi/2]
   uv *= invAtan; // [-0.5, 0.5]
   uv += 0.5f;
   return uv;
}

vec3 uvToXYZ(ivec3 cubeCoord, vec2 cubemapSize)
{
   vec2 texCoord = vec2(cubeCoord.xy) / cubemapSize;
   texCoord = texCoord * 2.0f - 1.0f;
   switch(cubeCoord.z)
   {
      case 0: return vec3( 1.0f,       -texCoord.yx); // +X
      case 1: return vec3(-1.0f,       -texCoord.y, texCoord.x); // -X
      case 2: return vec3( texCoord.x,  1.0f,       texCoord.y);  // +Y
      case 3: return vec3( texCoord.x, -1.0f,       -texCoord.y); // -Y
      case 4: return vec3( texCoord.x, -texCoord.y,  1.0f); // +Z
      case 5: return vec3(-texCoord.xy,             -1.0f); // -Z
   }

   return vec3(0.0);
}

void main()
{
    ivec3 cubeCoord = ivec3(gl_GlobalInvocationID.xyz);
    // TODO: Use push constants
    vec2 cubemapSize = vec2(512.0f);

    // Don't forget to normalize P
	vec3 p = normalize(uvToXYZ(cubeCoord, cubemapSize));

    vec4 col = texture(uHDRI, sphericalCoord(p));
    imageStore(uCubeMap, cubeCoord, col);
}