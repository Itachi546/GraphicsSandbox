#include "Scene.h"
#include "Logger.h"
#include "GpuMemoryAllocator.h"
#include "../Shared/MeshData.h"
#include "TextureCache.h"
#include "DebugDraw.h"
#include "../Shared/MathUtils.h"
#include "StringConstants.h"

#include <execution>
#include <algorithm>
#include <fstream>

void Scene::Initialize()
{
	mComponentManager = std::make_shared<ecs::ComponentManager>();
	mComponentManager->RegisterComponent<TransformComponent>();
	mComponentManager->RegisterComponent<MeshRenderer>();
	mComponentManager->RegisterComponent<SkinnedMeshRenderer>();
	mComponentManager->RegisterComponent<NameComponent>();
	mComponentManager->RegisterComponent<MaterialComponent>();
	mComponentManager->RegisterComponent<LightComponent>();
	mComponentManager->RegisterComponent<HierarchyComponent>();

	InitializePrimitiveMeshes();
	InitializeLights();

	mEnvMap = std::make_unique<EnvironmentMap>();
	mEnvMap->CreateFromHDRI(StringConstants::HDRI_PATH);
	mEnvMap->CalculateIrradiance();
	mEnvMap->Prefilter();
	mEnvMap->CalculateBRDFLUT();
}

void Scene::GenerateDrawData(std::vector<DrawData>& out)
{
	auto meshRendererComponents = mComponentManager->GetComponentArray<MeshRenderer>();
	auto& frustum = mCamera.mFrustum;

	for (int i = 0; i < meshRendererComponents->GetCount(); ++i)
	{
		const MeshRenderer& meshRenderer = meshRendererComponents->components[i];
		const ecs::Entity entity = meshRendererComponents->entities[i];

		if (meshRenderer.IsRenderable())
		{
			TransformComponent* transform = mComponentManager->GetComponent<TransformComponent>(entity);
			if (transform == nullptr) continue;

			BoundingBox aabb = meshRenderer.boundingBox;
			aabb.Transform(transform->worldMatrix);

			bool isVisible = true;
			if (mEnableFrustumCulling)
			{
				if (!frustum->Intersect(aabb))
					isVisible = false;
			}

			if (isVisible)
			{
				DrawData drawData = {};
				const gfx::BufferView& vertexBuffer = meshRenderer.vertexBuffer;
				const gfx::BufferView& indexBuffer = meshRenderer.indexBuffer;

				drawData.vertexBuffer = vertexBuffer;
				drawData.indexBuffer = indexBuffer;

				drawData.indexCount = static_cast<uint32_t>(meshRenderer.GetIndexCount());
				drawData.worldTransform = transform->worldMatrix;

				auto material = mComponentManager->GetComponent<MaterialComponent>(entity);
				drawData.material = material;
				out.push_back(std::move(drawData));
			}
		}
	}
}

void Scene::DrawBoundingBox()
{
	auto meshRendererCompArray = mComponentManager->GetComponentArray<MeshRenderer>();
	for (int i = 0; i < meshRendererCompArray->GetCount(); ++i)
	{
		const ecs::Entity& entity = meshRendererCompArray->entities[i];
		auto& meshRenderer = meshRendererCompArray->components[i];

		if (meshRenderer.IsRenderable())
		{
			TransformComponent* transform = mComponentManager->GetComponent<TransformComponent>(entity);
			BoundingBox aabb = meshRenderer.boundingBox;

			aabb.Transform(transform->worldMatrix);
			DebugDraw::AddAABB(aabb.min, aabb.max);

		}
	}
}

ecs::Entity Scene::CreateCube(std::string_view name)
{
	ecs::Entity entity = ecs::CreateEntity();
	{
		MeshRenderer& meshRenderer = mComponentManager->AddComponent<MeshRenderer>(entity);
		MeshRenderer* renderer = mComponentManager->GetComponent<MeshRenderer>(mCube);
		meshRenderer = *renderer;
		meshRenderer.SetRenderable(true);
	}

	if (!name.empty())
		mComponentManager->AddComponent<NameComponent>(entity).name = name;

	mComponentManager->AddComponent<TransformComponent>(entity);
	MaterialComponent& material = mComponentManager->AddComponent<MaterialComponent>(entity);
	return entity;
}

