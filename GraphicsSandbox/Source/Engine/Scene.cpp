#include "Scene.h"
#include "Logger.h"

#include "GpuMemoryAllocator.h"
void Scene::Initialize()
{
	mComponentManager = std::make_shared<ecs::ComponentManager>();

	mComponentManager->RegisterComponent<TransformComponent>();
	mComponentManager->RegisterComponent<MeshDataComponent>();
	mComponentManager->RegisterComponent<ObjectComponent>();
	mComponentManager->RegisterComponent<NameComponent>();

	InitializePrimitiveMesh();
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

		drawData.vertexBuffer = meshData.vertexBuffer;
		drawData.indexBuffer =  meshData.indexBuffer;
		drawData.indexCount = meshData.GetNumIndices();

		TransformComponent* transform = mComponentManager->GetComponent<TransformComponent>(entity);
		drawData.worldTransform = transform->world;
		out.push_back(std::move(drawData));
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

void Scene::Update(float dt)
{
	UpdateTransformData();
}

void Scene::Destroy(const ecs::Entity& entity)
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
	InitializePlaneMesh(planeMesh, vertexCount, indexCount);
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

void Scene::InitializePlaneMesh(MeshDataComponent* meshComp, unsigned int& accumVertexCount, unsigned int& accumIndexCount)
{
	meshComp->vertices = {
			Vertex{glm::vec3(-1.0f, 0.0f, +1.0f), glm::vec3(+0.0f, 1.0f, +0.0f), glm::vec2(0.0f)},
			Vertex{glm::vec3(+1.0f, 0.0f, +1.0f), glm::vec3(+0.0f, 1.0f, +0.0f), glm::vec2(0.0f)},
			Vertex{glm::vec3(+1.0f, 0.0f, -1.0f), glm::vec3(+0.0f, 1.0f, +0.0f), glm::vec2(0.0f)},
			Vertex{glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(+0.0f, 1.0f, +0.0f), glm::vec2(0.0f)},
	};

	meshComp->indices = {
		0,   1,  2,  0,  2,  3
	};

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
