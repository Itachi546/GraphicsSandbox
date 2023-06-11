#pragma once

#include "../Scene.h"
namespace ui {
	class SceneHierarchy
	{
	public:
		SceneHierarchy(Scene* scene);
		void Update(float dt);
		void AddUI();

	private:
		Scene* mScene;
		ecs::Entity mSelected;

		void CreateTreeNode(ecs::Entity entity, std::shared_ptr<ecs::ComponentManager> compMgr);

		void CreateHierarchyTab(std::shared_ptr<ecs::ComponentManager> mgr);

		void CreateSkeletonNode(int parent, Skeleton& skeleton);

		void CreateSkeletonHierarchy(SkinnedMeshRenderer* meshRenderer);

		void CreateComponentTab(std::shared_ptr<ecs::ComponentManager> mgr);

		void CreateObjectTab(std::shared_ptr<ecs::ComponentManager> mgr);

		void DrawTransformComponent(TransformComponent* transform);

		void DrawMaterialComponent(MaterialComponent* material);

		void DrawLightComponent(LightComponent* light);

		void DrawMeshRendererComponent(IMeshRenderer* meshRenderer);
	};
}