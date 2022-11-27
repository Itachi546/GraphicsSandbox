#include "SceneHierarchy.h"
#include "FileDialog.h"
#include "ImGui/imgui.h"

#include "../Engine/Input.h"
#include "../Engine/DebugDraw.h"
#include "../Engine/TextureCache.h"

SceneHierarchy::SceneHierarchy(Scene* scene, Platform::WindowType window) : mScene(scene),
    mSelected(ecs::INVALID_ENTITY),
	mWindow(window)
{
}

void SceneHierarchy::Update(float dt)
{
	auto componentManager = mScene->GetComponentManager();
	if (Input::Down(Input::Key::KEY_DELETE))
	{
		if (mSelected != mScene->GetSun())
		{
			mScene->Destroy(mSelected);
			mSelected = ecs::INVALID_ENTITY;
		}
	}

	auto lightComp = componentManager->GetComponent<LightComponent>(mSelected);
	if (lightComp && lightComp->type == LightType::Point)
	{
		glm::vec3 position = componentManager->GetComponent<TransformComponent>(mSelected)->position;
		DebugDraw::AddAABB(position + glm::vec3(-0.1f), position + glm::vec3(0.1f));
	}
}

void SceneHierarchy::CreateTreeNode(ecs::Entity entity, std::shared_ptr<ecs::ComponentManager> compMgr)
{
	ImGui::PushID(entity);
	std::string name = "node";
	if (compMgr->HasComponent<NameComponent>(entity))
		name = compMgr->GetComponent<NameComponent>(entity)->name;

	HierarchyComponent* hierarchyComponent = compMgr->GetComponent<HierarchyComponent>(entity);

	if (hierarchyComponent && hierarchyComponent->childrens.size() > 0)
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnDoubleClick;
		if (mSelected == entity)
			flags |= ImGuiTreeNodeFlags_Selected;

		bool nodeOpen = ImGui::TreeNodeEx(name.c_str(), flags);
		if (ImGui::IsItemClicked()) mSelected = entity;
		if (nodeOpen) {
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
	ImGui::PopID();
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
			if (component == nullptr)
				CreateTreeNode(entities[i], componentManager);
			else
			{
				if (component->parent == ecs::INVALID_ENTITY)
					CreateTreeNode(entities[i], componentManager);
			}
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

			LightComponent* light = componentManager->GetComponent<LightComponent>(mSelected);
			if (light)
				DrawLightComponent(light);

		}
		else {
			ImGui::Text("No Entity selected");
		}

		ImGui::EndTabItem();
	}
}

void SceneHierarchy::CreateObjectTab(std::shared_ptr<ecs::ComponentManager> mgr)
{
	if (ImGui::BeginTabItem("Create Objects"))
	{
		if (ImGui::Button("Sphere"))
			mSelected = mScene->CreateSphere("Sphere");
		if(ImGui::Button("Cube"))
			mSelected = mScene->CreateCube("Cube");
		if(ImGui::Button("Plane"))
			mSelected = mScene->CreatePlane("Plane");
		if (ImGui::Button("Light"))
			mSelected = mScene->CreateLight("Light");
		if (ImGui::Button("Mesh"))
		{
			std::wstring file = FileDialog::Open(L"", mWindow, L"Model(*.sbox)\0*.sbox\0\0");
			if(file.size() > 0) 
				mSelected = mScene->CreateMesh(Platform::WStringToString(file).c_str());
		}
		ImGui::EndTabItem();
	}
}

void SceneHierarchy::DrawTransformComponent(TransformComponent* transform)
{
	if (ImGui::CollapsingHeader("Transform Component", ImGuiTreeNodeFlags_DefaultOpen))
	{
		transform->dirty = ImGui::DragFloat3("Position", &transform->position[0]);
		glm::vec3 rotation = glm::degrees(glm::eulerAngles(transform->rotation));
		if (ImGui::DragFloat3("Rotation", &rotation[0], 1.0f, -180.0f, 180.0f))
		{
			transform->rotation = glm::quat(glm::radians(rotation));
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

		glm::vec3 localPosition = glm::vec3(transform->defaultMatrix[3]);

		ImGui::Text("%.2f %.2f %.2f", localPosition.x, localPosition.y, localPosition.z);
	}
}

void SceneHierarchy::DrawMaterialComponent(MaterialComponent* material)
{
	if (ImGui::CollapsingHeader("Material Component", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::ColorEdit3("Albedo", &material->albedo[0]);
		ImGui::SliderFloat("Roughness", &material->roughness, 0.0f, 1.0f);
		ImGui::SliderFloat("Metallic", &material->metallic, 0.0f, 1.0f);
		ImGui::SliderFloat("Emissive", &material->emissive, 0.0f, 100.0f);
		ImGui::SliderFloat("AO", &material->ao, 0.0f, 1.0f);

		auto ShowTexture = [](uint32_t index, std::string textureType) {
			std::string filename = "No File";
			if (index != INVALID_TEXTURE)
				filename = TextureCache::GetByIndex(index)->name;
			ImGui::Text("%s: %s\n", textureType.c_str(), filename.c_str());
		};

		ShowTexture(material->albedoMap, "Albedo");
		ShowTexture(material->normalMap, "Normal");
		ShowTexture(material->metallicMap, "Metallic");
		ShowTexture(material->roughnessMap, "Roughness");
		ShowTexture(material->emissiveMap, "Emissive");
		ShowTexture(material->ambientOcclusionMap, "AO");
		ShowTexture(material->opacityMap, "Opacity");
	}
}

void SceneHierarchy::DrawLightComponent(LightComponent* light)
{
	if (ImGui::CollapsingHeader("Light Component", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::ColorPicker3("LightColor", &light->color[0]);
		ImGui::SliderFloat("Intensity", &light->intensity, 0.0f, 100.0f);

		const char* lightTypes[] = {
			"Directional",
			"Point"
		};

		int currentItem = (int)light->type;
		if (ImGui::Combo("Light Type", &currentItem, lightTypes, IM_ARRAYSIZE(lightTypes)))
		{
			light->type = (LightType)(currentItem);
		}
	}
}

void SceneHierarchy::Draw()
{
	auto componentManager = mScene->GetComponentManager();
	ImGui::Begin("Scene");
	if (ImGui::BeginTabBar("Entities"))
	{
		CreateHierarchyTab(componentManager);
		CreateComponentTab(componentManager);
		CreateObjectTab(componentManager);
		ImGui::EndTabBar();
	}
	ImGui::End();
}

