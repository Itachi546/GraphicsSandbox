#include "Scene.h"
#include "Logger.h"
#include "MeshData.h"
#include "TextureCache.h"
#include "DebugDraw.h"
#include "MathUtils.h"
#include "StringConstants.h"
#include "GUI/ImGuiService.h"
#include "GUI/SceneHierarchy.h"
#include "GLTF-Mesh.h"
#include "EventDispatcher.h"

#include <meshoptimizer.h>
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

	InitializeLights();

	LoadEnvMap(StringConstants::HDRI_PATH);

	// Initialize debug UI
	mUISceneHierarchy = std::make_shared<ui::SceneHierarchy>(this);

	initializePrimitiveMesh();
}

void Scene::GenerateMeshData(ecs::Entity entity, IMeshRenderer* meshRenderer, std::vector<DrawData>& opaque, std::vector<DrawData>& transparent)
{
	if (meshRenderer->IsRenderable())
	{
		TransformComponent* transform = mComponentManager->GetComponent<TransformComponent>(entity);
		if (transform == nullptr) return;

		BoundingBox aabb = meshRenderer->boundingBox;
		aabb.Transform(transform->worldMatrix);

		auto& [min, max] = aabb;
		const glm::vec3 halfExtent = (max - min) * 0.5f;
		glm::vec3 center = min + halfExtent;

		float radius = glm::compMax(glm::abs(halfExtent)) * sqrtf(3.0f);
		meshRenderer->boundingSphere = glm::vec4(center.x, center.y, center.z, radius);

		DrawData drawData = {};
		const gfx::BufferView vertexBuffer = meshRenderer->vertexBuffer;
		const gfx::BufferView indexBuffer = meshRenderer->indexBuffer;

		drawData.vertexBuffer = vertexBuffer;
		drawData.indexBuffer = indexBuffer;
		drawData.entity = entity;
		drawData.indexCount = static_cast<uint32_t>(meshRenderer->GetIndexCount());
		drawData.worldTransform = transform->worldMatrix;
		drawData.elmSize = meshRenderer->IsSkinned() ? sizeof(AnimatedVertex) : sizeof(Vertex);
		drawData.meshletBuffer = meshRenderer->meshletBuffer;
		drawData.meshletTriangleBuffer = meshRenderer->meshletTriangleBuffer;
		drawData.meshletVertexBuffer = meshRenderer->meshletVertexBuffer;
		drawData.meshletCount = meshRenderer->meshletCount;
		drawData.boundingSphere = meshRenderer->boundingSphere;
		drawData.meshletOffset = meshRenderer->meshletOffset;

		MaterialComponent* material = mComponentManager->GetComponent<MaterialComponent>(entity);
		drawData.material = material;

		if (material->IsTransparent())
			transparent.push_back(std::move(drawData));
		else
			opaque.push_back(std::move(drawData));
	}
}

void Scene::GenerateDrawData(std::vector<DrawData>& opaque, std::vector<DrawData>& transparent)
{
	auto meshRendererComponents = mComponentManager->GetComponentArray<MeshRenderer>();
	auto& frustum = mCamera.mFrustum;

	for (int i = 0; i < meshRendererComponents->GetCount(); ++i)
	{
		MeshRenderer& meshRenderer = meshRendererComponents->components[i];
		const ecs::Entity entity = meshRendererComponents->entities[i];
		GenerateMeshData(entity, &meshRenderer, opaque, transparent);
	}
}

void Scene::GenerateSkinnedMeshDrawData(std::vector<DrawData>& opaque, std::vector<DrawData>& transparent)
{
	// @TODO : Temporary only to visualize bones
	auto skinnedMeshRendererComponents = mComponentManager->GetComponentArray<SkinnedMeshRenderer>();
	for (int i = 0; i < skinnedMeshRendererComponents->GetCount(); ++i)
	{
		SkinnedMeshRenderer& meshRenderer = skinnedMeshRendererComponents->components[i];
		const ecs::Entity entity = skinnedMeshRendererComponents->entities[i];
		GenerateMeshData(entity, &meshRenderer, opaque, transparent);
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
			glm::vec3 center = meshRenderer.boundingSphere;
			float radius = meshRenderer.boundingSphere.w;
			DebugDraw::AddSphere(center, meshRenderer.boundingSphere.w);
		}
	}
}


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
	
	mUISceneHierarchy->Update(dt);
	if(mShowBoundingBox)
		DrawBoundingBox();

	if (queuedHDR.length() > 0) {
		LoadEnvMap(queuedHDR);
		queuedHDR = "";
		EventDispatcher::DispatchEvent(Event(EventType::CubemapChanged));
	}
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