ecs::Entity Scene::CreatePlane(std::string_view name)
{
	ecs::Entity entity = ecs::CreateEntity();
	if (!name.empty())
		mComponentManager->AddComponent<NameComponent>(entity).name = name;
	{
		MeshRenderer& meshRenderer = mComponentManager->AddComponent<MeshRenderer>(entity);
		MeshRenderer* renderer = mComponentManager->GetComponent<MeshRenderer>(mPlane);
		meshRenderer = *renderer;
		meshRenderer.SetRenderable(true);
	}

	mComponentManager->AddComponent<TransformComponent>(entity);
	MaterialComponent& material = mComponentManager->AddComponent<MaterialComponent>(entity);

	return entity;

}

ecs::Entity Scene::CreateSphere(std::string_view name)
{
	ecs::Entity entity = ecs::CreateEntity();
	if (!name.empty())
		mComponentManager->AddComponent<NameComponent>(entity).name = name;

	mComponentManager->AddComponent<TransformComponent>(entity);
	MaterialComponent& material = mComponentManager->AddComponent<MaterialComponent>(entity);
	{
		MeshRenderer& meshRenderer = mComponentManager->AddComponent<MeshRenderer>(entity);
		MeshRenderer* renderer = mComponentManager->GetComponent<MeshRenderer>(mSphere);
		meshRenderer = *renderer;
		meshRenderer.SetRenderable(true);
	}
	return entity;
}
/*
void Scene::UpdateObjectData(ecs::Entity parent,
	uint32_t meshComponentIndex, 
	uint32_t nodeIndex,
	const std::vector<Mesh>& meshes,
	const std::vector<MaterialComponent>& materials,
	const std::vector<std::string>& textures)
{
	std::vector<uint32_t> meshIds;
	for (uint32_t i = 0; i < meshes.size(); ++i)
	{
		if (meshes[i].nodeIndex == nodeIndex)
			meshIds.push_back(i);
	}

	if (meshIds.size() > 0)
	{
		auto hierarchyCompArr = mComponentManager->GetComponentArray<HierarchyComponent>();
		for (auto meshId : meshIds)
		{
			auto mesh = meshes[meshId];
			ecs::Entity meshEntity = ecs::CreateEntity();
			mComponentManager->AddComponent<NameComponent>(meshEntity).name = std::string(mesh.meshName);
			auto& transform = mComponentManager->AddComponent<TransformComponent>(meshEntity);
			mComponentManager->AddComponent<HierarchyComponent>(meshEntity).parent = parent;
			std::size_t parentIndex = hierarchyCompArr->GetIndex(parent);
			if (parentIndex == ~0ull)
				mComponentManager->AddComponent<HierarchyComponent>(parent).childrens.push_back(meshEntity);
			else
				hierarchyCompArr->components[parentIndex].childrens.push_back(meshEntity);

			// Object Component
			ObjectComponent& objectComp = mComponentManager->AddComponent<ObjectComponent>(meshEntity);
			objectComp.meshComponentIndex = meshComponentIndex;
			objectComp.meshId = meshId;

			MaterialComponent& material = mComponentManager->AddComponent<MaterialComponent>(meshEntity);
			uint32_t materialIndex = mesh.materialIndex;
			if (materials.size() > 0 && materialIndex != -1)
			{
				material = materials[materialIndex];
				// @TODO Maybe it is not correct to generate mipmap for normal map and 
				// sample it like base mip level. But for now, I don't know any other 
				// ways to remove the normal map aliasing artifacts other than using mipmap.
				if (material.albedoMap != INVALID_TEXTURE)
					material.albedoMap = TextureCache::LoadTexture(textures[material.albedoMap], true);
				if (material.normalMap != INVALID_TEXTURE)
					material.normalMap = TextureCache::LoadTexture(textures[material.normalMap], true);
				if (material.emissiveMap != INVALID_TEXTURE)
					material.emissiveMap = TextureCache::LoadTexture(textures[material.emissiveMap]);
				if (material.metallicMap != INVALID_TEXTURE)
					material.metallicMap = TextureCache::LoadTexture(textures[material.metallicMap], true);
				if (material.roughnessMap != INVALID_TEXTURE)
					material.roughnessMap = TextureCache::LoadTexture(textures[material.roughnessMap], true);
				if (material.ambientOcclusionMap != INVALID_TEXTURE)
					material.ambientOcclusionMap = TextureCache::LoadTexture(textures[material.ambientOcclusionMap], true);
				if (material.opacityMap != INVALID_TEXTURE)
					material.opacityMap = TextureCache::LoadTexture(textures[material.opacityMap]);
			}
		}
	}


}

ecs::Entity Scene::CreateMeshEntity(uint32_t meshComponentIndex,
	uint32_t nodeIndex, 
	const std::vector<Node>& nodes,
	const std::vector<Mesh>& meshes,
	const std::vector<MaterialComponent>& materials, 
	const std::vector<std::string>& textures,
	ecs::Entity parent)
{
	ecs::Entity entity = ecs::CreateEntity();
	const Node& node = nodes[nodeIndex];

	// Name Component
	mComponentManager->AddComponent<NameComponent>(entity).name = node.name;

	// TransformComponent
	TransformComponent& transform = mComponentManager->AddComponent<TransformComponent>(entity);
	transform.defaultMatrix = node.localTransform;

	UpdateObjectData(entity, meshComponentIndex, nodeIndex, meshes, materials, textures);

	if (parent != ecs::INVALID_ENTITY)
	{
		auto hierarchyCompArr = mComponentManager->GetComponentArray<HierarchyComponent>();
		mComponentManager->AddComponent<HierarchyComponent>(entity).parent = parent;
		std::size_t parentIndex = hierarchyCompArr->GetIndex(parent);
		if (parentIndex == ~0ull)
			mComponentManager->AddComponent<HierarchyComponent>(parent).childrens.push_back(entity);
		else
			hierarchyCompArr->components[parentIndex].childrens.push_back(entity);
	}

	return entity;
}
*/
/*
ecs::Entity Scene::TraverseNode(uint32_t root, ecs::Entity parent, uint32_t meshCompIndex, const std::vector<Node>& nodes, std::vector<Mesh>& meshes, std::vector<MaterialComponent>& materials, std::vector<std::string>& textures)
{
	if (root == -1)
		return ecs::INVALID_ENTITY;

	ecs::Entity entity = CreateMeshEntity(meshCompIndex, root, nodes, meshes, materials, textures, parent);

	// Add Child
	const Node& node = nodes[root];
	if(node.firstChild != -1)
		TraverseNode(node.firstChild, entity, meshCompIndex, nodes, meshes, materials, textures);

	// Check sibling
	if(node.nextSibling != -1)
		TraverseNode(node.nextSibling, parent, meshCompIndex, nodes, meshes, materials, textures);

	return entity;
}
/*
ecs::Entity Scene::CreateMesh(const char* file)
{
	std::ifstream inFile(file, std::ios::binary);
	if (!inFile)
	{
		Logger::Warn("Failed to load: " + std::string(file));
		return ecs::INVALID_ENTITY;
	}

	MeshFileHeader header = {};
	inFile.read(reinterpret_cast<char*>(&header), sizeof(MeshFileHeader));
	assert(header.magicNumber == 0x12345678u);

	std::string name = file;

	std::size_t lastPathSplit = name.find_last_of("/\\");
	std::string folder = name.substr(0, lastPathSplit + 1);

	name = name.substr(lastPathSplit + 1, name.size() - 1);
	name = name.substr(0, name.find_first_of('.'));


	// Read Nodes
	uint32_t nNodes = header.nodeCount;
	std::vector<Node> nodes(nNodes);
	inFile.read(reinterpret_cast<char*>(nodes.data()), sizeof(Node) * nNodes);

	// Read Meshes
	MeshDataComponent meshData = {};
	uint32_t nMeshes = header.meshCount;
	std::vector<Mesh>& meshes = meshData.meshes;
	meshes.resize(nMeshes);
	inFile.read(reinterpret_cast<char*>(meshes.data()), sizeof(Mesh) * nMeshes);

	// Read Materials
	uint32_t nMaterial = header.materialCount;
	std::vector<MaterialComponent> materials(nMaterial);
	inFile.read(reinterpret_cast<char*>(materials.data()), sizeof(MaterialComponent) * nMaterial);
	
	// Read Texture Path
	std::vector<std::string> textureFiles(header.textureCount);
	if (header.textureCount > 0)
	{
		for (uint32_t i = 0; i < header.textureCount; ++i)
		{
			uint32_t size = 0;
			inFile.read(reinterpret_cast<char*>(&size), 4);

			std::string& path = textureFiles[i];
			path.resize(size);
			inFile.read(path.data(), size);
			path = folder + path;
		}
	}
	// Read BoundingBox
	meshData.boundingBoxes.resize(nMeshes);
	inFile.read(reinterpret_cast<char*>(meshData.boundingBoxes.data()), sizeof(BoundingBox) * nMeshes);

	//Read VertexData
	uint32_t nVertices = header.vertexDataSize / (sizeof(Vertex));
	uint32_t nIndices = header.indexDataSize / sizeof(uint32_t);

	meshData.vertices.resize(nVertices);
	inFile.seekg(header.dataBlockStartOffset, std::ios::beg);
	inFile.read(reinterpret_cast<char*>(meshData.vertices.data()), header.vertexDataSize);

	meshData.indices.resize(nIndices);
	inFile.read(reinterpret_cast<char*>(meshData.indices.data()), header.indexDataSize);
	inFile.close();

	// Allocate GPU Memory
	gfx::GpuMemoryAllocator* gpuAllocator = gfx::GpuMemoryAllocator::GetInstance();
	gfx::GPUBufferDesc bufferDesc;
	bufferDesc.bindFlag = gfx::BindFlag::ShaderResource;
	bufferDesc.usage = gfx::Usage::Default;
	bufferDesc.size = header.vertexDataSize;
	gfx::GPUBuffer* vb = gpuAllocator->AllocateBuffer(&bufferDesc, &meshData.vbIndex);

	bufferDesc.bindFlag = gfx::BindFlag::IndexBuffer;
	bufferDesc.size = header.indexDataSize;
	gfx::GPUBuffer* ib = gpuAllocator->AllocateBuffer(&bufferDesc, &meshData.ibIndex);
	meshData.CopyDataToBuffer(gpuAllocator, vb, ib);

	// Create Root Node
	const Node& node = nodes[0];
	ecs::Entity entity = ecs::CreateEntity();
	mComponentManager->AddComponent<MeshDataComponent>(entity, meshData);
	auto meshComponentIndex = mComponentManager->GetComponentArray<MeshDataComponent>()->GetIndex(entity);
	mComponentManager->AddComponent<NameComponent>(entity).name = node.name;
	TransformComponent& transform = mComponentManager->AddComponent<TransformComponent>(entity);
	transform.defaultMatrix = node.localTransform;
	UpdateObjectData(entity, (uint32_t)meshComponentIndex, 0, meshes, materials, textureFiles);

	TraverseNode(node.firstChild, entity, (uint32_t)meshComponentIndex, nodes, meshes, materials, textureFiles);
	return entity;
}
*/
ecs::Entity Scene::CreateLight(std::string_view name)
{
	ecs::Entity entity = ecs::CreateEntity();
	mComponentManager->AddComponent<TransformComponent>(entity);
	mComponentManager->AddComponent<LightComponent>(entity);
	mComponentManager->AddComponent<NameComponent>(entity).name = name;
	return entity;
}

