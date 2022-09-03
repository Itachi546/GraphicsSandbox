#include "Scene.h"
#include "Logger.h"
#include "GpuMemoryAllocator.h"
#include "../Shared/MeshData.h"

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
	mEnvMap->CreateFromHDRI("Assets/EnvironmentMap/sunset.hdr");
	mEnvMap->CalculateIrradiance();
	mEnvMap->Prefilter();
	mEnvMap->CalculateBRDFLUT();
}

void Scene::GenerateDrawData(std::vector<DrawData>& out)
{
	auto objectComponentArray = mComponentManager->GetComponentArray<ObjectComponent>();
	auto meshDataComponentArray = mComponentManager->GetComponentArray<MeshDataComponent>();

	for (int i = 0; i < objectComponentArray->GetCount(); ++i)
	{
		DrawData drawData = {};
		const ecs::Entity& entity = objectComponentArray->entities[i];

		auto& object = objectComponentArray->components[i];
		auto& meshData = meshDataComponentArray->components[object.meshId];

		if (meshData.IsRenderable())
		{
			drawData.vertexBuffer = meshData.vertexBuffer;
			drawData.indexBuffer = meshData.indexBuffer;
			drawData.indexCount = meshData.GetNumIndices();

			TransformComponent* transform = mComponentManager->GetComponent<TransformComponent>(entity);
			drawData.worldTransform = transform->world;

			MaterialComponent* material = mComponentManager->GetComponent<MaterialComponent>(entity);
			drawData.material = material;
			out.push_back(std::move(drawData));
		}
	}
}

ecs::Entity Scene::CreateCube(std::string_view name)
{
	ecs::Entity entity = ecs::CreateEntity();

	if (!name.empty())
		mComponentManager->AddComponent<NameComponent>(entity).name = name;

	mComponentManager->AddComponent<TransformComponent>(entity);
	
	ObjectComponent& objectComp = mComponentManager->AddComponent<ObjectComponent>(entity);
	objectComp.meshId = mComponentManager->GetComponentArray<MeshDataComponent>()->GetIndex(mCubeEntity);
	return entity;
}

ecs::Entity Scene::CreatePlane(std::string_view name)
{
	ecs::Entity entity = ecs::CreateEntity();
	if (!name.empty())
		mComponentManager->AddComponent<NameComponent>(entity).name = name;

	mComponentManager->AddComponent<TransformComponent>(entity);

	ObjectComponent& objectComp = mComponentManager->AddComponent<ObjectComponent>(entity);
	objectComp.meshId = mComponentManager->GetComponentArray<MeshDataComponent>()->GetIndex(mPlaneEntity);
	return entity;

}

ecs::Entity Scene::CreateSphere(std::string_view name)
{
	ecs::Entity entity = ecs::CreateEntity();
	if (!name.empty())
		mComponentManager->AddComponent<NameComponent>(entity).name = name;

	mComponentManager->AddComponent<TransformComponent>(entity);

	ObjectComponent& objectComp = mComponentManager->AddComponent<ObjectComponent>(entity);
	objectComp.meshId = mComponentManager->GetComponentArray<MeshDataComponent>()->GetIndex(mSphereEntity);
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

	ecs::Entity entity = ecs::CreateEntity();
	mComponentManager->AddComponent<NameComponent>(entity).name = std::string("teapot");
	mComponentManager->AddComponent<TransformComponent>(entity);
	MeshDataComponent& meshData = mComponentManager->AddComponent<MeshDataComponent>(entity);

	MeshFileHeader header = {};
	inFile.read(reinterpret_cast<char*>(&header), sizeof(MeshFileHeader));
	assert(header.magicNumber == 0x12345678u);

	uint32_t nVertices = header.vertexDataSize / (sizeof(float) * 8);
	uint32_t nIndices = header.indexDataSize / sizeof(uint32_t);
	
	meshData.vertices.resize(nVertices);
	inFile.read(reinterpret_cast<char*>(meshData.vertices.data()), header.vertexDataSize);

	meshData.indices.resize(nIndices);
	inFile.read(reinterpret_cast<char*>(meshData.indices.data()), header.indexDataSize);

	inFile.close();

	gfx::GpuMemoryAllocator* gpuAllocator = gfx::GpuMemoryAllocator::GetInstance();
	gfx::GPUBufferDesc vbDesc;
	vbDesc.bindFlag = gfx::BindFlag::ShaderResource;
	vbDesc.usage = gfx::Usage::Default;
	vbDesc.size = header.vertexDataSize;
	gfx::BufferIndex vb = gpuAllocator->AllocateBuffer(&vbDesc);

	gfx::GPUBufferDesc ibDesc;
	ibDesc.bindFlag = gfx::BindFlag::IndexBuffer;
	ibDesc.usage = gfx::Usage::Default;
	ibDesc.size = header.indexDataSize;
	gfx::BufferIndex ib = gpuAllocator->AllocateBuffer(&ibDesc);
	meshData.CopyDataToBuffer(gpuAllocator, vb, ib);

	ObjectComponent& objectComp = mComponentManager->AddComponent<ObjectComponent>(entity);
	objectComp.meshId = mComponentManager->GetComponentArray<MeshDataComponent>()->GetIndex(entity);
	return entity;
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
	UpdateTransformData();
}

