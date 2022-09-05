#pragma once

#include "../Engine/Scene.h"

class SceneHierarchy
{
public:
	SceneHierarchy(Scene* scene);

	void Draw();

private:
	Scene* mScene;

	ecs::Entity mSelected;

	void CreateTreeNode(ecs::Entity entity, std::shared_ptr<ecs::ComponentManager> compMgr);

	void CreateHierarchyTab(std::shared_ptr<ecs::ComponentManager> mgr);

	void CreateComponentTab(std::shared_ptr<ecs::ComponentManager> mgr);

	void DrawTransformComponent(TransformComponent* transform);

	void DrawMaterialComponent(MaterialComponent* material);
};