void Scene::Update(float dt)
{
	mCamera.Update(dt);
	UpdateTransform();
	UpdateHierarchy();

	if(mShowBoundingBox)
		DrawBoundingBox();
}

void Scene::SetSize(int width, int height)
{
	if (width > 0 && height > 0)
		mCamera.SetAspect(float(width) / float(height));
}

void Scene::Destroy(ecs::Entity entity)
{
	if (mComponentManager->HasComponent<HierarchyComponent>(entity))
	{
		auto hierarchy = mComponentManager->GetComponent<HierarchyComponent>(entity);
		if (hierarchy->parent != ecs::INVALID_ENTITY)
			RemoveChild(hierarchy->parent, entity);

		if (hierarchy->childrens.size() > 0)
		{
			for (const auto& child : hierarchy->childrens)
				Destroy(child);
		}
	}
	ecs::DestroyEntity(mComponentManager.get(), entity);
}

std::vector<ecs::Entity> Scene::FindChildren(ecs::Entity entity)
{
	auto hierarchyComp = mComponentManager->GetComponentArray<HierarchyComponent>();
	std::vector<ecs::Entity> children;

	auto& components = hierarchyComp->components;
	auto& entities = hierarchyComp->entities;
	for (uint32_t i = 0; i < components.size(); ++i)
	{
		if (components[i].parent == entity)
			children.push_back(entities[i]);
	}
	return children;
}

