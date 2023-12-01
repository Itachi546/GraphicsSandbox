const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

float textureProj(vec4 shadowCoord, vec2 offset, uint shadowMap, int cascadeIndex, float bias)
{
  float shadow = 1.0;
  float currentDepth = shadowCoord.z;
  if(currentDepth >	-1.0 &&	currentDepth <	1.0)
  {
     float depthFromTexture = texture(uTexturesArray[nonuniformEXT(shadowMap)], vec3(shadowCoord.xy + offset, cascadeIndex)).r;
	 if(shadowCoord.w > 0.0 && depthFromTexture < currentDepth)
	    shadow = 0.0f;
  }
  return shadow;
}

float CalculateShadowFromTexture(vec3 worldPos, uint shadowMap, int cascadeIndex)
{
    if(cascadeIndex >= MAX_CASCADES)
      return 1.0f;

    Cascade cascade = cascades[cascadeIndex];
    // Transform into light NDC Coordinate
    vec4 shadowCoord = (biasMat * cascade.VP) * vec4(worldPos, 1.0f);


    float shadowFactor = 0.0f;
    const float scale = 1.0f;
    vec2 shadowDims = vec2(shadowMapWidth, shadowMapHeight);
    vec2 invRes = scale / shadowDims.xy;

    int kSampleRadius = uPCFRadius;
    int sampleCount = 0;

    float multiplierX = invRes.x * uPCFRadiusMultiplier;
    float multiplierY = invRes.y * uPCFRadiusMultiplier;
    for(int x = -kSampleRadius; x <= kSampleRadius; ++x)
    {
       for(int y = -kSampleRadius; y <= kSampleRadius; ++y)
       {
          shadowFactor += textureProj(shadowCoord / shadowCoord.w, vec2(x * multiplierX, y * multiplierY), shadowMap, cascadeIndex, 0.001f);
          sampleCount ++;
       }
    }
    return shadowFactor / sampleCount;
}

float CalculateShadowFactor(vec3 worldPos, float camDist, uint shadowMap, out int cascadeIndex)
{

    cascadeIndex = 0;
    for(int i = 0; i < MAX_CASCADES; ++i)
    {
        if(camDist <= cascades[i].splitDistance.x) 
        {
    		cascadeIndex = i;
            break;
        }
    }

    return CalculateShadowFromTexture(worldPos, shadowMap, cascadeIndex);
}

