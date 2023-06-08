#include "GLTF-Mesh.h"
#include "Logger.h"
#include "../Shared/MeshData.h"
#include "../Shared/MathUtils.h"
#include "../Engine/Components.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION
#include <tiny_gltf.h>

static ecs::Entity createEntity(ecs::ComponentManager* mgr, const std::string& name = "") {
	ecs::Entity entity = ecs::CreateEntity();
	mgr->AddComponent<TransformComponent>(entity);
	mgr->AddComponent<NameComponent>(entity).name = name;
	return entity;
}

static void parseNodeHierarchy(tinygltf::Model* model, ecs::ComponentManager* manager, ecs::Entity parent, int nodeIndex) {
	tinygltf::Node& node = model->nodes[nodeIndex];

	// Create entity and write the transforms
	ecs::Entity entity = createEntity(manager, node.name);
	TransformComponent* transform = manager->GetComponent<TransformComponent>(entity);
	if(node.translation.size() > 0)
		transform->position = glm::vec3((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]);
	if (node.rotation.size() > 0)
		transform->rotation = glm::fquat((float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2], (float)node.rotation[3]);
	if (node.scale.size() > 0)
		transform->scale = glm::vec3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]);

	// Update MeshData
	tinygltf::Mesh& mesh = model->meshes[node.mesh];
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	for (auto& primitive : mesh.primitives)
	{
		for (auto& [key, val] : primitive.attributes) {
			tinygltf::Accessor& accessor = model->accessors[val];
			Logger::Debug("Parsing attribute: " + key);
		}
	}
	//MeshRenderer& meshRenderer = manager->AddComponent<MeshRenderer>(entity);

	// Create the scene graph hierarchy
	auto hierarchyCompArr = manager->GetComponentArray<HierarchyComponent>();
	manager->AddComponent<HierarchyComponent>(entity).parent = parent;

	HierarchyComponent* parentHierarchyComponent = manager->GetComponent<HierarchyComponent>(parent);
	if (parentHierarchyComponent == nullptr)
		parentHierarchyComponent = &manager->AddComponent<HierarchyComponent>(parent);

	parentHierarchyComponent->childrens.push_back(entity);

	for (auto child : node.children)
		parseNodeHierarchy(model, manager, entity, child);
}

static void parseScene(tinygltf::Model* model,
	ecs::ComponentManager* manager, 
	tinygltf::Scene* scene, ecs::Entity parent)
{
	ecs::Entity entity = createEntity(manager, scene->name);
	for (auto node : scene->nodes)
		parseNodeHierarchy(model, manager, parent, node);
}

static ecs::Entity createEntity(tinygltf::Model* model, ecs::ComponentManager* manager)
{
	ecs::Entity root = createEntity(manager);
	for (auto& scene : model->scenes) {
		parseScene(model, manager, &scene, root);
	}
	return root;
}

ecs::Entity gltfMesh::loadFile(const std::string& filename, ecs::ComponentManager* manager)
{
	tinygltf::TinyGLTF loader;
	std::string err{};
	std::string warn{};

	tinygltf::Model model;
	bool res = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
	if (!warn.empty())
		Logger::Warn(warn);
	if (!err.empty())
		Logger::Error(err);

	if (!res)
	{
		Logger::Warn("Failed to load models: " + filename);
		return ecs::INVALID_ENTITY;
	}

	ecs::Entity entity = createEntity(&model, manager);
	std::string name = filename.substr(filename.find_last_of("/\\") + 1, filename.size() - 1);
	name = name.substr(0, name.find_last_of('.'));
	manager->GetComponent<NameComponent>(entity)->name = name;

	return entity;
}