Scene::~Scene()
{
	ecs::Destroy(mComponentManager.get());
	gfx::GpuMemoryAllocator::GetInstance()->FreeMemory();
}

void Scene::UpdateTransform()
{
	auto transforms = mComponentManager->GetComponentArray<TransformComponent>();
	std::for_each(std::execution::par, transforms->components.begin(), transforms->components.end(), [](TransformComponent& transform) {
		transform.CalculateWorldMatrix();
	});

	//for (auto& transform : transforms->components)
	//transform.CalculateWorldMatrix();
}

void Scene::UpdateHierarchy()
{
	auto compArr = mComponentManager->GetComponentArray<HierarchyComponent>();
	auto entities = compArr->entities;
	
	for (uint32_t i = 0; i < compArr->GetCount(); ++i)
	{
		auto& comp = compArr->components[i];
		if (comp.parent == ecs::INVALID_ENTITY && comp.childrens.size() > 0)
			UpdateChildren(entities[i], glm::mat4(1.0f));
	}
}

void Scene::UpdateChildren(ecs::Entity entity, const glm::mat4& parentTransform)
{
	TransformComponent* transform = mComponentManager->GetComponent<TransformComponent>(entity);
	transform->worldMatrix = parentTransform * transform->localMatrix;

	HierarchyComponent* hierarchy = mComponentManager->GetComponent<HierarchyComponent>(entity);
	for (auto& child : hierarchy->childrens)
		UpdateChildren(child, transform->worldMatrix);

}

