#include "../GraphicsSandbox/Source/Shared/MeshData.h"

#include <iostream>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>
#include <assimp/material.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <fstream>
#include <algorithm>
#include <numeric>
#include <filesystem>

constexpr int EXPORT_TEXTURE_SIZE = 512;

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
	std::string outputImagePath = exportPath + TrimPathAndExtension(filename) + "_converted.png";

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

	std::string newFilePath = ResizeAndExportTexture(pixels, width, height, EXPORT_TEXTURE_SIZE, EXPORT_TEXTURE_SIZE, nChannel, outputImagePath);

	if (pixels)
		stbi_image_free(pixels);

	return newFilePath;
}


void ProcessTexture(const aiScene* scene, std::vector<std::string>& textureFiles, const std::string& basePath, const std::string& exportPath)
{
	auto converter = [&exportPath, &basePath, scene](const std::string& filename) {
		return ConvertTexture(basePath + filename, exportPath);
	};

	std::transform(textureFiles.begin(), textureFiles.end(), textureFiles.begin(), converter);
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
	if (aiGetMaterialFloat(assimpMat, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, &temp) == AI_SUCCESS)
		material.metallic = temp;
	if (aiGetMaterialFloat(assimpMat, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, &temp) == AI_SUCCESS)
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

Mesh ParseMesh(const aiMesh* mesh, const aiScene* scene, std::vector<float>& vertices, std::vector<uint32_t>& indices)
{
	uint32_t vertexOffset = static_cast<uint32_t>(vertices.size() * sizeof(float));
	uint32_t indexOffset  = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));

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

		vertices.push_back(v.x), vertices.push_back(v.y), vertices.push_back(v.z);
		vertices.push_back(n.x), vertices.push_back(n.y), vertices.push_back(n.z);
		vertices.push_back(t.x), vertices.push_back(t.y);
	}

	for (uint32_t i = 0; i < nFaces; ++i)
	{
		aiFace face = mesh->mFaces[i];

		if (face.mNumIndices != 3)
			continue;
		
		for(uint32_t index = 0; index < face.mNumIndices; ++index)
			indices.push_back(face.mIndices[index]);
	}

	Mesh result = {
		vertexOffset,
		indexOffset,
		nVertices,
		nFaces * 3,
		mesh->mMaterialIndex
	};

	uint32_t length = std::min(32u, name.length);
	std::memcpy(result.meshName, name.C_Str(), length);
	return result;
}

void ParseScene(const aiScene* scene, const std::string& basePath, const std::string& exportPath, MeshData* out)
{
	const uint32_t nMeshes = scene->mNumMeshes;
	out->meshes.resize(nMeshes);

	for (uint32_t i = 0; i < nMeshes; ++i)
		out->meshes[i] = ParseMesh(scene->mMeshes[i], scene, out->vertexData_, out->indexData_);

	printf("----------------------------------------------\n");
	printf("Parsing Material\n");
	std::vector<std::string>& textureFiles = out->textures;
	const uint32_t nMaterials = scene->mNumMaterials;
	out->materials.resize(nMaterials);
	for (uint32_t i = 0; i < nMaterials; ++i)
		ParseMaterial(scene->mMaterials[i], out->materials[i], textureFiles);
	printf("----------------------------------------------\n");

	printf("----------------------------------------------\n");
	printf("Processing Textures\n");
	ProcessTexture(scene, textureFiles, basePath, exportPath);
	printf("----------------------------------------------\n");
}

void LoadFile(const std::string& filename, const std::string& exportPath, MeshData* out)
{
	printf("Loading %s ...\n", filename.c_str());

	const std::size_t pathSeparator = filename.find_last_of("/\\");
	const std::string basePath = (pathSeparator != std::string::npos) ? filename.substr(0, pathSeparator + 1) : std::string();

	const uint32_t flags = 0 |
		aiProcess_JoinIdenticalVertices |
		aiProcess_Triangulate |
		aiProcess_GenSmoothNormals |
		aiProcess_LimitBoneWeights |
		aiProcess_SplitLargeMeshes |
		aiProcess_ImproveCacheLocality |
		aiProcess_RemoveRedundantMaterials |
		aiProcess_FindDegenerates |
		aiProcess_FindInvalidData |
		aiProcess_GenUVCoords;

	const aiScene* scene = aiImportFile(filename.c_str(), flags);

	if (!scene || !scene->HasMeshes())
	{
		printf("Unable to load %s\n", filename.c_str());
		exit(255);
	}

	ParseScene(scene, basePath, exportPath, out);
}

void SaveMeshData(const std::string& filename, MeshData* meshData)
{
	uint32_t nMeshes = (uint32_t)meshData->meshes.size();
	uint32_t vertexDataSize = (uint32_t)(meshData->vertexData_.size() * sizeof(float));
	uint32_t indexDataSize = (uint32_t)(meshData->indexData_.size() * sizeof(uint32_t));
	uint32_t nMaterial = (uint32_t)meshData->materials.size();
	uint32_t nTextures = (uint32_t)meshData->textures.size();

	// Texture string data size
	// first the size of string is written to file and then the original string is 
	// written, this help us to parse the string accordingly while reading
	uint32_t texStrDataSize = CalculateStringListSize(meshData->textures) + sizeof(uint32_t) * nTextures;
	MeshFileHeader header = {
		0x12345678u,
		nMeshes,
		nMaterial,
		nTextures,
		sizeof(MeshFileHeader) + sizeof(Mesh) * nMeshes + sizeof(MaterialComponent) * nMaterial + texStrDataSize,
		vertexDataSize,
		indexDataSize
	};

	std::ofstream outFile(filename, std::ios::binary);

	// Write header
	outFile.write(reinterpret_cast<const char*>(&header), sizeof(header));

	// Write MeshData
	outFile.write(reinterpret_cast<const char*>(meshData->meshes.data()), sizeof(Mesh) * nMeshes);

	// Write Material
	outFile.write(reinterpret_cast<const char*>(meshData->materials.data()), sizeof(MaterialComponent) * nMaterial);

	// Write Textures
	SaveStringList(outFile, meshData->textures);

	// Write Vertices
	outFile.write(reinterpret_cast<const char*>(meshData->vertexData_.data()), vertexDataSize);

	// Write Indices
	outFile.write(reinterpret_cast<const char*>(meshData->indexData_.data()), indexDataSize);

	outFile.close();

	printf("Exported File: %s\n", filename.c_str());
}

int main(int argc, char** argv)
{

	if (argc <= 1)
		printf("Convert any 3D Mesh Format to .sbox format\n");

	std::string filename = "C:\\Users\\lenovo\\Documents\\3D-Assets\\Models\\breakfast_room\\breakfast_room.gltf";
	//std::string filename = "Model/bloom.obj";
	std::string exportBasePath = "../Assets/models/";
	std::string newFilename = exportBasePath + TrimPathAndExtension(filename) + ".sbox";
	{
		MeshData meshData;
		LoadFile(filename, exportBasePath, &meshData);
		SaveMeshData(newFilename, &meshData);
	}
	return 0;
}