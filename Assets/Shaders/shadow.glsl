struct Cascade {
   mat4 VP;
   vec4 splitDistance;
};

const int MAX_CASCADES = 5;

#ifdef FRAGMENT_SHADER
layout(binding = 7, std140) uniform CascadeInfo
{
   Cascade cascades[MAX_CASCADES];
};

layout(binding = 8) uniform sampler2DArray uDepthMap;

float textureProj(int cascadeIndex, vec4 shadowCoord, vec2 offset, float bias)
{
  float shadow = 1.0;
  if(shadowCoord.z > -1.0 && shadowCoord.z <  1.0)
  {
     float dist = texture(uDepthMap, vec3(shadowCoord.x + offset.x, 1.0f - shadowCoord.y + offset.y, cascadeIndex)).r;
	 if(shadowCoord.w > 0.0 && dist < shadowCoord.z - bias)
	    shadow = 0.1f;
  }
  return shadow;
}

float CalculateShadowFromTexture(vec3 worldPos, int cascadeIndex)
{
    if(cascadeIndex >= MAX_CASCADES)
      return 1.0f;

    Cascade cascade = cascades[cascadeIndex];
    // Transform into light NDC Coordinate
    vec4 lsPosition = cascade.VP * vec4(worldPos, 1.0f);
    lsPosition.xyz /= lsPosition.w;

    // Transform into light ClipSpace position
    lsPosition.xy = lsPosition.xy * 0.5f + 0.5f;

    float shadow = 0.0f;
    float invRes = 1.0f / 2048.0f;
    float sampleCount = 0.0f;
    for(int x = -1; x <= 1; ++x)
    {
       for(int y = -1; y <= 1; ++y)
       {
          shadow += textureProj(cascadeIndex, lsPosition, vec2(x * invRes, y * invRes), 0.01f);
          sampleCount ++;
       }
    }
    return shadow /= sampleCount;
}

float CalculateShadowFactor(vec3 worldPos, float camDist)
{

    int currentCascade = 0;
    for(int i = 0; i < MAX_CASCADES; ++i)
    {
        if(camDist > cascades[i].splitDistance.x) 
           currentCascade += 1;
    }

    return CalculateShadowFromTexture(worldPos, currentCascade);

/*
    vec4 lsPosition = cascades[2].VP * vec4(worldPos, 1.0f);

    // Transform into light ClipSpace position
    lsPosition.xy = lsPosition.xy * 0.5f + 0.5f;
    lsPosition.xyz /= lsPosition.w;

    float depthFromTexture = texture(uDepthMap, vec3(lsPosition.x, 1.0f - lsPosition.y, 2)).r;
    return depthFromTexture < lsPosition.z	? 0.0f : 1.0f;
    */
}

#endif
