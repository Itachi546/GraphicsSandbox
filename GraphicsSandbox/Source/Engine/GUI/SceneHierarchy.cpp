#include "SceneHierarchy.h"
#include "FileDialog.h"
#include "ImGuiService.h"
#include "IconsFontAwesome6.h"

#include "../Engine/Input.h"
#include "../Engine/DebugDraw.h"
#include "../Engine/TextureCache.h"

namespace ui {

	SceneHierarchy::SceneHierarchy(Scene* scene) : mScene(scene),
		mSelected(ecs::INVALID_ENTITY)
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
			//DebugDraw::AddAABB(position + glm::vec3(-0.1f), position + glm::vec3(0.1f));
			uint32_t color = ConvertFloat3ToU32Color(lightComp->color);
			DebugDraw::AddSphere(position, lightComp->radius, color);
		}
	}

	void SceneHierarchy::CreateTreeNode(ecs::Entity entity, std::shared_ptr<ecs::ComponentManager> compMgr)
	{
		ImGui::PushID(entity);
		std::string name = "node";
		if (compMgr->HasComponent<NameComponent>(entity))
			name = compMgr->GetComponent<NameComponent>(entity)->name;

		SkinnedMeshRenderer* meshRenderer = compMgr->GetComponent<SkinnedMeshRenderer>(entity);
		if (meshRenderer)
			CreateSkeletonHierarchy(meshRenderer);

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

	void SceneHierarchy::CreateSkeletonNode(int parent, Skeleton& skeleton)
	{
		ImGui::PushID(parent);
		std::string name = std::string(ICON_FA_BONE) + " " + skeleton.GetJointName(parent);

		std::vector<int> childrens = skeleton.GetBindPose().FindChildren(parent);
		if (childrens.size() > 0)
		{
			bool nodeOpen = ImGui::TreeNodeEx(name.c_str());
			if (nodeOpen) {
				for (uint32_t child : childrens)
					CreateSkeletonNode(child, skeleton);
				ImGui::TreePop();
			}
		}
		else
		{
			ImGui::Indent();
			ImGui::Selectable(name.c_str());
			ImGui::Unindent();
		}
		ImGui::PopID();

	}

	void SceneHierarchy::CreateSkeletonHierarchy(SkinnedMeshRenderer* meshRenderer)
	{
		CreateSkeletonNode(meshRenderer->skeleton.GetRootBone(), meshRenderer->skeleton);
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

				IMeshRenderer* meshRenderer = componentManager->GetComponent<MeshRenderer>(mSelected);
				if (meshRenderer == nullptr)
					meshRenderer = componentManager->GetComponent<SkinnedMeshRenderer>(mSelected);
				if (meshRenderer)
					DrawMeshRendererComponent(meshRenderer);

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
			/*
			if (ImGui::Button(ICON_FA_CIRCLE " Sphere"))
				mSelected = mScene->CreateSphere("Sphere");
			if(ImGui::Button(ICON_FA_CUBE " Cube"))
				mSelected = mScene->CreateCube("Cube");
			if(ImGui::Button(ICON_FA_SQUARE_FULL " Plane"))
				mSelected = mScene->CreatePlane("Plane");
				*/
			if (ImGui::Button(ICON_FA_LIGHTBULB " Light"))
				mSelected = mScene->CreateLight("Light");
			/*
			if (ImGui::Button("Mesh"))
			{
				std::wstring file = FileDialog::Open(L"", mWindow, L"Model(*.sbox)\0*.sbox\0\0");
				if (file.size() > 0)
					mSelected = mScene->CreateMesh(Platform::WStringToString(file).c_str());
			}
			*/
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
			ImGui::Text("LocalPosition: %.2f %.2f %.2f", localPosition.x, localPosition.y, localPosition.z);
			ImGui::Separator();
		}
	}

	void SceneHierarchy::DrawMaterialComponent(MaterialComponent* material)
	{
		if (ImGui::CollapsingHeader("Material Component", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::ColorEdit3("Albedo", &material->albedo[0]);
			ImGui::SliderFloat("Roughness", &material->roughness, 0.0f, 1.0f);
			ImGui::SliderFloat("Metallic", &material->metallic, 0.0f, 1.0f);
			ImGui::ColorEdit3("Emissive", &material->emissive[0]);
			ImGui::SliderFloat("AO", &material->ao, 0.0f, 1.0f);

			const char* blendModes = "ALPHAMODE_NONE\0ALPHAMODE_BLEND\0ALPHAMODE_MASK";
			ImGui::Combo("AlphaMode", &material->alphaMode, blendModes);
			ImGui::SliderFloat("Transparency", &material->transparency, 0.0f, 1.0f);

			auto ShowTexture = [](uint32_t index, std::string textureType) {
				std::string filename = "No File";
				if (index != gfx::INVALID_TEXTURE_ID) {
					filename = TextureCache::GetTextureName(index);
					ui::ImGuiService::GetInstance()->AddImage(gfx::TextureHandle{index}, ImVec2{ 256, 256 });
				}
				ImGui::Text("%s: %s\n", textureType.c_str(), filename.c_str());
			};

			ShowTexture(material->albedoMap, "Albedo");
			ShowTexture(material->normalMap, "Normal");
			ShowTexture(material->metallicMap, "Metallic");
			ShowTexture(material->roughnessMap, "Roughness");
			ShowTexture(material->emissiveMap, "Emissive");
			ShowTexture(material->ambientOcclusionMap, "AO");
			ShowTexture(material->opacityMap, "Opacity");
			ImGui::Separator();
		}
	}

	void SceneHierarchy::DrawLightComponent(LightComponent* light)
	{
		if (ImGui::CollapsingHeader("Light Component", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Enable", &light->enabled);
			ImGui::ColorPicker3("LightColor", &light->color[0]);
			ImGui::SliderFloat("Intensity", &light->intensity, 0.0f, 100.0f);
			ImGui::SliderFloat("Radius", &light->radius, 0.0f, 10.0f);

			const char* lightTypes[] = {
				"Directional",
				"Point"
			};

			int currentItem = (int)light->type;
			if (ImGui::Combo("Light Type", &currentItem, lightTypes, IM_ARRAYSIZE(lightTypes)))
			{
				light->type = (LightType)(currentItem);
			}
			ImGui::Separator();
		}
	}

	void SceneHierarchy::DrawMeshRendererComponent(IMeshRenderer* meshRenderer)
	{
		const std::string title = meshRenderer->IsSkinned() ? "SkinnedMeshRenderer" : "MeshRenderer";
		if (ImGui::CollapsingHeader(title.c_str()))
		{
			static bool isVisible = meshRenderer->IsRenderable();
			ImGui::Checkbox("Visible", &isVisible);
			meshRenderer->SetRenderable(isVisible);

			ImGui::Text("Bounding Box");
			ImGui::DragFloat3("min", &meshRenderer->boundingBox.min[0]);
			ImGui::DragFloat3("max", &meshRenderer->boundingBox.max[0]);
			ImGui::Separator();


			if (meshRenderer->IsSkinned())
			{
				Skeleton& skeleton = static_cast<SkinnedMeshRenderer*>(meshRenderer)->skeleton;
				int poseMode = (int)skeleton.GetPoseMode();
				if (ImGui::Combo("Pose", &poseMode, "Rest\0Bind\0Animated"))
					skeleton.SetPoseMode(static_cast<PoseMode>(poseMode));
			}
		}
	}

	void SceneHierarchy::AddUI()
	{
		auto componentManager = mScene->GetComponentManager();
		if (ImGui::CollapsingHeader("Scene")) {
			if (ImGui::BeginTabBar("Entities"))
			{
				CreateHierarchyTab(componentManager);
				CreateComponentTab(componentManager);
				CreateObjectTab(componentManager);
				ImGui::EndTabBar();
			}
		}
	}
}