#ifndef GLOBALDATA_H
#define GLOBALDATA_H

struct GlobalData {
   mat4 P;
   mat4 V;
   mat4 VP;

   vec3 cameraPosition;
   float dt;
};

struct LightData
{
    vec3 position;
	float radius;
	vec3 color;
	float type;
};

#endif 