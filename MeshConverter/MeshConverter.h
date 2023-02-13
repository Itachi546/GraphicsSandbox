#pragma once
#include "../GraphicsSandbox/Source/Shared/MeshData.h"

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <iostream>
#include <cstdint>
#include <string>
#include <chrono>
#include <execution>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <filesystem>

namespace MeshConverter {

	constexpr int EXPORT_TEXTURE_SIZE = 1024;
	std::string TrimPathAndExtension(const std::string& path)
	{
		std::string name = path.substr(path.find_last_of("/\\") + 1, path.length());
		return name.substr(0, name.find_last_of("."));
	}

	uint32_t CalculateStringListSize(const std::vector<std::string>& list)
	{
		uint64_t size = 0;
		return (uint32_t)(std::accumulate(list.begin(), list.end(), size, [](const uint64_t& size, const std::string& str) { return size + str.size(); }));
	}

	int AddUnique(std::vector<std::string>& collection, const std::string& file)
	{
		if (file.empty())
			return -1;

		auto found = std::find(collection.begin(), collection.end(), file);
		if (found == collection.end())
		{
			collection.push_back(file);
			return (int)collection.size() - 1;
		}

		return (int)std::distance(collection.begin(), found);
	}

	uint32_t SaveStringList(std::ofstream& file, const std::vector<std::string>& lines)
	{
		uint32_t bytes = 0;
		for (auto& line : lines)
		{
			uint32_t size = (uint32_t)line.size();
			file.write(reinterpret_cast<const char*>(&size), 4 * sizeof(char));
			file.write(line.c_str(), size);
			bytes += size;
		}
		return bytes;
	}

	std::string ResizeAndExportTexture(const unsigned char* src, int srcWidth, int srcHeight, int dstMaxWidth, int dstMaxHeight, int nChannel, const std::string& outputPath)
	{
		const int newW = std::min(srcWidth, dstMaxWidth);
		const int newH = std::min(srcHeight, dstMaxHeight);

		const uint32_t imageSize = newW * newH * nChannel;
		std::vector<uint8_t> mipData(imageSize);
		uint8_t* dst = mipData.data();

		// @TODO bundle all texture into same file and export 
		stbir_resize_uint8(src, srcWidth, srcHeight, 0, dst, newW, newH, 0, nChannel);

		std::string outAbsolutePath = std::filesystem::absolute(std::filesystem::path(outputPath)).string();
		stbi_write_png(outAbsolutePath.c_str(), newW, newH, nChannel, dst, 0);
		std::cout << "Exported Image: " << outAbsolutePath << std::endl;
		return outAbsolutePath;
	}

	std::string ConvertTexture(const std::string& filename, const std::string& exportPath)
	{
		std::string newFileName = TrimPathAndExtension(filename) + "_converted.png";
		std::string outputImagePath = exportPath +  '/' + newFileName;

		int width, height, nChannel;
		stbi_uc* pixels = stbi_load(filename.c_str(), &width, &height, &nChannel, STBI_rgb_alpha);

		uint8_t* src = pixels;
		nChannel = STBI_rgb_alpha;

		if (pixels == nullptr)
		{
			// If file cannot be loaded we simply return that file as output
			std::cout << "Failed to load file: " << filename << std::endl;
			return std::filesystem::absolute(std::filesystem::path(filename)).string();
		}
		else {
			fprintf(stdout, "Loaded [%s] %dx%d texture with %d channels\n", filename.c_str(), width, height, nChannel);
		}

		ResizeAndExportTexture(pixels, width, height, EXPORT_TEXTURE_SIZE, EXPORT_TEXTURE_SIZE, nChannel, outputImagePath);

		stbi_image_free(pixels);

		return newFileName;
	}

	glm::mat4 AIMat4toGLM(const aiMatrix4x4& transform)
	{
		return glm::transpose(glm::make_mat4(&transform.a1));
	}

	template<typename T>
	int AddNode(const aiNode* aiNode, int parent, std::vector<T>& nodes_)
	{
		int currentNode = static_cast<int>(nodes_.size());
		{
			T node = {};
			//assert(aiNode->mName.length <= 64);
			std::strcpy(node.name, aiNode->mName.C_Str());
			node.parent = parent;
			node.firstChild = -1;
			node.nextSibling = -1;
			node.lastSibling = -1;
			node.localTransform = AIMat4toGLM(aiNode->mTransformation);
			nodes_.push_back(std::move(node));
		}

		if (parent > -1)
		{
			int firstSibling = nodes_[parent].firstChild;
			// if first sibling is not present we add current child as 
			// first child as well as last child
			if (firstSibling == -1)
			{
				nodes_[parent].firstChild = currentNode;
				// Cache the first child so that when we try to 
				// add the next sibling we can refer to the lastest one 
				// from the lastSibling
				nodes_[parent].lastSibling = currentNode;
			}
			else {
				int dest = nodes_[parent].lastSibling;
				for (dest = firstSibling;
					nodes_[dest].nextSibling != -1;
					dest = nodes_[dest].nextSibling);
				nodes_[dest].nextSibling = currentNode;
				nodes_[dest].lastSibling = currentNode;
			}
		}
		return currentNode;
	}

