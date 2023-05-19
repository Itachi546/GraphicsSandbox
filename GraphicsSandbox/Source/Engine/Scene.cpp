#include "Scene.h"
#include "Logger.h"
#include "../Shared/MeshData.h"
#include "TextureCache.h"
#include "DebugDraw.h"
#include "../Shared/MathUtils.h"
#include "StringConstants.h"
#include "GUI/ImGuiService.h"

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

void Scene::GenerateMeshData(ecs::Entity entity, const IMeshRenderer* meshRenderer, std::vector<DrawData>& opaque, std::vector<DrawData>& transparent)
{
	if (meshRenderer->IsRenderable())
	{
		TransformComponent* transform = mComponentManager->GetComponent<TransformComponent>(entity);
		if (transform == nullptr) return;

		BoundingBox aabb = meshRenderer->boundingBox;
		aabb.Transform(transform->worldMatrix);

		bool isVisible = true;
		if (mEnableFrustumCulling)
		{
			if (!mCamera.mFrustum->Intersect(aabb))
				isVisible = false;
		}

		if (isVisible)
		{
			DrawData drawData = {};
			const gfx::BufferView& vertexBuffer = meshRenderer->vertexBuffer;
			const gfx::BufferView& indexBuffer = meshRenderer->indexBuffer;

			drawData.vertexBuffer = vertexBuffer;
			drawData.indexBuffer = indexBuffer;
			drawData.entity = entity;
			drawData.indexCount = static_cast<uint32_t>(meshRenderer->GetIndexCount());
			drawData.worldTransform = transform->worldMatrix;
			drawData.elmSize = meshRenderer->IsSkinned() ? sizeof(AnimatedVertex) : sizeof(Vertex);

			auto material = mComponentManager->GetComponent<MaterialComponent>(entity);
			drawData.material = material;

			if (material->IsTransparent())
				transparent.push_back(std::move(drawData));
			else
				opaque.push_back(std::move(drawData));
		}
	}
}

void Scene::GenerateDrawData(std::vector<DrawData>& opaque, std::vector<DrawData>& transparent)
{
	auto meshRendererComponents = mComponentManager->GetComponentArray<MeshRenderer>();
	auto& frustum = mCamera.mFrustum;

	for (int i = 0; i < meshRendererComponents->GetCount(); ++i)
	{
		const MeshRenderer& meshRenderer = meshRendererComponents->components[i];
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
		const SkinnedMeshRenderer& meshRenderer = skinnedMeshRendererComponents->components[i];
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
		meshRenderer.Copy(renderer);
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
		meshRenderer.Copy(renderer);
		meshRenderer.SetRenderable(true);
	}

	mComponentManager->AddComponent<TransformComponent>(entity);
	mComponentManager->AddComponent<MaterialComponent>(entity);

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
		meshRenderer.Copy(renderer);
		meshRenderer.SetRenderable(true);
	}
	return entity;
}

void TraverseSkeletonHierarchy(uint32_t root, uint32_t parent, const std::vector<SkeletonNode>& skeletonNodes, Skeleton& skeleton)
{
	/*
	* BoneIndex
	* Parent
	* Root
	* mJointNames => BoneIndex
	*/

	if (root == -1)
		return;

	const SkeletonNode& node = skeletonNodes[root];
	uint32_t parentIndex = -1;
	glm::mat4 offsetMatrix4 = glm::inverse(node.offsetMatrix);
	if (parent != -1)
	{
		parentIndex = skeletonNodes[parent].boneIndex;
		offsetMatrix4 = skeletonNodes[parent].offsetMatrix * offsetMatrix4;
	}

	// @NOTE we use boneIndex to store the parent child relationship instead of
	// nodeIndex as it can be used later to index into the matrix without any 
	// additional information. The shader access the matrix pallete based on the
	// bone index defined by weight matrix.
	// Node index is used to traverse the hierarchy whereas all the data regarding
	// bones are stored by using boneIndex.
	TransformComponent localTransform;
	glm::mat4 localTransformMat = node.localTransform;
	TransformComponent::From(node.localTransform, localTransform);

	// Update Rest Pose Parent/Transform
	Pose& restPose = skeleton.GetRestPose();
	restPose.SetParent(node.boneIndex, parentIndex);
	restPose.SetLocalTransform(node.boneIndex, localTransform);

	// Update Bind Pose Parent/Transform
	TransformComponent bindTransform;
	TransformComponent::From(offsetMatrix4, bindTransform);
	Pose& bindPose = skeleton.GetBindPose();
	bindPose.SetParent(node.boneIndex, parentIndex);
	bindPose.SetLocalTransform(node.boneIndex, bindTransform);

	Pose& animatedPose = skeleton.GetAnimatedPose();
	animatedPose.SetParent(node.boneIndex, parentIndex);

	skeleton.SetInvBindPose(node.boneIndex, node.offsetMatrix);
	skeleton.SetJointName(node.boneIndex, node.name);

	if (node.firstChild != -1)
		TraverseSkeletonHierarchy(node.firstChild, root, skeletonNodes, skeleton);

	// Check sibling
	if (node.nextSibling != -1)
		TraverseSkeletonHierarchy(node.nextSibling, parent, skeletonNodes, skeleton);
}


