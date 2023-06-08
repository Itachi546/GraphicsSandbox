#pragma once

#include "ECS.h"

#include <string>

namespace gltfMesh {

	ecs::Entity loadFile(const std::string& filename, ecs::ComponentManager* manager);
};