	void ProcessTexture(const aiScene* scene, std::vector<std::string>& textureFiles, const std::string& basePath, const std::string& exportPathBase, const std::string& textureFolder)
	{
		std::string exportPath = exportPathBase + '/' + textureFolder;
		if (!std::filesystem::exists(exportPath))
			std::filesystem::create_directory(exportPath);

		auto converter = [&exportPath, &basePath, &textureFolder, scene](const std::string& filename) {
			return textureFolder + "/" + ConvertTexture(basePath + filename, exportPath);
		};

		std::transform(std::execution::par, textureFiles.begin(), textureFiles.end(), textureFiles.begin(), converter);
	}

	void ParseMaterial(aiMaterial* assimpMat, MaterialComponent& material, std::vector<std::string>& textureFiles)
	{
		printf("%s\n", assimpMat->GetName().C_Str());

		aiColor4D color;
		// Ignore emissive color for now
		//if (aiGetMaterialColor(material, AI_MATKEY_COLOR_AMBIENT, &color) == AI_SUCCESS)
		//	materialComp.emissive = 1.0f;
		if (aiGetMaterialColor(assimpMat, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS)
			material.albedo = { color.r, color.g, color.b, color.a };
		if (aiGetMaterialColor(assimpMat, AI_MATKEY_COLOR_EMISSIVE, &color) == AI_SUCCESS)
			material.emissive = color.r;

		// PBR Roughness and Metallic
		float temp = 1.0f;
		if (aiGetMaterialFloat(assimpMat, AI_MATKEY_METALLIC_FACTOR, &temp) == AI_SUCCESS)
			material.metallic = temp;
		if (aiGetMaterialFloat(assimpMat, AI_MATKEY_ROUGHNESS_FACTOR, &temp) == AI_SUCCESS)
			material.roughness = temp;

		// Textures
		aiString Path;
		aiTextureMapping Mapping;
		unsigned int UVIndex = 0;
		float Blend = 1.0f;
		aiTextureOp TextureOp = aiTextureOp_Add;
		aiTextureMapMode TextureMapMode[2] = { aiTextureMapMode_Wrap, aiTextureMapMode_Wrap };
		unsigned int TextureFlags = 0;

		if (aiGetMaterialTexture(assimpMat, aiTextureType_EMISSIVE, 0, &Path, &Mapping, &UVIndex, &Blend, &TextureOp, TextureMapMode, &TextureFlags) == AI_SUCCESS)
			material.emissiveMap = AddUnique(textureFiles, Path.C_Str());

		if (aiGetMaterialTexture(assimpMat, aiTextureType_DIFFUSE, 0, &Path, &Mapping, &UVIndex, &Blend, &TextureOp, TextureMapMode, &TextureFlags) == AI_SUCCESS)
			material.albedoMap = AddUnique(textureFiles, Path.C_Str());

		if (aiGetMaterialTexture(assimpMat, aiTextureType_NORMALS, 0, &Path, &Mapping, &UVIndex, &Blend, &TextureOp, TextureMapMode, &TextureFlags) == AI_SUCCESS)
			material.normalMap = AddUnique(textureFiles, Path.C_Str());

		if (aiGetMaterialTexture(assimpMat, aiTextureType_AMBIENT_OCCLUSION, 0, &Path, &Mapping, &UVIndex, &Blend, &TextureOp, TextureMapMode, &TextureFlags) == AI_SUCCESS)
			material.ambientOcclusionMap = AddUnique(textureFiles, Path.C_Str());

		if (aiGetMaterialTexture(assimpMat, aiTextureType_DIFFUSE_ROUGHNESS, 0, &Path, &Mapping, &UVIndex, &Blend, &TextureOp, TextureMapMode, &TextureFlags) == AI_SUCCESS)
			material.roughnessMap = AddUnique(textureFiles, Path.C_Str());

		if (aiGetMaterialTexture(assimpMat, aiTextureType_METALNESS, 0, &Path, &Mapping, &UVIndex, &Blend, &TextureOp, TextureMapMode, &TextureFlags) == AI_SUCCESS)
			material.metallicMap = AddUnique(textureFiles, Path.C_Str());

		/*
		if (aiGetMaterialTexture(assimpMat, aiTextureType_OPACITY, 0, &Path, &Mapping, &UVIndex, &Blend, &TextureOp, TextureMapMode, &TextureFlags) == AI_SUCCESS)
			material.opacityMap = AddUnique(textureFiles, Path.C_Str());
			material.transparency = 0.5f;
			*/
	}

	// Root bone is the bone that doesn't have any other bone as parent 
	aiNode* FindRootBone(aiNode* parent, std::vector<std::string>& bones)
	{
		std::string nodeName = parent->mName.C_Str();
		auto found = std::find_if(bones.begin(), bones.end(), [&nodeName](const std::string& bone) {
			return bone == nodeName;
			});

		if (found != bones.end())
			return parent;
		else
		{
			for (uint32_t i = 0; i < parent->mNumChildren; ++i)
			{
				aiNode* result = FindRootBone(parent->mChildren[i], bones);
				if (result != nullptr)
					return result;
			}
		}

		return nullptr;
	}

	uint32_t ParseSkeletonNode(aiNode* bone, uint32_t parent, std::vector<std::string>& bones, std::vector<glm::mat4>& offsetMatrix, std::vector<SkeletonNode>& skeletonNodes)
	{
		fprintf(stdout, "Bone: %s\n", bone->mName.C_Str());
		uint32_t node = AddNode<SkeletonNode>(bone, parent, skeletonNodes);
		auto found = std::find(bones.begin(), bones.end(), std::string(bone->mName.C_Str()));
		uint32_t index = 0;
		if (found == bones.end())
		{
			bones.push_back(bone->mName.C_Str());
			offsetMatrix.push_back(glm::mat4(1.0f));
			index = static_cast<uint32_t>(bones.size() - 1);
			fprintf(stdout, "Failed to find the boneIndex for %s", bone->mName.C_Str());
		}
		else
			index = (uint32_t)std::distance(bones.begin(), found);

		skeletonNodes[node].boneIndex = index;
		skeletonNodes[node].offsetMatrix = offsetMatrix[index];

		for (uint32_t i = 0; i < bone->mNumChildren; ++i)
			ParseSkeletonNode(bone->mChildren[i], node, bones, offsetMatrix, skeletonNodes);

		return node;
	}

	uint32_t CreateSkeletonHierarchy(const aiScene* scene, std::vector<std::string>& bones, std::vector<glm::mat4>& offsetMatrix, std::vector<SkeletonNode>& skeletonNodes)
	{
		aiNode* rootBone = FindRootBone(scene->mRootNode, bones);
		return ParseSkeletonNode(rootBone, -1, bones, offsetMatrix, skeletonNodes);
	}

	Mesh ParseMesh(const aiMesh* mesh, const aiScene* scene, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, BoundingBox& aabb, std::vector<SkeletonNode>& skeletonNodes, std::vector<std::string>& bones)
	{
		uint32_t vertexOffset = static_cast<uint32_t>(vertices.size() * sizeof(Vertex));
		uint32_t indexOffset = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));