void Scene::InitializePrimitiveMeshes()
{
	// Careful create all entity and add component first and then upload the data
	// to prevent the invalidation of refrence when container is resized
	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;
	{
		mCube = ecs::CreateEntity();
		mComponentManager->AddComponent<NameComponent>(mCube).name = "Cube";
		MeshRenderer& meshRenderer = mComponentManager->AddComponent<MeshRenderer>(mCube);
		meshRenderer.SetRenderable(false);
		InitializeCubeMesh(meshRenderer);
		vertexCount += (uint32_t)meshRenderer.vertices->size();
		indexCount += (uint32_t)meshRenderer.indices->size();
	}

	{
		mSphere = ecs::CreateEntity();
		mComponentManager->AddComponent<NameComponent>(mSphere).name = "Sphere";
		MeshRenderer& meshRenderer = mComponentManager->AddComponent<MeshRenderer>(mSphere);
		meshRenderer.SetRenderable(false);
		InitializeSphereMesh(meshRenderer);
		vertexCount += (uint32_t)meshRenderer.vertices->size();
		indexCount += (uint32_t)meshRenderer.indices->size();
	}

	{
		mPlane = ecs::CreateEntity();
		mComponentManager->AddComponent<NameComponent>(mPlane).name = "Plane";
		MeshRenderer& meshRenderer = mComponentManager->AddComponent<MeshRenderer>(mPlane);
		meshRenderer.SetRenderable(false);
		InitializePlaneMesh(meshRenderer);
		vertexCount += (uint32_t)meshRenderer.vertices->size();
		indexCount += (uint32_t)meshRenderer.indices->size();
	}

	{
		// Allocate Vertex Buffer
		gfx::GpuMemoryAllocator* gpuAllocator = gfx::GpuMemoryAllocator::GetInstance();
		uint32_t vertexBufferIndex = 0;
		gfx::GPUBufferDesc bufferDesc = { vertexCount * static_cast<uint32_t>(sizeof(Vertex)),
			gfx::Usage::Default,
			gfx::BindFlag::ShaderResource };
		std::shared_ptr<gfx::GPUBuffer> vertexBuffer = gpuAllocator->AllocateBuffer(&bufferDesc, &vertexBufferIndex);

		// Allocate Index Buffer
		uint32_t indexBufferIndex = 0;
		bufferDesc.bindFlag = gfx::BindFlag::IndexBuffer;
		bufferDesc.size = indexCount * static_cast<uint32_t>(sizeof(uint32_t));
		std::shared_ptr<gfx::GPUBuffer> indexBuffer = gpuAllocator->AllocateBuffer(&bufferDesc, &indexBufferIndex);

		MeshRenderer* cubeMesh = mComponentManager->GetComponent<MeshRenderer>(mCube);
		uint32_t vertexOffset = 0;
		uint32_t vertexSize = static_cast<uint32_t>(cubeMesh->vertices->size() * sizeof(Vertex));
		gpuAllocator->CopyToBuffer(&cubeMesh->vertexBuffer, vertexBuffer, cubeMesh->vertices->data(), vertexOffset, vertexSize);
		vertexOffset += vertexSize;

		uint32_t indexOffset = 0;
		uint32_t indexSize = static_cast<uint32_t>(cubeMesh->indices->size() * sizeof(uint32_t));
		gpuAllocator->CopyToBuffer(&cubeMesh->indexBuffer, indexBuffer, cubeMesh->indices->data(), indexOffset, indexSize);
		indexOffset += indexSize;

		MeshRenderer* sphereMesh = mComponentManager->GetComponent<MeshRenderer>(mSphere);
		vertexSize = static_cast<uint32_t>(sphereMesh->vertices->size() * sizeof(Vertex));
		gpuAllocator->CopyToBuffer(&sphereMesh->vertexBuffer, vertexBuffer, sphereMesh->vertices->data(), vertexOffset, vertexSize);
		vertexOffset += vertexSize;

		indexSize = static_cast<uint32_t>(sphereMesh->indices->size() * sizeof(uint32_t));
		gpuAllocator->CopyToBuffer(&sphereMesh->indexBuffer, indexBuffer, sphereMesh->indices->data(), indexOffset, indexSize);
		indexOffset += indexSize;


		MeshRenderer* planeMesh = mComponentManager->GetComponent<MeshRenderer>(mPlane);
		vertexSize = static_cast<uint32_t>(planeMesh->vertices->size() * sizeof(Vertex));
		gpuAllocator->CopyToBuffer(&planeMesh->vertexBuffer, vertexBuffer, planeMesh->vertices->data(), vertexOffset, vertexSize);

		indexSize = static_cast<uint32_t>(planeMesh->indices->size() * sizeof(uint32_t));
		gpuAllocator->CopyToBuffer(&planeMesh->indexBuffer, indexBuffer, planeMesh->indices->data(), indexOffset, indexSize);
	}

}

