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
	mComponentManager->RegisterComponent<MeshDataComponent>();
	mComponentManager->RegisterComponent<ObjectComponent>();
	mComponentManager->RegisterComponent<NameComponent>();
	mComponentManager->RegisterComponent<MaterialComponent>();
	mComponentManager->RegisterComponent<LightComponent>();
	mComponentManager->RegisterComponent<HierarchyComponent>();

	InitializePrimitiveMesh();
	InitializeLights();

	mEnvMap = std::make_unique<EnvironmentMap>();
	mEnvMap->CreateFromHDRI(StringConstants::HDRI_PATH);
	mEnvMap->CalculateIrradiance();
	mEnvMap->Prefilter();
	mEnvMap->CalculateBRDFLUT();
}

void Scene::GenerateDrawData(std::vector<DrawData>& out)
{
	auto objectComponentArray = mComponentManager->GetComponentArray<ObjectComponent>();
	auto meshDataComponentArray = mComponentManager->GetComponentArray<MeshDataComponent>();

	auto frustum = mCamera.GetFrustum();
	for (int i = 0; i < objectComponentArray->GetCount(); ++i)
	{
		const ecs::Entity& entity = objectComponentArray->entities[i];
		auto& object = objectComponentArray->components[i];
		auto& meshData = meshDataComponentArray->components[object.meshComponentIndex];
		if (meshData.IsRenderable())
		{
			TransformComponent* transform = mComponentManager->GetComponent<TransformComponent>(entity);
			BoundingBox aabb = meshData.boundingBoxes[object.meshId];
			aabb.Transform(transform->worldMatrix);

			bool isVisible = true;
			if (mEnableFrustumCulling && !frustum->Intersect(aabb))
				isVisible = false;

			if (isVisible)
			{
				DrawData drawData = {};
				const Mesh* mesh = meshData.GetMesh(object.meshId);
				drawData.vertexBuffer = gfx::BufferView{ meshData.vertexBuffer,
					meshData.vbIndex,
					mesh->vertexOffset,
					static_cast<uint32_t>(mesh->vertexCount * sizeof(Vertex)) };
				drawData.indexBuffer = gfx::BufferView{ meshData.indexBuffer,
					meshData.ibIndex,
					mesh->indexOffset,
					static_cast<uint32_t>(mesh->indexCount * sizeof(uint32_t)) };
				drawData.indexCount = mesh->indexCount;
				drawData.worldTransform = transform->worldMatrix;

				MaterialComponent* material = mComponentManager->GetComponent<MaterialComponent>(entity);
				drawData.material = material;
				out.push_back(std::move(drawData));
			}
		}
	}
}

void Scene::DrawBoundingBox()
{
	auto objectComponentArray = mComponentManager->GetComponentArray<ObjectComponent>();
	auto meshDataComponentArray = mComponentManager->GetComponentArray<MeshDataComponent>();
	for (int i = 0; i < objectComponentArray->GetCount(); ++i)
	{

		const ecs::Entity& entity = objectComponentArray->entities[i];
		auto& object = objectComponentArray->components[i];
		auto& meshData = meshDataComponentArray->components[object.meshComponentIndex];

		if (meshData.IsRenderable())
		{
			TransformComponent* transform = mComponentManager->GetComponent<TransformComponent>(entity);
			BoundingBox aabb =  meshData.boundingBoxes[object.meshId];
			aabb.Transform(transform->worldMatrix);
			DebugDraw::AddAABB(aabb.min, aabb.max);
		}
	}
}

ecs::Entity Scene::CreateCube(std::string_view name)
{
	ecs::Entity entity = ecs::CreateEntity();

	ObjectComponent& objectComp = mComponentManager->AddComponent<ObjectComponent>(entity);
	objectComp.meshComponentIndex = mComponentManager->GetComponentArray<MeshDataComponent>()->GetIndex(mPrimitives);
	objectComp.meshId = mCubeMeshId;

	if (!name.empty())
		mComponentManager->AddComponent<NameComponent>(entity).name = name;

	mComponentManager->AddComponent<TransformComponent>(entity);
	mComponentManager->AddComponent<MaterialComponent>(entity);
	return entity;
}

ecs::Entity Scene::CreatePlane(std::string_view name)
{
	ecs::Entity entity = ecs::CreateEntity();
	if (!name.empty())
		mComponentManager->AddComponent<NameComponent>(entity).name = name;

	mComponentManager->AddComponent<TransformComponent>(entity);
	mComponentManager->AddComponent<MaterialComponent>(entity);
	ObjectComponent& objectComp = mComponentManager->AddComponent<ObjectComponent>(entity);
	objectComp.meshComponentIndex = mComponentManager->GetComponentArray<MeshDataComponent>()->GetIndex(mPrimitives);
	objectComp.meshId = mPlaneMeshId;
	return entity;

}