void Scene::SetSize(int width, int height)
{
	if (width > 0 || height > 0)
		mCamera.SetAspect(float(width) / float(height));
}

void Scene::Destroy(ecs::Entity& entity)
{
	ecs::DestroyEntity(mComponentManager.get(), entity);
}

Scene::~Scene()
{
	ecs::DestroyEntity(mComponentManager.get(), mCubeEntity);
	gfx::GpuMemoryAllocator::GetInstance()->FreeMemory();
}

void Scene::UpdateTransformData()
{
	auto transforms = mComponentManager->GetComponentArray<TransformComponent>();
	for (auto& transform : transforms->components)
	{
		if (transform.dirty)
			transform.CalculateWorldMatrix();
	}
}

void Scene::InitializePrimitiveMesh()
{
	// Careful create all entity and add component first and then upload the data
	// to prevent the invalidation of refrence when container is resized

	mCubeEntity = ecs::CreateEntity();
	mComponentManager->AddComponent<NameComponent>(mCubeEntity).name = "cube";
	mComponentManager->AddComponent<MeshDataComponent>(mCubeEntity);

	mPlaneEntity = ecs::CreateEntity();
	mComponentManager->AddComponent<NameComponent>(mPlaneEntity).name = "plane";
	mComponentManager->AddComponent<MeshDataComponent>(mPlaneEntity);

	mSphereEntity = ecs::CreateEntity();
	mComponentManager->AddComponent<NameComponent>(mSphereEntity).name = "sphere";
	mComponentManager->AddComponent<MeshDataComponent>(mSphereEntity);

	unsigned int vertexCount = 0;
	unsigned int indexCount = 0;
	MeshDataComponent* cubeMesh = mComponentManager->GetComponent<MeshDataComponent>(mCubeEntity);
	InitializeCubeMesh(cubeMesh, vertexCount, indexCount);
	MeshDataComponent* planeMesh = mComponentManager->GetComponent<MeshDataComponent>(mPlaneEntity);
	InitializePlaneMesh(planeMesh, vertexCount, indexCount, 32);
	MeshDataComponent* sphereMesh = mComponentManager->GetComponent<MeshDataComponent>(mSphereEntity);
	InitializeSphereMesh(sphereMesh, vertexCount, indexCount);

	gfx::GpuMemoryAllocator* gpuAllocator = gfx::GpuMemoryAllocator::GetInstance();

	gfx::GPUBufferDesc vbDesc;
	vbDesc.bindFlag = gfx::BindFlag::ShaderResource;
	vbDesc.usage = gfx::Usage::Default;
	vbDesc.size = vertexCount * sizeof(Vertex);
	gfx::BufferIndex vb = gpuAllocator->AllocateBuffer(&vbDesc);

	gfx::GPUBufferDesc ibDesc;
	ibDesc.bindFlag = gfx::BindFlag::IndexBuffer;
	ibDesc.usage = gfx::Usage::Default;
	ibDesc.size = indexCount * sizeof(uint32_t);
	gfx::BufferIndex ib = gpuAllocator->AllocateBuffer(&ibDesc);

	cubeMesh->CopyDataToBuffer(gpuAllocator, vb, ib);
	planeMesh->CopyDataToBuffer(gpuAllocator, vb, ib);
	sphereMesh->CopyDataToBuffer(gpuAllocator, vb, ib);
}