void Scene::InitializeCubeMesh(MeshRenderer& meshRenderer)
{
	if (!meshRenderer.vertices)
		meshRenderer.vertices = std::make_shared<std::vector<Vertex>>();
	if (!meshRenderer.indices)
		meshRenderer.indices = std::make_shared<std::vector<uint32_t>>();

	*meshRenderer.vertices = {
		Vertex{glm::vec3(-1.0f, +1.0f, +1.0f), glm::vec3(+0.0f, +1.0f, +0.0f), glm::vec2(0.0f)},
		Vertex{glm::vec3(+1.0f, +1.0f, +1.0f), glm::vec3(+0.0f, +1.0f, +0.0f), glm::vec2(0.0f)},
		Vertex{glm::vec3(+1.0f, +1.0f, -1.0f), glm::vec3(+0.0f, +1.0f, +0.0f), glm::vec2(0.0f)},

		Vertex{glm::vec3(-1.0f, +1.0f, -1.0f), glm::vec3(+0.0f, +1.0f, +0.0f), glm::vec2(0.0f)},
		Vertex{glm::vec3(-1.0f, +1.0f, -1.0f), glm::vec3(+0.0f, +0.0f, -1.0f), glm::vec2(0.0f)},
		Vertex{glm::vec3(+1.0f, +1.0f, -1.0f), glm::vec3(+0.0f, +0.0f, -1.0f), glm::vec2(0.0f)},

		Vertex{glm::vec3(+1.0f, -1.0f, -1.0f), glm::vec3(+0.0f, +0.0f, -1.0f), glm::vec2(0.0f)},
		Vertex{glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(+0.0f, +0.0f, -1.0f), glm::vec2(0.0f)},
		Vertex{glm::vec3(+1.0f, +1.0f, -1.0f), glm::vec3(+1.0f, +0.0f, +0.0f), glm::vec2(0.0f)},

		Vertex{glm::vec3(+1.0f, +1.0f, +1.0f), glm::vec3(+1.0f, +0.0f, +0.0f), glm::vec2(0.0f)},
		Vertex{glm::vec3(+1.0f, -1.0f, +1.0f), glm::vec3(+1.0f, +0.0f, +0.0f), glm::vec2(0.0f)},
		Vertex{glm::vec3(+1.0f, -1.0f, -1.0f), glm::vec3(+1.0f, +0.0f, +0.0f), glm::vec2(0.0f)},

		Vertex{glm::vec3(-1.0f, +1.0f, +1.0f), glm::vec3(-1.0f, +0.0f, +0.0f), glm::vec2(0.0f)},
		Vertex{glm::vec3(-1.0f, +1.0f, -1.0f), glm::vec3(-1.0f, +0.0f, +0.0f), glm::vec2(0.0f)},
		Vertex{glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(-1.0f, +0.0f, +0.0f), glm::vec2(0.0f)},

		Vertex{glm::vec3(-1.0f, -1.0f, +1.0f), glm::vec3(-1.0f, +0.0f, +0.0f), glm::vec2(0.0f)},
		Vertex{glm::vec3(+1.0f, +1.0f, +1.0f), glm::vec3(+0.0f, +0.0f, +1.0f), glm::vec2(0.0f)},
		Vertex{glm::vec3(-1.0f, +1.0f, +1.0f), glm::vec3(+0.0f, +0.0f, +1.0f), glm::vec2(0.0f)},

		Vertex{glm::vec3(-1.0f, -1.0f, +1.0f), glm::vec3(+0.0f, +0.0f, +1.0f), glm::vec2(0.0f)},
		Vertex{glm::vec3(+1.0f, -1.0f, +1.0f), glm::vec3(+0.0f, +0.0f, +1.0f), glm::vec2(0.0f)},
		Vertex{glm::vec3(+1.0f, -1.0f, -1.0f), glm::vec3(+0.0f, -1.0f, +0.0f), glm::vec2(0.0f)},

		Vertex{glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(+0.0f, -1.0f, +0.0f), glm::vec2(0.0f)},
		Vertex{glm::vec3(-1.0f, -1.0f, +1.0f), glm::vec3(+0.0f, -1.0f, +0.0f), glm::vec2(0.0f)},
		Vertex{glm::vec3(+1.0f, -1.0f, +1.0f), glm::vec3(+0.0f, -1.0f, +0.0f), glm::vec2(0.0f)},
	};

	*meshRenderer.indices = {
		0,   1,  2,  0,  2,  3, // Top
		4,   5,  6,  4,  6,  7, // Front
		8,   9, 10,  8, 10, 11, // Right
		12, 13, 14, 12, 14, 15, // Left
		16, 17, 18, 16, 18, 19, // Back
		20, 22, 21, 20, 23, 22, // Bottom
	};

	meshRenderer.boundingBox.min = glm::vec3(-1.0f);
	meshRenderer.boundingBox.max = glm::vec3(1.0f);
}