		aiString name = mesh->mName;
		const uint32_t nVertices = mesh->mNumVertices;
		const uint32_t nFaces = mesh->mNumFaces;

		printf("----------------------------------------------\n");
		printf("Loading Mesh:  %s\n", name.C_Str());
		printf("Total Vertex:  %d\n", nVertices);
		printf("Total Indices: %d\n", nFaces * 3);
		printf("----------------------------------------------\n");

		bool bTexCoords = mesh->HasTextureCoords(0);
		for (uint32_t i = 0; i < nVertices; ++i)
		{
			aiVector3D v = mesh->mVertices[i];
			aiVector3D n = mesh->mNormals[i];
			aiVector3D t = bTexCoords ? mesh->mTextureCoords[0][i] : aiVector3D();
			aiVector3D tangent = aiVector3D(0.0f, 0.0f, 1.0f);
			aiVector3D bitangent = aiVector3D(0.0f, 1.0f, 0.0f);
			
			if (mesh->mTangents)
			{
				tangent = mesh->mTangents[i];
				bitangent = mesh->mBitangents[i];
			}

			Vertex vertex{ glm::vec3(v.x, v.y, v.z),
				 glm::vec3(n.x, n.y, n.z),
				glm::vec2(t.x, t.y),
				glm::vec3(tangent.x, tangent.y, tangent.z),
				glm::vec3(bitangent.x, bitangent.y, bitangent.z),
			};
			vertices.push_back(vertex);
		}

