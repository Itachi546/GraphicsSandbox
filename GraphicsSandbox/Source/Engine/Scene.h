#pragma once

#include "ECS.h"
#include "Components.h"
#include "EnvironmentMap.h"
#include "Camera.h"

#include <string_view>
#include <vector>

namespace tinygltf {
	class Model;
	struct Scene;
};

namespace ui {
	class SceneHierarchy;
};

class Scene
{
public:

	Scene(){}

	void Initialize();

	Scene(const Scene&) = delete;
	void operator=(const Scene&) = delete;

	std::shared_ptr<ecs::ComponentManager> GetComponentManager()
	{
		return mComponentManager;
	}

	ecs::Entity GetSun() { return mSun; }
	ecs::Entity CreateMesh(const std::string& filename);
	ecs::Entity CreateLight(std::string_view name = "");

	void Update(float dt);

	void SetSize(int width, int height);

	void SetShowBoundingBox(bool state) { mShowBoundingBox = state; }
	bool GetShowBoundingBox() { return mShowBoundingBox; }

	void SetEnableFrustumCulling(bool state) { mEnableFrustumCulling = state; }

	void Destroy(ecs::Entity entity);

	void AddUI();

	Camera* GetCamera() {
		return &mCamera;
	}

	inline std::unique_ptr<EnvironmentMap>& GetEnvironmentMap() { return mEnvMap; }

	std::vector<ecs::Entity> FindChildren(ecs::Entity entity);

	std::vector<DrawData>& GetDrawDataOpaque() {
		return mOpaqueBatches;
	}

	std::vector<DrawData>& GetDrawDataTransparent() {
		return mTransparentBatches;
	}

	std::vector<DrawData>& GetDrawDataSkinned()
	{
		return mSkinnedBatches;
	}

	void Shutdown();
	virtual ~Scene() = default; 


private:
	// Default Entities
	ecs::Entity mCube;
	ecs::Entity mSphere;
	ecs::Entity mPlane;

	ecs::Entity mSun;

	Camera mCamera;
	bool mShowBoundingBox = false;
	bool mEnableFrustumCulling = true;

	std::shared_ptr<ecs::ComponentManager> mComponentManager;
	std::unique_ptr<EnvironmentMap> mEnvMap;
	std::vector<gfx::BufferHandle> mAllocatedBuffers;

	std::vector<DrawData> mOpaqueBatches;
	std::vector<DrawData> mTransparentBatches;
	std::vector<DrawData> mSkinnedBatches;

	void GenerateDrawData(std::vector<DrawData>& opaque, std::vector<DrawData>& transparent);
	void GenerateSkinnedMeshDrawData(std::vector<DrawData>& opaque, std::vector<DrawData>& transparent);

	void UpdateTransform();
	void UpdateHierarchy();

	void UpdateChildren(ecs::Entity entity, const glm::mat4& parentTransform);

	void DrawBoundingBox();
	void InitializeLights();
	void GenerateMeshData(ecs::Entity entity, const IMeshRenderer* meshRenderer, std::vector<DrawData>& opaque, std::vector<DrawData>& transparent);
	void RemoveChild(ecs::Entity parent, ecs::Entity child);
	void AddChild(ecs::Entity parent, ecs::Entity child);

	// Helpers for gltf mesh
	struct StagingData {
		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;
	} mStagingData;

	void parseMaterial(tinygltf::Model* model, MaterialComponent* component, uint32_t matIndex);
	ecs::Entity parseModel(tinygltf::Model* model);
	ecs::Entity createEntity(const std::string& name = "");
	ecs::Entity parseScene(tinygltf::Model* model, tinygltf::Scene* scene);
	void parseNodeHierarchy(tinygltf::Model* model, ecs::Entity parent, int nodeIndex);
	void updateMeshRenderer(gfx::BufferHandle vertexBuffer, gfx::BufferHandle indexBuffer, ecs::Entity parent);

	// Debug Options
	std::shared_ptr<ui::SceneHierarchy> mUISceneHierarchy;

	friend class Renderer;
};