void Scene::InitializeCubeMesh(MeshDataComponent* meshComp, unsigned int& accumVertexCount, unsigned int& accumIndexCount)
{
		meshComp->vertices = {
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

		meshComp->indices = {
			0,   1,  2,  0,  2,  3, // Top
			4,   5,  6,  4,  6,  7, // Front
			8,   9, 10,  8, 10, 11, // Right
			12, 13, 14, 12, 14, 15, // Left
			16, 17, 18, 16, 18, 19, // Back
			20, 22, 21, 20, 23, 22, // Bottom
		};

		accumVertexCount += (uint32_t)meshComp->vertices.size();
		accumIndexCount += (uint32_t)meshComp->indices.size();
}

void Scene::InitializePlaneMesh(MeshDataComponent* meshComp, unsigned int& accumVertexCount, unsigned int& accumIndexCount, uint32_t vertexCount)
{
	meshComp->vertices.reserve(vertexCount * vertexCount);
	float dims = (float)vertexCount;

	float stepSize = 2.0f / float(vertexCount);

	std::vector<Vertex>& vertices = meshComp->vertices;
	for (uint32_t y = 0; y < vertexCount; ++y)
	{
		float yPos = y * stepSize - 1.0f;
		for (uint32_t x = 0; x < vertexCount; ++x)
		{
			vertices.push_back(Vertex{ glm::vec3(x * stepSize - 1.0f, 0.0f, yPos),
				glm::vec3(0.0f, 1.0f, 0.0f),
				glm::vec2(x * stepSize, y * stepSize) });
		}
	}

	uint32_t totalGrid = (vertexCount - 1) * (vertexCount - 1);
	uint32_t nIndices =  totalGrid * 6;

	std::vector<unsigned int>& indices = meshComp->indices;
	meshComp->indices.reserve(nIndices);
	for (uint32_t y = 0; y < vertexCount - 1; ++y)
	{
		for (uint32_t x = 0; x < vertexCount - 1; ++x)
		{
			uint32_t i0 = y * vertexCount + x;
			uint32_t i1 = i0 + 1;
			uint32_t i2 = i0 + vertexCount;
			uint32_t i3 = i2 + 1;

			indices.push_back(i0);  indices.push_back(i2); indices.push_back(i1);
			indices.push_back(i2);  indices.push_back(i3); indices.push_back(i1);
		}
	}

	accumVertexCount += (uint32_t)meshComp->vertices.size();
	accumIndexCount += (uint32_t)meshComp->indices.size();
}

void Scene::InitializeSphereMesh(MeshDataComponent* meshComp, unsigned int& accumVertexCount, unsigned int& accumIndexCount)
{
	const int nSector = 32;
	const int nRing = 32;

	// x = r * sinTheta * cosPhi
	// y = r * sinTheta * sinPhi
	// z = r * cosTheta

	auto& vertices = meshComp->vertices;
	vertices.reserve(nSector * nRing);

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
		}
	}

	auto& indices = meshComp->indices;
	uint32_t numIndices = (nSector + 1) * (nRing + 1) * 6;
	indices.reserve(numIndices);

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

		}
	}

	accumVertexCount += (uint32_t)vertices.size();
	accumIndexCount += (uint32_t)indices.size();

}

void Scene::InitializeLights()
{

	mSun = ecs::CreateEntity();
	TransformComponent& transform = mComponentManager->AddComponent<TransformComponent>(mSun);
	transform.position = glm::vec3(5.0f, 5.0f, 5.0f);
	transform.rotation = glm::fquat(glm::vec3(0.0f, -0.35f, glm::pi<float>()));
	LightComponent& light = mComponentManager->AddComponent<LightComponent>(mSun);
	light.color = glm::vec3(1.28f, 1.20f, 0.99f);
	light.intensity = 50.0f;
	light.type = LightType::Point;

	/*
	glm::vec3 positions[] = {
		glm::vec3(-10.0f, 10.0f, 10.0f),
		glm::vec3(10.0f, 10.0f, 10.0f),
		glm::vec3(-10.0f, -10.0f, 10.0f),
        glm::vec3(10.0f, -10.0f, 10.0f)
	};

	glm::vec3 colors[] = {
		glm::vec3(1.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, 1.0f),
		glm::vec3(1.0f, 1.0f, 1.0f)
	};
    
	for (int i = 0; i < 4; ++i)
	{
		ecs::Entity lightEntity = ecs::CreateEntity();
		TransformComponent& transform = mComponentManager->AddComponent<TransformComponent>(lightEntity);
		transform.position = positions[i];
		LightComponent& light = mComponentManager->AddComponent<LightComponent>(lightEntity);
		light.color = colors[i];
		light.intensity = 300.0f;
		light.type = LightType::Point;

	}
	*/

}