void Scene::AddUI()
{
	ImGui::Checkbox("Show AABB", &mShowBoundingBox);
	ImGui::Separator();
	mUISceneHierarchy->AddUI();
	/*
	LightComponent* light = mComponentManager->GetComponent<LightComponent>(mSun);
	TransformComponent* transform = mComponentManager->GetComponent<TransformComponent>(mSun);
	if (ImGui::CollapsingHeader("Directional Light"))
	{
		ImGui::PushID("dirlight");
		ImGui::SliderFloat3("rotation", &transform->rotation[0], -glm::pi<float>(), glm::pi<float>());
		ImGui::SliderFloat("intensity", &light->intensity, 0.0f, 4.0f);
		ImGui::ColorPicker3("color", &light->color[0]);
		ImGui::PopID();
	}*/
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

void Scene::Shutdown()
{
	gfx::GraphicsDevice* device = gfx::GetDevice();

	for (gfx::BufferHandle buffer : mAllocatedBuffers)
		device->Destroy(buffer);

	mEnvMap->Shutdown();
	ecs::Destroy(mComponentManager.get());
}

void Scene::UpdateTransform()
{
	auto transforms = mComponentManager->GetComponentArray<TransformComponent>();
	std::for_each(std::execution::par, transforms->components.begin(), transforms->components.end(), [](TransformComponent& transform) {
		transform.CalculateWorldMatrix();
	});

	auto skinnedMeshComp = mComponentManager->GetComponentArray<SkinnedMeshRenderer>();
	std::for_each(std::execution::par, skinnedMeshComp->components.begin(), skinnedMeshComp->components.end(), [](SkinnedMeshRenderer& skinnedMesh) {
		skinnedMesh.skeleton.GetAnimatedPose().UpdateMatrixPallete();
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

void Scene::InitializeLights()
{
	mSun = ecs::CreateEntity();
	mComponentManager->AddComponent<NameComponent>(mSun).name = "Sun";
	TransformComponent& transform = mComponentManager->AddComponent<TransformComponent>(mSun);
	transform.position = glm::vec3(0.0f, 1.0f, 0.0f);
	transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
	//transform.rotation = glm::vec3(0.0f, 0.021f, 0.375f);
	LightComponent& light = mComponentManager->AddComponent<LightComponent>(mSun);
	light.color = glm::vec3(1.28f, 1.20f, 0.99f);
	light.intensity = 1.0f;
	light.type = LightType::Directional;

}

ecs::Entity Scene::createEntity(const std::string& name) {
	ecs::Entity entity = ecs::CreateEntity();
	mComponentManager->AddComponent<TransformComponent>(entity);
	mComponentManager->AddComponent<NameComponent>(entity).name = name.length() > 0 ? name : "unnamed";
	return entity;
}

void Scene::parseMaterial(tinygltf::Model* model, MaterialComponent* component, uint32_t matIndex) {
	if (matIndex == -1)
		return;

	tinygltf::Material& material = model->materials[matIndex];
	component->alphaCutoff = (float)material.alphaCutoff;

	if (material.alphaMode == "BLEND")
		component->alphaMode = ALPHAMODE_BLEND;
	else if (material.alphaMode == "MASK")
		component->alphaMode = ALPHAMODE_MASK;

	// Parse Material value
	tinygltf::PbrMetallicRoughness& pbr = material.pbrMetallicRoughness;
	std::vector<double>& baseColor = pbr.baseColorFactor;
	component->albedo = glm::vec4((float)baseColor[0], (float)baseColor[1], (float)baseColor[2], (float)baseColor[3]);
	component->transparency = (float)baseColor[3];
	component->metallic = (float)pbr.metallicFactor;
	component->roughness = (float)pbr.roughnessFactor;
	std::vector<double>& emissiveColor = material.emissiveFactor;
	component->emissive = glm::vec3((float)emissiveColor[0], (float)emissiveColor[1], (float)emissiveColor[2]);

	// Parse Material texture
	auto loadTexture = [&](uint32_t index) {
		tinygltf::Texture& texture = model->textures[index];
		tinygltf::Image& image = model->images[texture.source];
		const std::string& name = image.uri.length() == 0 ? image.name : image.uri;
		return TextureCache::LoadTexture(name, image.width, image.height, image.image.data(), image.component, true);
	};
	

	if (pbr.baseColorTexture.index >= 0)
		component->albedoMap = loadTexture(pbr.baseColorTexture.index);

	if (pbr.metallicRoughnessTexture.index >= 0) 
		component->metallicMap = component->roughnessMap = loadTexture(pbr.metallicRoughnessTexture.index);

	if (material.normalTexture.index >= 0)
		component->normalMap = loadTexture(material.normalTexture.index);

	if (material.occlusionTexture.index >= 0)
		component->ambientOcclusionMap = loadTexture(material.occlusionTexture.index);

	if (material.emissiveTexture.index >= 0)
		component->emissiveMap = loadTexture(material.emissiveTexture.index);
}

void Scene::parseMesh(tinygltf::Model* model, tinygltf::Mesh& mesh, ecs::Entity parent) {
	for (auto& primitive : mesh.primitives)
	{
		// Parse position
		const tinygltf::Accessor& positionAccessor = model->accessors[primitive.attributes["POSITION"]];
		float* positions = (float*)gltfMesh::getBufferPtr(model, positionAccessor);
		uint32_t numPosition = (uint32_t)positionAccessor.count;

		// Parse normals
		float* normals = nullptr;
		auto normalAttributes = primitive.attributes.find("NORMAL");
		if (normalAttributes != primitive.attributes.end()) {
			const tinygltf::Accessor& normalAccessor = model->accessors[normalAttributes->second];
			assert(numPosition == normalAccessor.count);
			normals = (float*)gltfMesh::getBufferPtr(model, normalAccessor);
		}

		// Parse tangents
		float* tangents = nullptr;
		auto tangentAttributes = primitive.attributes.find("TANGENT");
		if (tangentAttributes != primitive.attributes.end()) {
			const tinygltf::Accessor& tangentAccessor = model->accessors[normalAttributes->second];
			assert(numPosition == tangentAccessor.count);
			tangents = (float*)gltfMesh::getBufferPtr(model, tangentAccessor);
		}

		// Parse UV
		float* uvs = nullptr;
		auto uvAttributes = primitive.attributes.find("TEXCOORD_0");
		if (uvAttributes != primitive.attributes.end()) {
			const tinygltf::Accessor& uvAccessor = model->accessors[uvAttributes->second];
			assert(numPosition == uvAccessor.count);
			uvs = (float*)gltfMesh::getBufferPtr(model, uvAccessor);
		}


		uint32_t vertexOffset = (uint32_t)(mStagingData.vertices.size() * sizeof(Vertex));
		uint32_t indexOffset = (uint32_t)(mStagingData.indices.size() * sizeof(uint32_t));

		std::vector<Vertex> vertices(numPosition);
		for (uint32_t i = 0; i < numPosition; ++i) {
			Vertex& vertex = vertices[i];

			vertex.px = positions[i * 3 + 0];
			vertex.py = positions[i * 3 + 1];
			vertex.pz = positions[i * 3 + 2];

			glm::vec3 n = glm::vec3(0.0f, 1.0f, 0.0f);
			if (normals)
			{
				n = glm::vec3(normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]);
				vertex.nx = uint8_t(n.x * 127.0f + 127.0f);
				vertex.ny = uint8_t(n.y * 127.0f + 127.0f);
				vertex.nz = uint8_t(n.z * 127.0f + 127.0f);
			}

			if (uvs)
			{
				vertex.ux = uvs[i * 2 + 0];
				vertex.uy = 1.0f - uvs[i * 2 + 1];
			}


			if (tangents) {
				glm::vec3 t = glm::vec3(tangents[i * 3 + 0], tangents[i * 3 + 1], tangents[i * 3 + 2]);
				glm::vec3 bt = glm::normalize(glm::cross(n, t));

				vertex.tx = uint8_t(t.x * 127.0f + 127.0f);
				vertex.ty = uint8_t(t.y * 127.0f + 127.0f);
				vertex.tz = uint8_t(t.z * 127.0f + 127.0f);

				vertex.bx = uint8_t(bt.x * 127.0f + 127.0f);
				vertex.by = uint8_t(bt.y * 127.0f + 127.0f);
				vertex.bz = uint8_t(bt.z * 127.0f + 127.0f);
			}
		}

		const tinygltf::Accessor& indicesAccessor = model->accessors[primitive.indices];
		uint16_t* indicesPtr = (uint16_t*)gltfMesh::getBufferPtr(model, indicesAccessor);
		uint32_t indexCount = (uint32_t)indicesAccessor.count;
		std::vector<uint32_t> indices(indicesPtr, indicesPtr + indexCount);

		// Generate Meshlets
		const std::size_t maxMeshlets = meshopt_buildMeshletsBound(indexCount, MAX_MESHLET_VERTICES, MAX_MESHLET_TRIANGLES);
		std::vector<meshopt_Meshlet> localMeshlets(maxMeshlets);

		std::vector<uint32_t> meshletVertices(maxMeshlets * MAX_MESHLET_VERTICES);
		std::vector<uint8_t> meshletTriangles(maxMeshlets * MAX_MESHLET_TRIANGLES * 3);

		std::size_t meshletCount = meshopt_buildMeshlets(localMeshlets.data(),
			meshletVertices.data(),
			meshletTriangles.data(),
			indices.data(),
			indexCount,
			(float*)vertices.data(),
			vertices.size(),
			sizeof(Vertex),
			MAX_MESHLET_VERTICES,
			MAX_MESHLET_TRIANGLES,
			0.0f);

		std::string name = mesh.name.length() > 0 ? mesh.name : "unnamed";
		ecs::Entity child = createEntity("submesh_" + name);

		uint32_t meshletVertexOffset = (uint32_t)mStagingData.meshletVertices.size();
		uint32_t meshletTriangleOffset = (uint32_t)mStagingData.meshletTriangles.size();
		uint32_t meshletOffset = (uint32_t)mStagingData.meshlets.size();
		for (uint32_t i = 0; i < meshletCount; ++i) {

			mStagingData.meshlets.emplace_back(Meshlet{
				meshletVertexOffset + localMeshlets[i].vertex_offset,
				localMeshlets[i].vertex_count,
				meshletTriangleOffset + localMeshlets[i].triangle_offset,
				localMeshlets[i].triangle_count,
				glm::vec4(0.0f)
				});
		}

		// Add to the staging data
		mStagingData.vertices.insert(mStagingData.vertices.end(), vertices.begin(), vertices.end());
		mStagingData.indices.insert(mStagingData.indices.end(), indices.begin(), indices.end());

		std::size_t meshletVertexCount = MAX_MESHLET_VERTICES * meshletCount;
		std::size_t meshletTriangleCount = MAX_MESHLET_TRIANGLES * meshletCount * 3;
		mStagingData.meshletVertices.insert(mStagingData.meshletVertices.end(), meshletVertices.begin(), meshletVertices.begin() + meshletVertexCount);
		mStagingData.meshletTriangles.insert(mStagingData.meshletTriangles.end(), meshletTriangles.begin(), meshletTriangles.begin() + meshletTriangleCount);

		MeshRenderer& meshRenderer = mComponentManager->AddComponent<MeshRenderer>(child);
		meshRenderer.vertexBuffer.byteOffset = vertexOffset;
		meshRenderer.vertexBuffer.byteLength = (uint32_t)(vertices.size() * sizeof(Vertex));
		meshRenderer.indexBuffer.byteOffset = indexOffset;
		meshRenderer.indexBuffer.byteLength = (uint32_t)(indexCount * sizeof(uint32_t));
		meshRenderer.meshletCount = (uint32_t)meshletCount;
		meshRenderer.meshletOffset = meshletOffset;

		glm::vec3 minExtent = glm::vec3(positionAccessor.minValues[0], positionAccessor.minValues[1], positionAccessor.minValues[2]);
		glm::vec3 maxExtent = glm::vec3(positionAccessor.maxValues[0], positionAccessor.maxValues[1], positionAccessor.maxValues[2]);

		meshRenderer.boundingBox.min = minExtent;
		meshRenderer.boundingBox.max = maxExtent;

		MaterialComponent& material = mComponentManager->AddComponent<MaterialComponent>(child);
		parseMaterial(model, &material, primitive.material);
		AddChild(parent, child);
	}
}

void Scene::parseNodeHierarchy(tinygltf::Model* model, ecs::Entity parent, int nodeIndex) {
	tinygltf::Node& node = model->nodes[nodeIndex];

	// Create entity and write the transforms
	ecs::Entity entity = createEntity(node.name);
	TransformComponent* transform = mComponentManager->GetComponent<TransformComponent>(entity);
	if (node.translation.size() > 0)
		transform->position = glm::vec3((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]);
	if (node.rotation.size() > 0)
		transform->rotation = glm::fquat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]);
	if (node.scale.size() > 0)
		transform->scale = glm::vec3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]);

	// Update MeshData
	if (node.mesh >= 0) {
		tinygltf::Mesh& mesh = model->meshes[node.mesh];
		parseMesh(model, mesh, entity);
	}

	// Create the scene graph hierarchy
	AddChild(parent, entity);

	for (auto child : node.children)
		parseNodeHierarchy(model, entity, child);
}

