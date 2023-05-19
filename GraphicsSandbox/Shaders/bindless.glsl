#ifndef BINDLESS_GLSL
#define BINDLESS_GLSL

#extension GL_EXT_nonuniform_qualifier : enable
layout (set = 1, binding = 10) uniform sampler2D uTextures[];
layout (set = 1, binding = 10 )	uniform	sampler3D uTextures3D[];
layout (set = 1, binding = 10 )	uniform	samplerCube uTexturesCube[];

#endif