void Scene::InitializePlaneMesh(MeshRenderer& meshRenderer, uint32_t subdivide)
{
	if (!meshRenderer.vertices)
		meshRenderer.vertices = std::make_shared<std::vector<Vertex>>();
	if (!meshRenderer.indices)
		meshRenderer.indices = std::make_shared<std::vector<uint32_t>>();

	float dims = (float)subdivide;
	float stepSize = 2.0f / float(subdivide);

	std::shared_ptr<std::vector<Vertex>> vertices = meshRenderer.vertices;
	for (uint32_t y = 0; y < subdivide; ++y)
	{
		float yPos = y * stepSize - 1.0f;
		for (uint32_t x = 0; x < subdivide; ++x)
		{
			vertices->push_back(Vertex{ glm::vec3(x * stepSize - 1.0f, 0.0f, yPos),
				glm::vec3(0.0f, 1.0f, 0.0f),
				glm::vec2(x * stepSize, y * stepSize) });
		}
	}

	uint32_t totalGrid = (subdivide - 1) * (subdivide - 1);
	uint32_t nIndices =  totalGrid * 6;

	std::shared_ptr<std::vector<unsigned int>> indices = meshRenderer.indices;
	indices->reserve(nIndices);
	for (uint32_t y = 0; y < subdivide - 1; ++y)
	{
		for (uint32_t x = 0; x < subdivide - 1; ++x)
		{
			uint32_t i0 = y * subdivide + x;
			uint32_t i1 = i0 + 1;
			uint32_t i2 = i0 + subdivide;
			uint32_t i3 = i2 + 1;

			indices->push_back(i0);  indices->push_back(i2); indices->push_back(i1);
			indices->push_back(i2);  indices->push_back(i3); indices->push_back(i1);
		}
	}

	meshRenderer.boundingBox.min = glm::vec3(-1.0f, -0.01f, -1.0f);
	meshRenderer.boundingBox.max = glm::vec3(1.0f, 0.01f, 1.0f);
}

