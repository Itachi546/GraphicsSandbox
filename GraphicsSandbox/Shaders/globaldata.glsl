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

const int MAX_CASCADES = 5;

struct Cascade {
   mat4 VP;
   vec4 splitDistance;
};

const vec3 cascadeColor[4] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(1.0, 1.0, 0.0)
);
#define ENABLE_CASCADE_DEBUG 0


#endif 