		for (uint32_t i = 0; i < nFaces; ++i)
		{
			aiFace face = mesh->mFaces[i];

			if (face.mNumIndices != 3)
				continue;

			for (uint32_t index = 0; index < face.mNumIndices; ++index)
				indices.push_back(face.mIndices[index]);
		}
		aabb.min = glm::vec3(mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z);
		aabb.max = glm::vec3(mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z);

		// Process bones
		uint32_t skeletonIndex = -1;
		uint32_t boneCount = 0;
		if (mesh->HasBones())
		{
			aiBone** aiBones = mesh->mBones;
			std::vector<std::string> currentBones;
			std::vector<glm::mat4> offsetMatrix;

			for (uint32_t i = 0; i < mesh->mNumBones; ++i)
			{
				aiString boneName = aiBones[i]->mName;
				currentBones.push_back(boneName.C_Str());
				offsetMatrix.push_back(AIMat4toGLM(aiBones[i]->mOffsetMatrix));
			}

			// Create Skeleton Hierarchy
			skeletonIndex = CreateSkeletonHierarchy(scene, currentBones, offsetMatrix, skeletonNodes);
			boneCount = (uint32_t)currentBones.size();
			bones.insert(bones.end(), currentBones.begin(), currentBones.end());
		}

		Mesh result = {};
		result.vertexOffset = vertexOffset;
		result.indexOffset = indexOffset;
		result.vertexCount = nVertices;
		result.indexCount = nFaces * 3;
		result.materialIndex = mesh->mMaterialIndex;
		result.skeletonIndex = skeletonIndex;
		result.boneCount = boneCount;

