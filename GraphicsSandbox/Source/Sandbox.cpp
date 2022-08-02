#include "Sandbox.h"

struct PositionComponent
{
	glm::vec3 xyz;
};

void TestApplication::Initialize()
{
	Application::Initialize();
	this->mInitialized = true;
}
