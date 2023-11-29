#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D uInputTexture;
layout(binding = 1, r16f) uniform image2D uOutputTexture;

layout(push_constant) uniform PushConstants {
   int width;
   int height;
};

void main() {
   const int radius = 2;
   ivec2 coord = ivec2(gl_GlobalInvocationID.xy);

   vec2 invRes = 1.0f / vec2(width, height);

   vec3 color = vec3(0.0);
   int sampleCount = 0;
   for(int dx = -radius; dx <= radius; dx++) 
   {
      for(int dy = -radius; dy <= radius; dy++) 
      {
         vec2 uv = vec2(coord.x + dx, coord.y + dy) * invRes;
         color += texture(uInputTexture, uv).rgb;
         sampleCount ++;
      }
   }
   color /= float(sampleCount);
   imageStore(uOutputTexture, coord, vec4(color, 1.0f));
} 