void Scene::ParseSkeleton(const Mesh& mesh, Skeleton& skeleton, uint32_t rootBone, const std::vector<SkeletonNode>& skeletonNodes)
{
	skeleton.Resize(mesh.boneCount);

	Pose& restPose = skeleton.GetRestPose();
	restPose.Resize(mesh.boneCount);

	Pose& bindPose = skeleton.GetBindPose();
	bindPose.Resize(mesh.boneCount);

	Pose& animatedPose = skeleton.GetAnimatedPose();
	animatedPose.Resize(mesh.boneCount);

	skeleton.SetRootBone(skeletonNodes[rootBone].boneIndex);
	TraverseSkeletonHierarchy(rootBone, ~0u, skeletonNodes, skeleton);
}

void Scene::ParseAnimation(const StagingMeshData& meshData, std::vector<AnimationClip>& animationClips)
{
	for (auto& animation : meshData.animations_)
	{
		AnimationClip animationClip = {};
		animationClip.SetTickPerSeconds(animation.framePerSecond);
		animationClip.SetStartTime(0.0f);
		animationClip.SetEndTime(animation.duration);
		std::vector<TransformTrack>& transformTracks = animationClip.GetTracks();
		transformTracks.resize(meshData.skeletonNodes_.size());

		uint32_t channelStart = animation.channelStart;
		uint32_t channelCount = animation.channelCount;
		for (uint32_t i = channelStart; i < channelStart + channelCount; ++i)
		{
			const Channel& channel = meshData.channels_[i];

			uint32_t tS = channel.translationTrack;
			uint32_t tC = channel.traslationCount;
			std::vector<Vector3Track> translations(tC);
			std::copy(meshData.vector3Tracks_.begin() + tS, meshData.vector3Tracks_.begin() + tS + tC, translations.begin());

			uint32_t rS = channel.rotationTrack;
			uint32_t rC = channel.rotationCount;
			std::vector<QuaternionTrack> rotations(rC);
			std::copy(meshData.quaternionTracks_.begin() + rS, meshData.quaternionTracks_.begin() + rS + rC, rotations.begin());

			uint32_t sS = channel.scalingTrack;
			uint32_t sC = channel.scalingCount;
			std::vector<Vector3Track> scaling(sC);
			std::copy(meshData.vector3Tracks_.begin() + sS, meshData.vector3Tracks_.begin() + sS + sC, scaling.begin());

			transformTracks[channel.boneId].AddPositions(translations);
			transformTracks[channel.boneId].AddRotations(rotations);
			transformTracks[channel.boneId].AddScale(scaling);
		}
		animationClips.push_back(std::move(animationClip));
	}

}

