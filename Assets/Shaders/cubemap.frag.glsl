#version 450

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 brightColor;

layout(binding = 2) uniform samplerCube uCubemap;

layout(location = 0) in vec3 uv;

const float bloomThreshold = 1.0f;
void main()
{
   vec3 col = texture(uCubemap, uv).rgb;
   //col /= (1.0f + col);
   //col = pow(col, vec3(0.4545));

   float luminance = dot(col, vec3(0.2126, 0.7152, 0.0722));
   if(luminance > bloomThreshold)
     	brightColor = vec4(col, 1.0f);
	else 
	    brightColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);


   fragColor = vec4(col, 1.0f);
}