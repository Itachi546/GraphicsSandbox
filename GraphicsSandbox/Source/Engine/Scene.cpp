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
	mCubeEntity = ecs::CreateEntity();
	mComponentManager->AddComponent<NameComponent>(mCubeEntity).name = "cube";
	mComponentManager->AddComponent<MeshDataComponent>(mCubeEntity);

	mPlaneEntity = ecs::CreateEntity();
	mComponentManager->AddComponent<NameComponent>(mPlaneEntity).name = "plane";
	mComponentManager->AddComponent<MeshDataComponent>(mPlaneEntity);

	// Careful create all entity and add component first and then upload the data
	// to prevent the invalidation of refrence when container is resized

	MeshDataComponent* cubeMesh = mComponentManager->GetComponent<MeshDataComponent>(mCubeEntity);
	InitializeCubeMesh(cubeMesh);
	MeshDataComponent* planeMesh = mComponentManager->GetComponent<MeshDataComponent>(mPlaneEntity);
	InitializePlaneMesh(planeMesh);

	gfx::GpuMemoryAllocator* gpuAllocator = gfx::GpuMemoryAllocator::GetInstance();

	gfx::GPUBufferDesc vbDesc;
	vbDesc.bindFlag = gfx::BindFlag::ShaderResource;
	vbDesc.usage = gfx::Usage::Default;
	vbDesc.size = (uint32_t)(cubeMesh->vertices.size() + planeMesh->vertices.size()) * sizeof(Vertex);
	gfx::BufferIndex vb = gpuAllocator->AllocateBuffer(&vbDesc);

	gfx::GPUBufferDesc ibDesc;
	ibDesc.bindFlag = gfx::BindFlag::IndexBuffer;
	ibDesc.usage = gfx::Usage::Default;
	ibDesc.size = (uint32_t)(cubeMesh->indices.size() + planeMesh->indices.size()) * sizeof(uint32_t);
	gfx::BufferIndex ib = gpuAllocator->AllocateBuffer(&ibDesc);

	cubeMesh->CopyDataToBuffer(gpuAllocator, vb, ib);
	planeMesh->CopyDataToBuffer(gpuAllocator, vb, ib);
}

void Scene::InitializeCubeMesh(MeshDataComponent* meshComp)
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
}

void Scene::InitializePlaneMesh(MeshDataComponent* meshComp)
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

}
