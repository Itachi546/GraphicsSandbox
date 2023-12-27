float rgb2Luma(vec3 rgb)
{
	//return sqrt(dot(rgb, vec3(0.299, 0.587, 0.114)));
	return dot(rgb, vec3(0.299, 0.587, 0.114));
}

vec3 ACESFilm(vec3 x)
{
   float a = 2.51f;
   float b = 0.03f;
   float c = 2.43f;
   float d = 0.59f;
   float e = 0.14f;
   return clamp((x*(a*x+b))/(x*(c*x+d)+e), vec3(0.0f), vec3(1.0f));
}
