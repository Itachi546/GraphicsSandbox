#version 450

#extension GL_GOOGLE_include_directive: require

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 brightColor;

layout(location = 0) in VS_OUT 
{
   vec3 normal;
   vec3 tangent;
   vec3 bitangent;
   vec3 worldPos;
   vec3 lsPos;
   vec3 viewDir;
   vec2 uv;
   flat uint matId;
} fs_in;

#define FRAGMENT_SHADER
#include "bindings.glsl"
#include "shadow.glsl"
#include "pbr.glsl"

#extension GL_EXT_nonuniform_qualifier : enable

layout (set = 1, binding = 10) uniform sampler2D textures[];

void main()
{
	Material material = aMaterialData[fs_in.matId];
    vec3 Lo = material.albedo.rgb;
	if(material.albedoMap != INVALID_TEXTURE)
	 Lo = texture(textures[material.albedoMap], fs_in.uv).rgb;

    float luminance = dot(Lo, vec3(0.2126, 0.7152, 0.0722));
	if(luminance > globals.bloomThreshold || material.emissive > 0.01f)
	{
	    // Nasty inf pixel that become visible in bloom
		if(isinf(Lo.x))
		  brightColor = vec4(0.0f);
	    else
		   brightColor	= vec4(Lo, 1.0f);
	}
	else 
	    brightColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);

	fragColor =	vec4(Lo, 1.0f);
}