ecs::Entity Scene::parseScene(tinygltf::Model* model,
	tinygltf::Scene* scene)
{
	ecs::Entity entity = createEntity(scene->name);
	for (auto node : scene->nodes)
		parseNodeHierarchy(model, entity, node);
	return entity;
}

ecs::Entity Scene::parseModel(tinygltf::Model* model)
{
	ecs::Entity root = createEntity();
	for (auto& scene : model->scenes) {
		ecs::Entity entity = parseScene(model, &scene);
		AddChild(root, entity);
	}
	return root;
}

void Scene::updateMeshRenderer(gfx::BufferHandle vertexBuffer,
	gfx::BufferHandle indexBuffer,
	gfx::BufferHandle meshletBuffer,
	gfx::BufferHandle meshletVertexBuffer,
	gfx::BufferHandle meshletTriangleBuffer,
	ecs::Entity entity)
{
	auto meshRenderer = mComponentManager->GetComponent<MeshRenderer>(entity);
	if (meshRenderer) {
		meshRenderer->vertexBuffer.buffer = vertexBuffer;
		meshRenderer->indexBuffer.buffer = indexBuffer;
		meshRenderer->meshletBuffer = meshletBuffer;
		meshRenderer->meshletVertexBuffer = meshletVertexBuffer;
		meshRenderer->meshletTriangleBuffer = meshletTriangleBuffer;
	}

	auto hierarchyComponent = mComponentManager->GetComponent<HierarchyComponent>(entity);
	if (hierarchyComponent) {
		for (auto child : hierarchyComponent->childrens)
			updateMeshRenderer(vertexBuffer, indexBuffer, meshletBuffer, meshletVertexBuffer, meshletTriangleBuffer, child);
	}
}

