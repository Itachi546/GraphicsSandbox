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

vec2 poissonDisk[16] = vec2[]( 
   vec2( -0.94201624, -0.39906216 ), 
   vec2( 0.94558609, -0.76890725 ), 
   vec2( -0.094184101, -0.92938870 ), 
   vec2( 0.34495938, 0.29387760 ), 
   vec2( -0.91588581, 0.45771432 ), 
   vec2( -0.81544232, -0.87912464 ), 
   vec2( -0.38277543, 0.27676845 ), 
   vec2( 0.97484398, 0.75648379 ), 
   vec2( 0.44323325, -0.97511554 ), 
   vec2( 0.53742981, -0.47373420 ), 
   vec2( -0.26496911, -0.41893023 ), 
   vec2( 0.79197514, 0.19090188 ), 
   vec2( -0.24188840, 0.99706507 ), 
   vec2( -0.81409955, 0.91437590 ), 
   vec2( 0.19984126, 0.78641367 ), 
   vec2( 0.14383161, -0.14100790 ) 
);

float rand(vec2 uv) {
   float dot_product = dot(uv, vec2(12.9898,78.233));
   return fract(sin(dot_product) * 43758.5453);
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
          vec2 coord = vec2(x * multiplierX, y * multiplierY);
          int index = int(rand(coord) * 15.0);
          shadowFactor += textureProj(shadowCoord / shadowCoord.w, coord + poissonDisk[index] * invRes, shadowMap, cascadeIndex, 0.001f);
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