void Scene::InitializeSphereMesh(MeshRenderer& meshRenderer)
{
	if (!meshRenderer.vertices)
		meshRenderer.vertices = std::make_shared<std::vector<Vertex>>();
	if (!meshRenderer.indices)
		meshRenderer.indices = std::make_shared<std::vector<uint32_t>>();

	const int nSector = 32;
	const int nRing = 32;

	// x = r * sinTheta * cosPhi
	// y = r * sinTheta * sinPhi
	// z = r * cosTheta

	auto& vertices = meshRenderer.vertices;

	constexpr float pi = glm::pi<float>();
	constexpr float kdTheta = pi / float(nRing);
	constexpr float kdPhi = (pi * 2.0f) / float(nSector);

	for (int s = 0; s <= nRing; ++s)
	{
		float theta = pi * 0.5f - s * kdTheta;
		float cosTheta = static_cast<float>(cos(theta));
		float sinTheta = static_cast<float>(sin(theta));
		for (int r = 0; r <= nSector; ++r)
		{
			float phi = r * kdPhi;
			float x = cosTheta * static_cast<float>(cos(phi));
			float y = sinTheta;
			float z = cosTheta * static_cast<float>(sin(phi)); 
			vertices->push_back(Vertex{ glm::vec3(x, y, z), glm::vec3(x, y, z), glm::vec2(0.0f, 0.0f) });
		}
	}

	auto& indices = meshRenderer.indices;
	uint32_t numIndices = (nSector + 1) * (nRing + 1) * 6;

	for (unsigned int r = 0; r < nRing; ++r)
	{
		for (unsigned int s = 0; s < nSector; ++s)
		{
			unsigned int i0 = r * (nSector + 1) + s;
			unsigned int i1 = i0 + (nSector + 1);

			indices->push_back(i0);
			indices->push_back(i0 + 1);
			indices->push_back(i1);

			indices->push_back(i1);
			indices->push_back(i0 + 1);
			indices->push_back(i1 + 1);
		}
	}
	meshRenderer.boundingBox.min = glm::vec3(-1.0f);
	meshRenderer.boundingBox.max = glm::vec3(1.0f);
}

void Scene::InitializeLights()
{
	mSun = ecs::CreateEntity();
	mComponentManager->AddComponent<NameComponent>(mSun).name = "Sun";
	TransformComponent& transform = mComponentManager->AddComponent<TransformComponent>(mSun);
	transform.position = glm::vec3(0.0f, 1.0f, 0.0f);
	transform.rotation = glm::vec3(-0.967f, 0.021f, 0.675f);
	//transform.rotation = glm::vec3(0.0f, 0.021f, 0.375f);
	LightComponent& light = mComponentManager->AddComponent<LightComponent>(mSun);
	light.color = glm::vec3(1.28f, 1.20f, 0.99f);
	light.intensity = 1.0f;
	light.type = LightType::Directional;
}

void Scene::RemoveChild(ecs::Entity parent, ecs::Entity child)
{
	auto comp = mComponentManager->GetComponent<HierarchyComponent>(parent);
	if (comp->RemoveChild(child))
		Logger::Info("Children Removed of Id: " + std::to_string(child));
	else
		Logger::Warn("No Children of Id: " + std::to_string(child));
}