ecs::Entity Scene::CreateMesh(const std::string& filename)
{
	Logger::Debug("Loading Mesh: " + filename);
	tinygltf::Model model;
	if (gltfMesh::loadFile(filename, &model))
	{
		gfx::GraphicsDevice* device = gfx::GetDevice();
		ecs::Entity entity = parseModel(&model);
		std::string name = filename.substr(filename.find_last_of("/\\") + 1, filename.size() - 1);
		name = name.substr(0, name.find_last_of('.'));

		mComponentManager->GetComponent<NameComponent>(entity)->name = name;

		gfx::GPUBufferDesc bufferDesc;
		bufferDesc.bindFlag = gfx::BindFlag::ShaderResource;
		bufferDesc.usage = gfx::Usage::Default;
		bufferDesc.size = static_cast<uint32_t>(mStagingData.vertices.size() * sizeof(Vertex));

		// Vertex Buffer
		gfx::BufferHandle vertexBuffer = device->CreateBuffer(&bufferDesc);
		mAllocatedBuffers.push_back(vertexBuffer);
		device->CopyToBuffer(vertexBuffer, mStagingData.vertices.data(), 0, bufferDesc.size);

		// Meshlet buffer
		bufferDesc.size = static_cast<uint32_t>(mStagingData.meshlets.size() * sizeof(Meshlet));
		gfx::BufferHandle meshletBuffer = device->CreateBuffer(&bufferDesc);
		mAllocatedBuffers.push_back(meshletBuffer);
		device->CopyToBuffer(meshletBuffer, mStagingData.meshlets.data(), 0, bufferDesc.size);

		// Meshlet vertex buffer
		bufferDesc.size = static_cast<uint32_t>(mStagingData.meshletVertices.size() * sizeof(uint32_t));
		gfx::BufferHandle meshletVertexBuffer = device->CreateBuffer(&bufferDesc);
		mAllocatedBuffers.push_back(meshletVertexBuffer);
		device->CopyToBuffer(meshletVertexBuffer, mStagingData.meshletVertices.data(), 0, bufferDesc.size);

		// Meshlet index buffer
		bufferDesc.size = static_cast<uint32_t>(mStagingData.meshletTriangles.size() * sizeof(uint8_t));
		gfx::BufferHandle meshletTriangleBuffer = device->CreateBuffer(&bufferDesc);
		mAllocatedBuffers.push_back(meshletTriangleBuffer);
		device->CopyToBuffer(meshletTriangleBuffer, mStagingData.meshletTriangles.data(), 0, bufferDesc.size);

		bufferDesc.bindFlag = gfx::BindFlag::IndexBuffer;
		bufferDesc.size = static_cast<uint32_t>(mStagingData.indices.size() * sizeof(uint32_t));

		gfx::BufferHandle indexBuffer = device->CreateBuffer(&bufferDesc);
		mAllocatedBuffers.push_back(indexBuffer);
		device->CopyToBuffer(indexBuffer, mStagingData.indices.data(), 0, bufferDesc.size);

		// Update all the mesh component recursively
		updateMeshRenderer(vertexBuffer, indexBuffer, meshletBuffer, meshletVertexBuffer, meshletTriangleBuffer, entity);

		// clear the staging data
		mStagingData.clear();

		return entity;
	}
	return ecs::INVALID_ENTITY;
}


