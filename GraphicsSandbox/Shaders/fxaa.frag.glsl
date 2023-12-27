#version 460

#extension GL_GOOGLE_include_directive : require
#include "bindless.glsl"
#include "utils.glsl"

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

layout(push_constant) uniform PushConstants {
  uint uInputTexture;
  float invWidth;
  float invHeight;
  float edgeThresholdMin;
  float edgeThresholdMax;
};

const int ITERATIONS = 12;
const float QUALITY[7] = float[7](1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0);
/*
 * FXAA takes the non linear input texture and internally convert
 * it into linear value. This is approximated by sqrt when converting to
 * luma.
 * However we already output the value with gamma correction from the 
 * lighting pass.
*/
void main()
{
   vec4 col = texture(uTextures[nonuniformEXT(uInputTexture)], uv);

   // Calculate luminance of current pixels and it's neighbour
   float lumaCenter = rgb2Luma(col.rgb);
   float lumaUp    =  rgb2Luma(textureOffset(uTextures[nonuniformEXT(uInputTexture)], uv, ivec2(0, 1)).rgb);
   float lumaDown  =  rgb2Luma(textureOffset(uTextures[nonuniformEXT(uInputTexture)], uv, ivec2(0, -1)).rgb);
   float lumaLeft  =  rgb2Luma(textureOffset(uTextures[nonuniformEXT(uInputTexture)], uv, ivec2(-1, 0)).rgb);
   float lumaRight =  rgb2Luma(textureOffset(uTextures[nonuniformEXT(uInputTexture)], uv, ivec2(1, -1)).rgb);

   // Calculate maximum and minimum luma around current fragment
   float lumaMax = max(lumaCenter, max(lumaUp, max(lumaDown, max(lumaLeft, lumaRight))));
   float lumaMin = min(lumaCenter, min(lumaUp, min(lumaDown, min(lumaLeft, lumaRight))));

   float lumaRange = lumaMax - lumaMin;

   // If the luminance range is below threshold then we are not in the edge
   if(lumaRange < max(edgeThresholdMin, lumaMax * edgeThresholdMax)) {
     fragColor = col;
     return;
   }
   // We have detected the edge, now we need to smooth it out
   // Choose edge direction
   float lumaDownLeft =  rgb2Luma(textureOffset(uTextures[nonuniformEXT(uInputTexture)], uv, ivec2(-1,-1)).rgb);
   float lumaUpRight =   rgb2Luma(textureOffset(uTextures[nonuniformEXT(uInputTexture)], uv, ivec2(1,1)).rgb);
   float lumaUpLeft	=    rgb2Luma(textureOffset(uTextures[nonuniformEXT(uInputTexture)], uv, ivec2(-1,1)).rgb);
   float lumaDownRight = rgb2Luma(textureOffset(uTextures[nonuniformEXT(uInputTexture)], uv, ivec2(1,-1)).rgb);

   float lumaDownUp = lumaDown + lumaUp;
   float lumaLeftRight = lumaLeft + lumaRight;

   float lumaLeftCorners = lumaDownLeft + lumaUpLeft;
   float lumaDownCorners = lumaDownLeft	+ lumaDownRight;
   float lumaRightCorners =	lumaDownRight +	lumaUpRight;
   float lumaUpCorners = lumaUpRight + lumaUpLeft;

   // Compute an estimation of the gradient along the horizontal and vertical axis.
   float edgeHorizontal =	abs(-2.0 * lumaLeft	+ lumaLeftCorners)	+ abs(-2.0 * lumaCenter	+ lumaDownUp ) * 2.0	+ abs(-2.0 * lumaRight + lumaRightCorners);
   float edgeVertical =    abs(-2.0 * lumaUp + lumaUpCorners)      + abs(-2.0 * lumaCenter + lumaLeftRight) * 2.0  + abs(-2.0 * lumaDown + lumaDownCorners);

   bool isHorizontal = edgeHorizontal >= edgeVertical;
   // Select two neighbouring texel luma
   float luma1 = isHorizontal ? lumaDown : lumaLeft;
   float luma2 = isHorizontal ? lumaUp : lumaRight;

   float gradient1 = luma1 - lumaCenter;
   float gradient2 = luma2 - lumaCenter;

   bool is1Steepest = abs(gradient1) >= abs(gradient2);
   float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));

   float stepLength = isHorizontal ? invHeight : invWidth;
   float lumaLocalAverage = 0.0;

   if(is1Steepest) {
     stepLength = -stepLength;
     lumaLocalAverage = 0.5 * (luma1 + lumaCenter);
   }
   else 
     lumaLocalAverage = 0.5 * (luma2 + lumaCenter);

   vec2 currentUv = uv;
   if(isHorizontal)
      currentUv.y += stepLength * 0.5;
   else 
     currentUv.x += stepLength * 0.5;

   // Compute offset (for each iteration step) in the right direction.
   vec2	offset = isHorizontal ?	vec2(invWidth,0.0) :	vec2(0.0,invHeight);
   // Compute UVs to explore on	each side of the edge, orthogonally. The QUALITY allows	us to step faster.
   vec2	uv1	= currentUv	- offset;
   vec2	uv2	= currentUv	+ offset;

   // Read the lumas at	both current extremities of	the	exploration	segment, and compute the delta wrt to the local	average	luma.
   float lumaEnd1 =	rgb2Luma( texture(uTextures[nonuniformEXT(uInputTexture)],uv1).rgb);
   float lumaEnd2 =	rgb2Luma(texture(uTextures[nonuniformEXT(uInputTexture)],uv2).rgb);
   lumaEnd1	-= lumaLocalAverage;
   lumaEnd2	-= lumaLocalAverage;

   // If the luma deltas at	the	current	extremities	are	larger than	the	local gradient,	we have	reached	the	side of	the	edge.
   bool	reached1 = abs(lumaEnd1) >=	gradientScaled;
   bool	reached2 = abs(lumaEnd2) >=	gradientScaled;
   bool	reachedBoth	= reached1 && reached2;

   // If the side is not reached, we continue to explore in	this direction.
   if(!reached1){
     uv1 -= offset;
   }
   if(!reached2){
    uv2 += offset;
   }
   // If both sides have not been reached, continue to explore.
   if(!reachedBoth){
    for(int i = 2; i < ITERATIONS; i++){
        // If needed, read luma in 1st direction, compute delta.
        if(!reached1){
            lumaEnd1 = rgb2Luma(texture(uTextures[nonuniformEXT(uInputTexture)], uv1).rgb);
            lumaEnd1 = lumaEnd1 - lumaLocalAverage;
        }
        // If needed, read luma in opposite direction, compute delta.
        if(!reached2){
            lumaEnd2 = rgb2Luma(texture(uTextures[nonuniformEXT(uInputTexture)], uv2).rgb);
            lumaEnd2 = lumaEnd2 - lumaLocalAverage;
        }
        // If the luma deltas at the current extremities is larger than the local gradient, we have reached the side of the edge.
        reached1 = abs(lumaEnd1) >= gradientScaled;
        reached2 = abs(lumaEnd2) >= gradientScaled;
        reachedBoth = reached1 && reached2;

        // If the side is not reached, we continue to explore in this direction, with a variable quality.
        if(!reached1){
            uv1 -= offset * QUALITY[i];
        }
        if(!reached2){
            uv2 += offset * QUALITY[i];
        }

        // If both sides have been reached, stop the exploration.
        if(reachedBoth){ break;}
		}	
    }

    // Compute the distances to each extremity of the edge.
	float distance1	= isHorizontal ? (uv.x -  uv1.x) : (uv.y - uv1.y);
	float distance2	= isHorizontal ? (uv2.x	- uv.x) : (uv2.y - uv.y);

	// In which	direction is the extremity of the edge closer ?
	bool isDirection1 =	distance1 <	distance2;
	float distanceFinal	= min(distance1, distance2);

	// Length of the edge.
	float edgeThickness	= (distance1 + distance2);

	// UV offset: read in the direction	of the closest side	of the edge.
	float pixelOffset =	- distanceFinal	/ edgeThickness	+ 0.5;

    // Is the luma at center smaller than the local average ?
	bool isLumaCenterSmaller = lumaCenter <	lumaLocalAverage;

	// If the luma at center is	smaller	than at	its	neighbour, the delta luma at each end should be	positive (same variation).
	// (in the direction of	the	closer side	of the edge.)
	bool correctVariation =	((isDirection1 ? lumaEnd1 :	lumaEnd2) <	0.0) !=	isLumaCenterSmaller;

	// If the luma variation is	incorrect, do not offset.
	float finalOffset =	correctVariation ? pixelOffset : 0.0;

    // Sub-pixel shifting
	// Full	weighted average of	the	luma over the 3x3 neighborhood.
	float lumaAverage =	(1.0/12.0) * (2.0 *	(lumaDownUp	+ lumaLeftRight) + lumaLeftCorners + lumaRightCorners);
	// Ratio of	the	delta between the global average and the center	luma, over the luma	range in the 3x3 neighborhood.
	float subPixelOffset1 =	clamp(abs(lumaAverage -	lumaCenter)/lumaRange,0.0,1.0);
	float subPixelOffset2 =	(-2.0 *	subPixelOffset1	+ 3.0) * subPixelOffset1 * subPixelOffset1;
	// Compute a sub-pixel offset based	on this	delta.
	float subPixelOffsetFinal =	subPixelOffset2	* subPixelOffset2 *	0.75;

	// Pick	the	biggest	of the two offsets.
	finalOffset	= max(finalOffset,subPixelOffsetFinal);

    // Compute the final UV coordinates.
	vec2 finalUv = uv;
	if(isHorizontal){
    	finalUv.y += finalOffset * stepLength;
	} else {
		finalUv.x += finalOffset * stepLength;
	}

	// Read	the	color at the new UV	coordinates, and use it.
	vec4 finalColor	= texture(uTextures[nonuniformEXT(uInputTexture)],finalUv);
    fragColor = finalColor;
}