ecs::Entity Scene::CreateSphere(std::string_view name)
{
	ecs::Entity entity = ecs::CreateEntity();
	if (!name.empty())
		mComponentManager->AddComponent<NameComponent>(entity).name = name;

	mComponentManager->AddComponent<TransformComponent>(entity);
	mComponentManager->AddComponent<MaterialComponent>(entity);

	ObjectComponent& objectComp = mComponentManager->AddComponent<ObjectComponent>(entity);
	objectComp.meshComponentIndex = mComponentManager->GetComponentArray<MeshDataComponent>()->GetIndex(mPrimitives);
	objectComp.meshId = mSphereMeshId;
	return entity;
}

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
	name = name.substr(name.find_last_of('/') + 1, name.size() - 1);
	name = name.substr(0, name.find_first_of('.'));
	ecs::Entity parent = ecs::CreateEntity();
	mComponentManager->AddComponent<NameComponent>(parent).name = name;
	mComponentManager->AddComponent<TransformComponent>(parent);

	MeshDataComponent& meshData = mComponentManager->AddComponent<MeshDataComponent>(parent);
	uint32_t nMeshes = header.meshCount;

	// Read Meshes
	std::vector<Mesh>& meshes = meshData.meshes;
	meshes.resize(nMeshes);
	inFile.read(reinterpret_cast<char*>(meshes.data()), sizeof(Mesh) * nMeshes);

	// Read Materials
	uint32_t nMaterial = header.materialCount;
	std::vector<MaterialComponent> materials(nMaterial);
	inFile.read(reinterpret_cast<char*>(materials.data()), sizeof(MaterialComponent) * nMaterial);
	
	// Read Texture Path
	std::vector<std::string> textureFiles(header.textureCount);
	for (uint32_t i = 0; i < header.textureCount; ++i)
	{
		uint32_t size = 0;
		inFile.read(reinterpret_cast<char*>(&size), 4);

		std::string& path = textureFiles[i];
		path.resize(size);
		inFile.read(path.data(), size);
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

	auto createObjectAndMaterial = [this, &materials, &meshes, &textureFiles](ecs::Entity entity, std::size_t meshComponentIndex, uint32_t meshId) {
		ObjectComponent& objectComp = mComponentManager->AddComponent<ObjectComponent>(entity);
		objectComp.meshComponentIndex = meshComponentIndex;
		objectComp.meshId = meshId;

		MaterialComponent& material = mComponentManager->AddComponent<MaterialComponent>(entity);
		if (materials.size() > 0)
		{
			MaterialComponent meshMat = materials[meshes[meshId].materialIndex];
			// @TODO Maybe it is not correct to generate mipmap for normal map and 
			// sample it like base mip level. But for now, I don't know any other 
			// ways to remove the normal map aliasing artifacts other than using 
			// mipmap.
			if (meshMat.albedoMap != INVALID_TEXTURE)
				meshMat.albedoMap = TextureCache::LoadTexture(textureFiles[meshMat.albedoMap], true);
			if (meshMat.normalMap != INVALID_TEXTURE)
				meshMat.normalMap = TextureCache::LoadTexture(textureFiles[meshMat.normalMap], true);
			if (meshMat.emissiveMap != INVALID_TEXTURE)
				meshMat.emissiveMap = TextureCache::LoadTexture(textureFiles[meshMat.emissiveMap]);
			if (meshMat.metallicMap != INVALID_TEXTURE)
				meshMat.metallicMap = TextureCache::LoadTexture(textureFiles[meshMat.metallicMap], true);
			if (meshMat.roughnessMap != INVALID_TEXTURE)
				meshMat.roughnessMap = TextureCache::LoadTexture(textureFiles[meshMat.roughnessMap], true);
			if (meshMat.ambientOcclusionMap != INVALID_TEXTURE)
				meshMat.ambientOcclusionMap = TextureCache::LoadTexture(textureFiles[meshMat.ambientOcclusionMap], true);
			if (meshMat.opacityMap != INVALID_TEXTURE)
				meshMat.opacityMap = TextureCache::LoadTexture(textureFiles[meshMat.opacityMap]);
			material = { meshMat };
		}
	};

	std::size_t meshComponentIndex = mComponentManager->GetComponentArray<MeshDataComponent>()->GetIndex(parent);
	if (nMeshes == 1)
		createObjectAndMaterial(parent, meshComponentIndex, 0);
	else
	{
		mComponentManager->AddComponent<HierarchyComponent>(parent);
		auto hierarchyCompArr = mComponentManager->GetComponentArray<HierarchyComponent>();
		std::size_t parentIndex = hierarchyCompArr->GetIndex(parent);

		for (uint32_t i = 0; i < nMeshes; ++i)
		{
			ecs::Entity child = ecs::CreateEntity();
			mComponentManager->AddComponent<NameComponent>(child).name = meshes[i].meshName;
			mComponentManager->AddComponent<TransformComponent>(child);
			mComponentManager->AddComponent<HierarchyComponent>(child).parent = parent;
			hierarchyCompArr->components[parentIndex].childrens.push_back(child);
			createObjectAndMaterial(child, meshComponentIndex, i);
		}
	}
	return parent;
}