void Scene::RemoveChild(ecs::Entity parent, ecs::Entity child)
{
	auto comp = mComponentManager->GetComponent<HierarchyComponent>(parent);
	if (comp->RemoveChild(child))
		Logger::Info("Children Removed of Id: " + std::to_string(child));
	else
		Logger::Warn("No Children of Id: " + std::to_string(child));
}

void Scene::AddChild(ecs::Entity parent, ecs::Entity child)
{
	// Create the scene graph hierarchy
	mComponentManager->AddComponent<HierarchyComponent>(child).parent = parent;

	HierarchyComponent* parentHierarchyComponent = mComponentManager->GetComponent<HierarchyComponent>(parent);
	if (parentHierarchyComponent == nullptr)
		parentHierarchyComponent = &mComponentManager->AddComponent<HierarchyComponent>(parent);

	parentHierarchyComponent->childrens.push_back(child);

}

void Scene::initializePrimitiveMesh()
{
	// Careful create all entity and add component first and then upload the data
	// to prevent the invalidation of refrence when container is resized
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	{
		mCube = ecs::CreateEntity();
		mComponentManager->AddComponent<NameComponent>(mCube).name = "Cube";
		MeshRenderer& meshRenderer = mComponentManager->AddComponent<MeshRenderer>(mCube);
		meshRenderer.SetRenderable(false);
		InitializeCubeMesh(vertices, indices);
		meshRenderer.vertexBuffer.byteOffset = 0;
		meshRenderer.vertexBuffer.byteLength = static_cast<uint32_t>(vertices.size() * sizeof(Vertex));
		meshRenderer.indexBuffer.byteOffset = 0;
		meshRenderer.indexBuffer.byteLength = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
		meshRenderer.boundingBox.min = glm::vec3(-1.0f);
		meshRenderer.boundingBox.max = glm::vec3(1.0f);
	}

	{
		uint32_t vertexOffset = (uint32_t)vertices.size();
		uint32_t indexOffset = (uint32_t)indices.size();

		mSphere = ecs::CreateEntity();
		mComponentManager->AddComponent<NameComponent>(mSphere).name = "Sphere";
		MeshRenderer& meshRenderer = mComponentManager->AddComponent<MeshRenderer>(mSphere);
		meshRenderer.SetRenderable(false);
		InitializeSphereMesh(vertices, indices);
		meshRenderer.vertexBuffer.byteOffset = vertexOffset * sizeof(Vertex);
		meshRenderer.vertexBuffer.byteLength = static_cast<uint32_t>((vertices.size() - vertexOffset) * sizeof(Vertex));
		meshRenderer.indexBuffer.byteOffset = indexOffset * sizeof(uint32_t);
		meshRenderer.indexBuffer.byteLength = static_cast<uint32_t>((indices.size() - indexOffset) * sizeof(uint32_t));
		meshRenderer.boundingBox.min = glm::vec3(-1.0f);
		meshRenderer.boundingBox.max = glm::vec3(1.0f);
	}

	{
		uint32_t vertexOffset = (uint32_t)vertices.size();
		uint32_t indexOffset = (uint32_t)indices.size();

		mPlane = ecs::CreateEntity();
		mComponentManager->AddComponent<NameComponent>(mPlane).name = "Plane";
		MeshRenderer& meshRenderer = mComponentManager->AddComponent<MeshRenderer>(mPlane);
		meshRenderer.SetRenderable(false);
		InitializePlaneMesh(vertices, indices);
		meshRenderer.vertexBuffer.byteOffset = vertexOffset * sizeof(Vertex);
		meshRenderer.vertexBuffer.byteLength = static_cast<uint32_t>((vertices.size() - vertexOffset) * sizeof(Vertex));
		meshRenderer.indexBuffer.byteOffset = indexOffset * sizeof(uint32_t);
		meshRenderer.indexBuffer.byteLength = static_cast<uint32_t>((indices.size() - indexOffset) * sizeof(uint32_t));
		meshRenderer.boundingBox.min = glm::vec3(-1.0f, -0.01f, -1.0f);
		meshRenderer.boundingBox.max = glm::vec3(1.0f, 0.01f, 1.0f);
	}

	// Allocate Vertex Buffer
	uint32_t vertexSize = static_cast<uint32_t>(sizeof(Vertex) * vertices.size());
	gfx::GPUBufferDesc bufferDesc = { vertexSize,
		gfx::Usage::Default,
		gfx::BindFlag::ShaderResource };

	gfx::GraphicsDevice* device = gfx::GetDevice();
	gfx::BufferHandle vertexBuffer = device->CreateBuffer(&bufferDesc);
	mAllocatedBuffers.push_back(vertexBuffer);

	device->CopyToBuffer(vertexBuffer, vertices.data(), 0, vertexSize);

	// Allocate Index Buffer
	uint32_t indexSize = (uint32_t)(sizeof(uint32_t) * indices.size());

	bufferDesc.bindFlag = gfx::BindFlag::IndexBuffer;
	bufferDesc.size = indexSize;
	gfx::BufferHandle indexBuffer = device->CreateBuffer(&bufferDesc);
	mAllocatedBuffers.push_back(indexBuffer);
	device->CopyToBuffer(indexBuffer, indices.data(), 0, indexSize);

	auto updateBuffers = [&](ecs::Entity entity, gfx::BufferHandle vertexBuffer, gfx::BufferHandle indexBuffer) {
		MeshRenderer* meshRenderer = mComponentManager->GetComponent<MeshRenderer>(entity);
		meshRenderer->vertexBuffer.buffer = vertexBuffer;
		meshRenderer->indexBuffer.buffer = indexBuffer;
	};

	updateBuffers(mCube, vertexBuffer, indexBuffer);
	updateBuffers(mSphere, vertexBuffer, indexBuffer);
	updateBuffers(mPlane, vertexBuffer, indexBuffer);
}