		uint32_t length = std::min(32u, name.length);
		std::memcpy(result.meshName, name.C_Str(), length);
		return result;
	}

	void ParseAnimation(const aiScene* scene, uint32_t nAnimations, aiAnimation** animations)
	{

	}

	void ParseSceneNode(const aiScene* scene, const aiNode* aiNode, MeshData* out, std::vector<std::string>& bones, int parent = -1)
	{
		std::string nodeName = aiNode->mName.C_Str();
		auto found = std::find(bones.begin(), bones.end(), nodeName);
		if (found != bones.end())
			return;

		int currentNode = AddNode<Node>(aiNode, parent, out->nodes_);
		if (aiNode->mNumMeshes > 0)
		{
			const uint32_t nMeshes = aiNode->mNumMeshes;
			for (uint32_t i = 0; i < nMeshes; ++i)
			{
				aiMesh* mesh = scene->mMeshes[i];
				uint32_t meshId = aiNode->mMeshes[i];
				//out->meshes_[meshId] = ParseMesh(scene->mMeshes[meshId], scene, out->vertexData_, out->indexData_, out->boxes_[meshId]);
				out->meshes_[meshId].nodeIndex = currentNode;
			}
		}

		for (uint32_t i = 0; i < aiNode->mNumChildren; ++i)
			ParseSceneNode(scene, aiNode->mChildren[i], out, bones, currentNode);
	}

	void ParseScene(const aiScene* scene, const std::string& basePath, const std::string& exportPath, const std::string& filename, MeshData* out)
	{
		const uint32_t nMeshes = scene->mNumMeshes;
		const uint32_t nMaterials = scene->mNumMaterials;
		out->meshes_.resize(nMeshes);
		out->boxes_.resize(nMeshes);
		out->materials_.resize(nMaterials);

		std::vector<std::string> bones;
		for (uint32_t i = 0; i < nMeshes; ++i)
			out->meshes_[i] = ParseMesh(scene->mMeshes[i], scene, out->vertexData_, out->indexData_, out->boxes_[i], out->skeletonNodes_, bones);

		if (scene->mRootNode)
			ParseSceneNode(scene, scene->mRootNode, out, bones);

		//if (scene->mRootNode)
		//ParseNode(scene, scene->mRootNode, out);

		std::vector<std::string>& textureFiles = out->textures_;
		printf("----------------------------------------------\n");
		printf("Parsing Material\n");
		for (uint32_t i = 0; i < nMaterials; ++i)
			ParseMaterial(scene->mMaterials[i], out->materials_[i], textureFiles);
		printf("----------------------------------------------\n");

		if (textureFiles.size() > 0)
		{
			printf("----------------------------------------------\n");
			printf("Processing Textures\n");
			ProcessTexture(scene, textureFiles, basePath, exportPath, TrimPathAndExtension(filename));
			printf("----------------------------------------------\n");
		}

		if (scene->HasAnimations())
		{
			printf("----------------------------------------------\n");
			printf("Processing Animation\n");
			ParseAnimation(scene, scene->mNumAnimations, scene->mAnimations);
		}
	}

	void LoadFile(const std::string& filename, const std::string& exportPath, MeshData* out)
	{
		printf("Loading %s ...\n", filename.c_str());

		const std::size_t pathSeparator = filename.find_last_of("/\\");
		const std::string basePath = (pathSeparator != std::string::npos) ? filename.substr(0, pathSeparator + 1) : std::string();

		const uint32_t flags = 0 |
			aiProcess_Triangulate |
			/*aiProcess_PreTransformVertices |*/
			aiProcess_GenBoundingBoxes |
			aiProcess_JoinIdenticalVertices |
			aiProcess_GenSmoothNormals |
			aiProcess_LimitBoneWeights |
			aiProcess_CalcTangentSpace |
			aiProcess_SplitLargeMeshes |
			aiProcess_ImproveCacheLocality |
			aiProcess_RemoveRedundantMaterials |
			aiProcess_FindDegenerates |
			aiProcess_FindInvalidData;

		const aiScene* scene = aiImportFile(filename.c_str(), flags);

		if (!scene || !scene->HasMeshes())
		{
			printf("Unable to load %s\n", filename.c_str());
			exit(255);
		}

		ParseScene(scene, basePath, exportPath, filename, out);
	}

	void SaveMeshData(const std::string& filename, MeshData* meshData)
	{
		uint32_t nNodes = (uint32_t)meshData->nodes_.size();
		uint32_t nSkeletonNodes = (uint32_t)meshData->skeletonNodes_.size();
		uint32_t nMeshes = (uint32_t)meshData->meshes_.size();
		uint32_t vertexDataSize = (uint32_t)(meshData->vertexData_.size() * sizeof(Vertex));
		uint32_t indexDataSize = (uint32_t)(meshData->indexData_.size() * sizeof(uint32_t));
		uint32_t nMaterial = (uint32_t)meshData->materials_.size();
		uint32_t nTextures = (uint32_t)meshData->textures_.size();

		// Texture string data size
		// first the size of string is written to file and then the original string is 
		// written, this help us to parse the string accordingly while reading
		uint32_t texStrDataSize = CalculateStringListSize(meshData->textures_) + sizeof(uint32_t) * nTextures;
		MeshFileHeader header = {
			0x12345678u,
			nNodes,
			nSkeletonNodes,
			nMeshes,
			nMaterial,
			nTextures,
			sizeof(MeshFileHeader) +
			sizeof(Mesh) * nMeshes +
			sizeof(SkeletonNode) * nSkeletonNodes +
			sizeof(MaterialComponent) * nMaterial +
			texStrDataSize +
			sizeof(BoundingBox) * nMeshes + 
			sizeof(Node) * nNodes,
			vertexDataSize,indexDataSize
		};

		std::ofstream outFile(filename, std::ios::binary);

		// Write header
		outFile.write(reinterpret_cast<const char*>(&header), sizeof(header));

		// Write Nodes
		outFile.write(reinterpret_cast<const char*>(meshData->nodes_.data()), sizeof(Node) * nNodes);

		// Write SkeletonNodes
		if(meshData->skeletonNodes_.size() > 0)
			outFile.write(reinterpret_cast<const char*>(meshData->skeletonNodes_.data()), sizeof(SkeletonNode) * nSkeletonNodes);

		// Write MeshData
		outFile.write(reinterpret_cast<const char*>(meshData->meshes_.data()), sizeof(Mesh) * nMeshes);

		// Write Material
		outFile.write(reinterpret_cast<const char*>(meshData->materials_.data()), sizeof(MaterialComponent) * nMaterial);

		// Write Textures
		if(nTextures > 0)
			SaveStringList(outFile, meshData->textures_);

		// Write BoundingBox
		outFile.write(reinterpret_cast<const char*>(meshData->boxes_.data()), sizeof(BoundingBox) * nMeshes);

		// Write Vertices
		outFile.write(reinterpret_cast<const char*>(meshData->vertexData_.data()), vertexDataSize);

		// Write Indices
		outFile.write(reinterpret_cast<const char*>(meshData->indexData_.data()), indexDataSize);

		outFile.close();

		printf("Exported File: %s\n", filename.c_str());
	}

	void Convert(std::string fileName, std::string exportPath)
	{
		std::string newFilename = exportPath + "/" + TrimPathAndExtension(fileName) + ".sbox";
		{
			MeshData meshData;
			LoadFile(fileName, exportPath, &meshData);
			SaveMeshData(newFilename, &meshData);
		}
	}
}