void Scene::UpdateEntity(ecs::Entity parent,
	uint32_t nodeIndex,
	const StagingMeshData& stagingMeshData)
{
	std::vector<uint32_t> meshIds;
	for (uint32_t i = 0; i < stagingMeshData.meshes_.size(); ++i)
	{
		if (stagingMeshData.meshes_[i].nodeIndex == nodeIndex)
			meshIds.push_back(i);
	}

	if (meshIds.size() > 0)
	{
		auto hierarchyCompArr = mComponentManager->GetComponentArray<HierarchyComponent>();

		// if parent doesn't have hierarchyComponent add it
		std::size_t parentIndex = hierarchyCompArr->GetIndex(parent);
		if (parentIndex == ~0ull)
		{
			mComponentManager->AddComponent<HierarchyComponent>(parent);
			parentIndex = hierarchyCompArr->GetIndex(parent);
		}

		for (auto meshId : meshIds)
		{
			// For Each mesh create entity
			const Mesh& mesh = stagingMeshData.meshes_[meshId];

			ecs::Entity meshEntity = ecs::CreateEntity();
			mComponentManager->AddComponent<NameComponent>(meshEntity).name = std::string(mesh.meshName);
			auto& transform = mComponentManager->AddComponent<TransformComponent>(meshEntity);

			// Add parent entity 
			mComponentManager->AddComponent<HierarchyComponent>(meshEntity).parent = parent;
			hierarchyCompArr->components[parentIndex].childrens.push_back(meshEntity);

			// Update MeshRenderer Component
			IMeshRenderer* meshRenderer = nullptr;
			if (mesh.skeletonIndex == -1)
			{
				meshRenderer = &mComponentManager->AddComponent<MeshRenderer>(meshEntity);
				meshRenderer->SetSkinned(false);
			}
			else {
				meshRenderer = &mComponentManager->AddComponent<SkinnedMeshRenderer>(meshEntity);
				meshRenderer->SetSkinned(true);
				SkinnedMeshRenderer* skinnedMeshRenderer = reinterpret_cast<SkinnedMeshRenderer*>(meshRenderer);
				// Parse Skeleton
				ParseSkeleton(mesh, skinnedMeshRenderer->skeleton, mesh.skeletonIndex, stagingMeshData.skeletonNodes_);
				ParseAnimation(stagingMeshData, skinnedMeshRenderer->animationClips);
			}
			meshRenderer->vertexBuffer.buffer = stagingMeshData.vertexBuffer;
			meshRenderer->vertexBuffer.offset = mesh.vertexOffset;
			meshRenderer->vertexBuffer.size = mesh.vertexCount;

			meshRenderer->indexBuffer.buffer = stagingMeshData.indexBuffer;
			meshRenderer->indexBuffer.offset = mesh.indexOffset;
			meshRenderer->indexBuffer.size = mesh.indexCount;

			meshRenderer->boundingBox = stagingMeshData.boxes_[meshId];

			// @TODO Seperate between Skinned and StaticMesh
			meshRenderer->CopyVertices((void*)(stagingMeshData.vertexData_.data() + mesh.vertexOffset), mesh.vertexCount);
			meshRenderer->CopyIndices((void*)(stagingMeshData.indexData_.data() + mesh.indexOffset), mesh.indexCount);

			MaterialComponent& material = mComponentManager->AddComponent<MaterialComponent>(meshEntity);
			uint32_t materialIndex = mesh.materialIndex;
			if (stagingMeshData.materials_.size() > 0 && materialIndex != -1)
			{
				const std::vector<std::string>& textures = stagingMeshData.textures_;

				material = stagingMeshData.materials_[materialIndex];
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


ecs::Entity Scene::CreateMeshEntity(uint32_t nodeIndex, ecs::Entity parent, const StagingMeshData& stagingMeshData)
{
	ecs::Entity entity = ecs::CreateEntity();
	const Node& node = stagingMeshData.nodes_[nodeIndex];

	// Name Component
	mComponentManager->AddComponent<NameComponent>(entity).name = node.name;

	// TransformComponent
	TransformComponent& transform = mComponentManager->AddComponent<TransformComponent>(entity);
	transform.defaultMatrix = node.localTransform;

	UpdateEntity(entity, nodeIndex, stagingMeshData);

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


ecs::Entity Scene::TraverseNode(uint32_t root, ecs::Entity parent, const StagingMeshData& stagingData)
{
	if (root == -1)
		return ecs::INVALID_ENTITY;

	ecs::Entity entity = CreateMeshEntity(root, parent, stagingData);

	// Add Child
	const Node& node = stagingData.nodes_[root];
	if(node.firstChild != -1)
		TraverseNode(node.firstChild, entity, stagingData);

	// Check sibling
	if(node.nextSibling != -1)
		TraverseNode(node.nextSibling, parent, stagingData);

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

	// Read Header
	MeshFileHeader header = {};
	inFile.read(reinterpret_cast<char*>(&header), sizeof(MeshFileHeader));
	assert(header.magicNumber == 0x12345678u);

	std::string name = file;
	std::size_t lastPathSplit = name.find_last_of("/\\");
	std::string folder = name.substr(0, lastPathSplit + 1);

	name = name.substr(lastPathSplit + 1, name.size() - 1);
	name = name.substr(0, name.find_first_of('.'));

	// Create a meshdata struct
	StagingMeshData stagingMeshData;

	// Read Nodes
	uint32_t nNodes = header.nodeCount;
	stagingMeshData.nodes_.resize(nNodes);
	inFile.read(reinterpret_cast<char*>(stagingMeshData.nodes_.data()), sizeof(Node) * nNodes);

	// Read Meshes
	uint32_t nMeshes = header.meshCount;
	stagingMeshData.meshes_.resize(nMeshes);
	inFile.read(reinterpret_cast<char*>(stagingMeshData.meshes_.data()), sizeof(Mesh) * nMeshes);

	// Read Materials
	uint32_t nMaterial = header.materialCount;
	stagingMeshData.materials_.resize(nMaterial);
	inFile.read(reinterpret_cast<char*>(stagingMeshData.materials_.data()), sizeof(MaterialComponent) * nMaterial);
	
	// Read Texture Path
	stagingMeshData.textures_.resize(header.textureCount);
	if (header.textureCount > 0)
	{
		for (uint32_t i = 0; i < header.textureCount; ++i)
		{
			uint32_t size = 0;
			inFile.read(reinterpret_cast<char*>(&size), 4);

			std::string& path = stagingMeshData.textures_[i];
			path.resize(size);
			inFile.read(path.data(), size);
			path = folder + path;
		}
	}

	// Read BoundingBox
	stagingMeshData.boxes_.resize(nMeshes);
	inFile.read(reinterpret_cast<char*>(stagingMeshData.boxes_.data()), sizeof(BoundingBox) * nMeshes);

	//Read VertexData
	stagingMeshData.vertexData_.resize(header.vertexDataSize);
	stagingMeshData.indexData_.resize(header.indexDataSize);
	inFile.seekg(header.dataBlockStartOffset, std::ios::beg);
	inFile.read(reinterpret_cast<char*>(stagingMeshData.vertexData_.data()), header.vertexDataSize);
	inFile.read(reinterpret_cast<char*>(stagingMeshData.indexData_.data()), header.indexDataSize);

	// Read Skeleton Nodes
	uint32_t nSkeletonNodes = header.skeletonNodeCount;
	if (nSkeletonNodes > 0)
	{
		stagingMeshData.skeletonNodes_.resize(nSkeletonNodes);
		inFile.read(reinterpret_cast<char*>(stagingMeshData.skeletonNodes_.data()), sizeof(SkeletonNode) * nSkeletonNodes);

		uint32_t nAnimation = header.animationCount;
		if (nAnimation > 0)
		{
			stagingMeshData.animations_.resize(nAnimation);
			inFile.read(reinterpret_cast<char*>(stagingMeshData.animations_.data()), sizeof(Animation) * nAnimation);

			uint32_t nChannel = header.animationChannelCount;
			stagingMeshData.channels_.resize(nChannel);
			inFile.read(reinterpret_cast<char*>(stagingMeshData.channels_.data()), sizeof(Channel) * nChannel);

			uint32_t nV3Track = header.v3TrackCount;
			stagingMeshData.vector3Tracks_.resize(nV3Track);
			inFile.read(reinterpret_cast<char*>(stagingMeshData.vector3Tracks_.data()), sizeof(Vector3Track) * nV3Track);

			uint32_t nQuatTrack = header.quatTrackCount;
			stagingMeshData.quaternionTracks_.resize(nQuatTrack);
			inFile.read(reinterpret_cast<char*>(stagingMeshData.quaternionTracks_.data()), sizeof(QuaternionTrack) * nQuatTrack);

		}
	}
	
	inFile.close();

	// Allocate GPU Memory
	gfx::GraphicsDevice* gfxDevice = gfx::GetDevice();
	gfx::GPUBufferDesc bufferDesc;
	bufferDesc.bindFlag = gfx::BindFlag::ShaderResource;
	bufferDesc.usage = gfx::Usage::Default;
	bufferDesc.size = header.vertexDataSize;
	stagingMeshData.vertexBuffer = gfxDevice->CreateBuffer(&bufferDesc);
	mAllocatedBuffers.push_back(stagingMeshData.vertexBuffer);

	bufferDesc.bindFlag = gfx::BindFlag::IndexBuffer;
	bufferDesc.size = header.indexDataSize;
	stagingMeshData.indexBuffer = gfxDevice->CreateBuffer(&bufferDesc);
	mAllocatedBuffers.push_back(stagingMeshData.indexBuffer);

	gfxDevice->CopyToBuffer(stagingMeshData.vertexBuffer, stagingMeshData.vertexData_.data(), 0, header.vertexDataSize);
	gfxDevice->CopyToBuffer(stagingMeshData.indexBuffer, stagingMeshData.indexData_.data(), 0, header.indexDataSize);
	return TraverseNode(0, ecs::INVALID_ENTITY, stagingMeshData);
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
	mOpaqueBatches.clear();
	mTransparentBatches.clear();
	mSkinnedBatches.clear();

	mCamera.Update(dt);
	UpdateTransform();
	UpdateHierarchy();

	if(mShowBoundingBox)
		DrawBoundingBox();

	GenerateDrawData(mOpaqueBatches, mTransparentBatches);
	GenerateSkinnedMeshDrawData(mSkinnedBatches, mTransparentBatches);
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
	LightComponent* light = mComponentManager->GetComponent<LightComponent>(mSun);
	TransformComponent* transform = mComponentManager->GetComponent<TransformComponent>(mSun);
	if (ImGui::CollapsingHeader("Directional Light"))
	{
		ImGui::PushID("dirlight");
		ImGui::SliderFloat3("rotation", &transform->rotation[0], -glm::pi<float>(), glm::pi<float>());
		ImGui::SliderFloat("intensity", &light->intensity, 0.0f, 4.0f);
		ImGui::ColorPicker3("color", &light->color[0]);
		ImGui::PopID();
	}
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
		uint32_t vertexBufferIndex = 0;
		gfx::GPUBufferDesc bufferDesc = { vertexCount * static_cast<uint32_t>(sizeof(Vertex)),
			gfx::Usage::Default,
			gfx::BindFlag::ShaderResource };

		gfx::GraphicsDevice* device = gfx::GetDevice();
		gfx::BufferHandle vertexBuffer = device->CreateBuffer(&bufferDesc);
		mAllocatedBuffers.push_back(vertexBuffer);

		// Allocate Index Buffer
		uint32_t indexBufferIndex = 0;
		bufferDesc.bindFlag = gfx::BindFlag::IndexBuffer;
		bufferDesc.size = indexCount * static_cast<uint32_t>(sizeof(uint32_t));
		gfx::BufferHandle indexBuffer = device->CreateBuffer(&bufferDesc);
		mAllocatedBuffers.push_back(indexBuffer);

		MeshRenderer* cubeMesh = mComponentManager->GetComponent<MeshRenderer>(mCube);
		uint32_t vertexOffset = 0;
		uint32_t vertexSize = static_cast<uint32_t>(cubeMesh->vertices->size() * sizeof(Vertex));
		device->CopyToBuffer(vertexBuffer, cubeMesh->vertices->data(), vertexOffset, vertexSize);
		cubeMesh->vertexBuffer = { vertexBuffer, vertexOffset, vertexSize };
		vertexOffset += vertexSize;

		uint32_t indexOffset = 0;
		uint32_t indexSize = static_cast<uint32_t>(cubeMesh->indices->size() * sizeof(uint32_t));
		device->CopyToBuffer(indexBuffer, cubeMesh->indices->data(), indexOffset, indexSize);
		cubeMesh->indexBuffer = { indexBuffer, indexOffset, indexSize };
		indexOffset += indexSize;

		MeshRenderer* sphereMesh = mComponentManager->GetComponent<MeshRenderer>(mSphere);
		vertexSize = static_cast<uint32_t>(sphereMesh->vertices->size() * sizeof(Vertex));
		device->CopyToBuffer(vertexBuffer, sphereMesh->vertices->data(), vertexOffset, vertexSize);
		sphereMesh->vertexBuffer = { vertexBuffer, vertexOffset, vertexSize };
		vertexOffset += vertexSize;

		indexSize = static_cast<uint32_t>(sphereMesh->indices->size() * sizeof(uint32_t));
		device->CopyToBuffer(indexBuffer, sphereMesh->indices->data(), indexOffset, indexSize);
		sphereMesh->indexBuffer = { indexBuffer, indexOffset, indexSize };
		indexOffset += indexSize;


		MeshRenderer* planeMesh = mComponentManager->GetComponent<MeshRenderer>(mPlane);
		vertexSize = static_cast<uint32_t>(planeMesh->vertices->size() * sizeof(Vertex));
		device->CopyToBuffer(vertexBuffer, planeMesh->vertices->data(), vertexOffset, vertexSize);
		planeMesh->vertexBuffer = { vertexBuffer, vertexOffset, vertexSize };

		indexSize = static_cast<uint32_t>(planeMesh->indices->size() * sizeof(uint32_t));
		device->CopyToBuffer(indexBuffer, planeMesh->indices->data(), indexOffset, indexSize);
		planeMesh->indexBuffer = { indexBuffer, indexOffset, indexSize };
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
	for (uint32_t y = 0; y <= subdivide; ++y)
	{
		float yPos = y * stepSize - 1.0f;
		for (uint32_t x = 0; x <= subdivide; ++x)
		{
			vertices->push_back(Vertex{ glm::vec3(x * stepSize - 1.0f, 0.0f, yPos),
				glm::vec3(0.0f, 1.0f, 0.0f),
				glm::vec2(x * stepSize, y * stepSize) });
		}
	}

	uint32_t totalGrid = subdivide * subdivide;
	uint32_t nIndices =  totalGrid * 6;

	std::shared_ptr<std::vector<unsigned int>> indices = meshRenderer.indices;
	indices->reserve(nIndices);
	for (uint32_t y = 0; y < subdivide; ++y)
	{
		for (uint32_t x = 0; x < subdivide; ++x)
		{
			uint32_t i0 = y * (subdivide + 1) + x;
			uint32_t i1 = i0 + 1;
			uint32_t i2 = i0 + subdivide + 1;
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
/*
	for (uint32_t i = 0; i < 50; ++i)
	{
		ecs::Entity light = ecs::CreateEntity();
		mComponentManager->AddComponent<NameComponent>(light).name = "light" + std::to_string(i);
		TransformComponent& transform = mComponentManager->AddComponent<TransformComponent>(light);
		transform.position = glm::vec3(MathUtils::Rand01() * 50 - 25, MathUtils::Rand01() * 25, MathUtils::Rand01() * 50 - 25);
		transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
		//transform.rotation = glm::vec3(0.0f, 0.021f, 0.375f);
		LightComponent& lightComp = mComponentManager->AddComponent<LightComponent>(light);
		lightComp.color = glm::vec3(MathUtils::Rand01(), 1.0f - MathUtils::Rand01(), MathUtils::Rand01());
		lightComp.intensity = 10.0f;
		lightComp.type = LightType::Point;
	}
	*/
}

void Scene::RemoveChild(ecs::Entity parent, ecs::Entity child)
{
	auto comp = mComponentManager->GetComponent<HierarchyComponent>(parent);
	if (comp->RemoveChild(child))
		Logger::Info("Children Removed of Id: " + std::to_string(child));
	else
		Logger::Warn("No Children of Id: " + std::to_string(child));
}