ecs::Entity Scene::CreateCube(std::string_view name)
{
	ecs::Entity entity = ecs::CreateEntity();
	{
		MeshRenderer& meshRenderer = mComponentManager->AddComponent<MeshRenderer>(entity);
		MeshRenderer* prefab = mComponentManager->GetComponent<MeshRenderer>(mCube);

        // No pointer so it should be fine
		meshRenderer = *prefab;

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
		MeshRenderer* prefab = mComponentManager->GetComponent<MeshRenderer>(mPlane);
		meshRenderer = *prefab;
		meshRenderer.SetRenderable(true);
	}

	mComponentManager->AddComponent<TransformComponent>(entity);
	mComponentManager->AddComponent<MaterialComponent>(entity);
	return entity;

}

void Scene::LoadEnvMap(const std::string& name)
{
	if (mEnvMap)
		mEnvMap->Shutdown();

	mEnvMap = std::make_unique<EnvironmentMap>();
	mEnvMap->CreateFromHDRI(name.c_str());
	mEnvMap->CalculateIrradiance();
	mEnvMap->Prefilter();
	mEnvMap->CalculateBRDFLUT();
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
		MeshRenderer* prefab = mComponentManager->GetComponent<MeshRenderer>(mSphere);
		meshRenderer = *prefab;
		meshRenderer.SetRenderable(true);
	}
	return entity;
}