ecs::Entity Scene::CreateLight(std::string_view name)
{
	ecs::Entity entity = ecs::CreateEntity();
	mComponentManager->AddComponent<TransformComponent>(entity);
	mComponentManager->AddComponent<LightComponent>(entity);
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

void Scene::Destroy(ecs::Entity& entity)
{
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
	ecs::DestroyEntity(mComponentManager.get(), mPrimitives);
	gfx::GpuMemoryAllocator::GetInstance()->FreeMemory();
}

void Scene::UpdateTransform()
{
	auto transforms = mComponentManager->GetComponentArray<TransformComponent>();
	for (auto& transform : transforms->components)
		transform.CalculateWorldMatrix();
}

void Scene::UpdateHierarchy()
{
	auto compArr = mComponentManager->GetComponentArray<HierarchyComponent>();
	auto entities = compArr->entities;
	
	for (uint32_t i = 0; i < compArr->GetCount(); ++i)
	{
		auto& comp = compArr->components[i];
		if (comp.parent == ecs::INVALID_ENTITY && comp.childrens.size() > 0)
			updateChildren(entities[i], glm::mat4(1.0f));
	}
}

void Scene::updateChildren(ecs::Entity entity, const glm::mat4& parentTransform)
{
	TransformComponent* transform = mComponentManager->GetComponent<TransformComponent>(entity);
	transform->worldMatrix = parentTransform * transform->localMatrix;

	HierarchyComponent* hierarchy = mComponentManager->GetComponent<HierarchyComponent>(entity);
	for (auto& child : hierarchy->childrens)
		updateChildren(child, transform->worldMatrix);

}

void Scene::InitializePrimitiveMesh()
{
	// Careful create all entity and add component first and then upload the data
	// to prevent the invalidation of refrence when container is resized
	mPrimitives = ecs::CreateEntity();
	mComponentManager->AddComponent<NameComponent>(mPrimitives).name = "primitives";
	MeshDataComponent& meshComp = mComponentManager->AddComponent<MeshDataComponent>(mPrimitives);

	unsigned int vertexOffset = 0;
	unsigned int indexOffset = 0;
	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;
	{
		InitializeCubeMesh(&meshComp, vertexCount, indexCount);
		meshComp.meshes.push_back(Mesh{ vertexOffset,
			indexOffset,
			vertexCount,
			indexCount,
			0,
			"Cube"});

		vertexOffset += vertexCount * sizeof(Vertex);
		indexOffset += indexCount * sizeof(uint32_t);
		mCubeMeshId = 0;
	}
	{
		vertexCount = 0;
		indexCount = 0;
		InitializePlaneMesh(&meshComp, vertexCount, indexCount, 32);
		meshComp.meshes.push_back(Mesh{ vertexOffset,
			indexOffset,
			vertexCount,
			indexCount,
			0,
			"Plane"});
		vertexOffset += vertexCount * sizeof(Vertex);
		indexOffset += indexCount * sizeof(uint32_t);
		mPlaneMeshId = 1;
	}
	{
		vertexCount = 0;
		indexCount = 0;
		InitializeSphereMesh(&meshComp, vertexCount, indexCount);
		meshComp.meshes.push_back(Mesh{ vertexOffset,
			indexOffset,
			vertexCount,
			indexCount,
			0,
			"Sphere" });
		mSphereMeshId = 2;
		vertexOffset += vertexCount * sizeof(Vertex);
		indexOffset += indexCount * sizeof(uint32_t);
	}

	gfx::GpuMemoryAllocator* gpuAllocator = gfx::GpuMemoryAllocator::GetInstance();

	gfx::GPUBufferDesc bufferDesc = { vertexOffset, gfx::Usage::Default, gfx::BindFlag::ShaderResource };
	gfx::GPUBuffer* vb = gpuAllocator->AllocateBuffer(&bufferDesc, &meshComp.vbIndex);

	uint32_t ibIndex = 0;
	bufferDesc.bindFlag = gfx::BindFlag::IndexBuffer;
	bufferDesc.size = indexOffset;
	gfx::GPUBuffer* ib = gpuAllocator->AllocateBuffer(&bufferDesc, &meshComp.ibIndex);

	meshComp.CopyDataToBuffer(gpuAllocator, vb, ib);
}

void Scene::InitializeCubeMesh(MeshDataComponent* meshComp, unsigned int& vertexCount, unsigned int& indexCount)
{
	meshComp->vertices.insert(meshComp->vertices.end(), {
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
		});

	meshComp->indices.insert(meshComp->indices.end(), {
		0,   1,  2,  0,  2,  3, // Top
		4,   5,  6,  4,  6,  7, // Front
		8,   9, 10,  8, 10, 11, // Right
		12, 13, 14, 12, 14, 15, // Left
		16, 17, 18, 16, 18, 19, // Back
		20, 22, 21, 20, 23, 22, // Bottom
		});

	vertexCount = 24;
	indexCount = 36;
	meshComp->boundingBoxes.emplace_back(glm::vec3(-1.0f), glm::vec3(1.0f));
}

void Scene::InitializePlaneMesh(MeshDataComponent* meshComp, unsigned int& vertexCount, unsigned int& indexCount, uint32_t subdivide)
{
	float dims = (float)subdivide;
	float stepSize = 2.0f / float(subdivide);
	std::vector<Vertex>& vertices = meshComp->vertices;

	for (uint32_t y = 0; y < subdivide; ++y)
	{
		float yPos = y * stepSize - 1.0f;
		for (uint32_t x = 0; x < subdivide; ++x)
		{
			vertices.push_back(Vertex{ glm::vec3(x * stepSize - 1.0f, 0.0f, yPos),
				glm::vec3(0.0f, 1.0f, 0.0f),
				glm::vec2(x * stepSize, y * stepSize) });
			vertexCount++;
		}
	}

	uint32_t totalGrid = (subdivide - 1) * (subdivide - 1);
	uint32_t nIndices =  totalGrid * 6;

	std::vector<unsigned int>& indices = meshComp->indices;
	meshComp->indices.reserve(nIndices);
	for (uint32_t y = 0; y < subdivide - 1; ++y)
	{
		for (uint32_t x = 0; x < subdivide - 1; ++x)
		{
			uint32_t i0 = y * subdivide + x;
			uint32_t i1 = i0 + 1;
			uint32_t i2 = i0 + subdivide;
			uint32_t i3 = i2 + 1;

			indices.push_back(i0);  indices.push_back(i2); indices.push_back(i1);
			indices.push_back(i2);  indices.push_back(i3); indices.push_back(i1);
			indexCount += 6;
		}
	}

	int size = subdivide;
	meshComp->boundingBoxes.emplace_back(glm::vec3(-size, -0.1f, -size), glm::vec3(size, 0.1f, size));
}

void Scene::InitializeSphereMesh(MeshDataComponent* meshComp, unsigned int& vertexCount, unsigned int& indexCount)
{
	const int nSector = 32;
	const int nRing = 32;

	// x = r * sinTheta * cosPhi
	// y = r * sinTheta * sinPhi
	// z = r * cosTheta

	auto& vertices = meshComp->vertices;

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
			vertices.push_back(Vertex{ glm::vec3(x, y, z), glm::vec3(x, y, z), glm::vec2(0.0f, 0.0f) });
			vertexCount++;
		}
	}

	auto& indices = meshComp->indices;
	uint32_t numIndices = (nSector + 1) * (nRing + 1) * 6;

	for (unsigned int r = 0; r < nRing; ++r)
	{
		for (unsigned int s = 0; s < nSector; ++s)
		{
			unsigned int i0 = r * (nSector + 1) + s;
			unsigned int i1 = i0 + (nSector + 1);

			indices.push_back(i0);
			indices.push_back(i0 + 1);
			indices.push_back(i1);

			indices.push_back(i1);
			indices.push_back(i0 + 1);
			indices.push_back(i1 + 1);
			indexCount+=6;
		}
	}
	meshComp->boundingBoxes.emplace_back(glm::vec3(-1.0f), glm::vec3(1.0f));
}

void Scene::InitializeLights()
{
	mSun = ecs::CreateEntity();
	mComponentManager->AddComponent<NameComponent>(mSun).name = "Sun";
	TransformComponent& transform = mComponentManager->AddComponent<TransformComponent>(mSun);
	transform.position = glm::vec3(0.0f, 1.0f, 0.0f);
	transform.rotation = glm::vec3(-0.967f, 0.021f, 0.675f);
	LightComponent& light = mComponentManager->AddComponent<LightComponent>(mSun);
	light.color = glm::vec3(1.28f, 1.20f, 0.99f);
	light.intensity = 1.0f;
	light.type = LightType::Directional;
}
