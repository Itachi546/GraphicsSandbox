struct Cascade {
   mat4 VP;
   vec4 splitDistance;
};

const int MAX_CASCADES = 5;

#ifdef FRAGMENT_SHADER
layout(binding = 7, std140) uniform CascadeInfo
{
   Cascade cascades[MAX_CASCADES];
   vec4 shadowDims;
};

layout(binding = 8) uniform sampler2DArray uDepthMap;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

float textureProj(int cascadeIndex, vec4 shadowCoord, vec2 offset, float bias)
{
  float shadow = 1.0;
  float currentDepth = shadowCoord.z;
  if(currentDepth > -1.0 && currentDepth <  1.0)
  {
     float depthFromTexture = texture(uDepthMap, vec3(shadowCoord.xy + offset, cascadeIndex)).r;
	 if(shadowCoord.w > 0.0 && depthFromTexture < currentDepth - bias)
	    shadow = 0.0f;
  }
  return shadow;
}

float CalculateShadowFromTexture(vec3 worldPos, int cascadeIndex)
{
    if(cascadeIndex >= MAX_CASCADES)
      return 1.0f;

    Cascade cascade = cascades[cascadeIndex];
    // Transform into light NDC Coordinate
    vec4 lsPosition = biasMat * cascade.VP * vec4(worldPos, 1.0f);
    lsPosition.xyz /= lsPosition.w;

    float shadow = 0.0f;
    vec2 invRes = 1.0f / shadowDims.xy;
    float sampleCount = 0.0f;
    for(int x = -1; x <= 1; ++x)
    {
       for(int y = -1; y <= 1; ++y)
       {
          shadow += textureProj(cascadeIndex, lsPosition, vec2(x * invRes.x, y * invRes.y), 0.0f);
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
        if(camDist < cascades[i].splitDistance.x) 
        {
    		currentCascade = i;
            break;
        }
    }

    return CalculateShadowFromTexture(worldPos, currentCascade);
}

#endif
