#include "SceneHierarchy.h"

#include "ImGui/imgui.h"

SceneHierarchy::SceneHierarchy(Scene* scene) : mScene(scene), mSelected(ecs::INVALID_ENTITY)
{
}

void SceneHierarchy::CreateTreeNode(ecs::Entity entity, std::shared_ptr<ecs::ComponentManager> compMgr)
{
	std::string name = "node";
	if (compMgr->HasComponent<NameComponent>(entity))
		name = compMgr->GetComponent<NameComponent>(entity)->name;

	HierarchyComponent* hierarchyComponent = compMgr->GetComponent<HierarchyComponent>(entity);
	if (hierarchyComponent && hierarchyComponent->childrens.size() > 0)
	{
		if (ImGui::TreeNode(name.c_str()))
		{
			for (ecs::Entity child : hierarchyComponent->childrens)
				CreateTreeNode(child, compMgr);

			ImGui::TreePop();
		}
	}
	else
	{
		ImGui::Indent();
		bool selected = mSelected == entity;
		if (ImGui::Selectable(name.c_str(), selected))
			mSelected = entity;
		ImGui::Unindent();
	}
}

void SceneHierarchy::CreateHierarchyTab(std::shared_ptr<ecs::ComponentManager> componentManager)
{
	auto entities = componentManager->GetComponentArray<TransformComponent>()->entities;

	if (ImGui::BeginTabItem("Hierarchy"))
	{
		uint32_t index = 0;
		for (uint32_t i = 0; i < entities.size(); ++i)
		{
			HierarchyComponent* component = componentManager->GetComponent<HierarchyComponent>(entities[i]);
			ImGui::PushID(i);

			if (component == nullptr)
				CreateTreeNode(entities[i], componentManager);
			else
			{
				if(component->parent == ecs::INVALID_ENTITY)
					CreateTreeNode(entities[i], componentManager);
			}
			ImGui::PopID();
		}
		ImGui::EndTabItem();
	}
}

void SceneHierarchy::CreateComponentTab(std::shared_ptr<ecs::ComponentManager> componentManager)
{
	if (ImGui::BeginTabItem("Components"))
	{
		if (mSelected) {
			// Name Component
			NameComponent* nameComponent = componentManager->GetComponent<NameComponent>(mSelected);
			ImGui::Text(nameComponent->name.c_str());

			// TransformComponent
			TransformComponent* transform = componentManager->GetComponent<TransformComponent>(mSelected);
			DrawTransformComponent(transform);

			// MaterialCompoenent
			MaterialComponent* material = componentManager->GetComponent<MaterialComponent>(mSelected);
			if (material)
				DrawMaterialComponent(material);
		}
		else {
			ImGui::Text("No Entity selected");
		}

		ImGui::EndTabItem();
	}
}

void SceneHierarchy::DrawTransformComponent(TransformComponent* transform)
{
	transform->dirty = ImGui::DragFloat3("Position", &transform->position[0]);

	glm::vec3 rotation = glm::eulerAngles(transform->rotation);
	if (ImGui::DragFloat3("Rotation", &rotation[0], 0.001f, -3.141592f, 3.141592f))
	{
		transform->rotation = rotation;
		transform->dirty = true;
	}

	static bool uniform = true;
	float scaleFactor = transform->scale.x;
	ImGui::Checkbox("Uniform Scaling", &uniform);
	if (uniform)
	{
		if (ImGui::DragFloat("Scale", &scaleFactor, 0.01f, -1000.0f, 1000.0f))
		{
			transform->scale = { scaleFactor,scaleFactor, scaleFactor };
			transform->dirty = true;
		}
	}
	else
		transform->dirty = ImGui::DragFloat3("Scale", &transform->scale[0], 0.01f, -1000.0f, 1000.0f);
}

void SceneHierarchy::DrawMaterialComponent(MaterialComponent* material)
{
	ImGui::ColorEdit3("Albedo", &material->albedo[0]);
	ImGui::SliderFloat("Roughness", &material->roughness, 0.0f, 1.0f);
	ImGui::SliderFloat("Metallic", &material->metallic, 0.0f, 1.0f);
	ImGui::SliderFloat("Emissive", &material->emissive, 0.0f, 100.0f);
	ImGui::SliderFloat("AO", &material->ao, 0.0f, 1.0f);
}

void SceneHierarchy::Draw()
{
	auto componentManager = mScene->GetComponentManager();
	ImGui::Begin("Scene");
	if (ImGui::BeginTabBar("Entities"))
	{
		CreateHierarchyTab(componentManager);
		CreateComponentTab(componentManager);
		ImGui::EndTabBar();
	}
	ImGui::End();
}

