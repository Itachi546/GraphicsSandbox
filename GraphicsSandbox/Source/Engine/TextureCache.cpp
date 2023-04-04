#include "TextureCache.h"

#include "GraphicsDevice.h"
#include "Logger.h"
#include "ImageLoader.h"
#include "../Shared/MeshData.h"

#include <vector>
#include <cassert>
#include <unordered_map>
#include <algorithm>

#include "stb_image.h"

/*
* @TODO maybe we don't need this anymore
*/
namespace TextureCache
{
	std::vector<gfx::TextureHandle> gAllTextures(1024);
	std::unordered_map<std::string, gfx::TextureHandle> gAllTextureIndex;
	std::vector<std::string> gAllTextureName(1024);
	gfx::TextureHandle gSolidTexture;
	const uint32_t kMaxMipLevel = 6;
	AsynchronousLoader* gAsyncLoader = nullptr;

	gfx::TextureHandle GetDefaultTexture()
	{
		return gSolidTexture;
	}

	gfx::TextureHandle CreateTexture(int width, int height, int nChannel, bool generateMipmap)
	{
		gfx::GPUTextureDesc desc;
		desc.width = width;
		desc.height = height;
		desc.depth = 1;
		desc.mipLevels = generateMipmap ? kMaxMipLevel : 1;
		desc.arrayLayers = 1;
		desc.bCreateSampler = true;
		desc.bindFlag = gfx::BindFlag::ShaderResource;
		desc.format = gfx::Format::R8G8B8A8_UNORM;
		desc.samplerInfo.wrapMode = gfx::TextureWrapMode::Repeat;
		if (generateMipmap)
			desc.samplerInfo.enableAnisotropicFiltering = true;
		
		gfx::GraphicsDevice* device = gfx::GetDevice();

		gfx::TextureHandle texture = device->CreateTexture(&desc);
		return texture;
	}

	gfx::TextureHandle CreateSolidRGBATexture(uint32_t width, uint32_t height)
	{
		std::vector<uint8_t> pixels(width * height * 4);
		std::fill(pixels.begin(), pixels.end(), 255);
		gfx::TextureHandle texture = CreateTexture(width, height, 4, false);
		gfx::GetDevice()->CopyTexture(texture, pixels.data(), (uint32_t)(pixels.size() * sizeof(uint8_t)));
		//return CreateTexture(pixels.data(), width, height, 4, false);
		return texture;
	}
	
	void Initialize(AsynchronousLoader* loader)
	{
		gSolidTexture = CreateSolidRGBATexture(512, 512);
		gAsyncLoader = loader;
	}

	uint32_t LoadTexture(const std::string& filename, bool generateMipmap)
	{
		auto found = gAllTextureIndex.find(filename);
		if (found != gAllTextureIndex.end())
			return found->second.handle;

		int comp, width, height;
		stbi_info(filename.c_str(), &width, &height, &comp);

		gfx::TextureHandle texture = CreateTexture(width, height, comp, true);
		gAsyncLoader->RequestTextureData(filename, texture);
		/*
		Logger::Debug("Loading Texture: " + filename);
		int width, height, nChannel;
		
		uint8_t* pixels = Utils::ImageLoader::Load(filename.c_str(), width, height, nChannel);
		if (pixels == nullptr)
		{
			Logger::Warn("Failed to load texture: " + filename);
			return INVALID_TEXTURE;
		}
		assert(nChannel == 4);
		//texture.name = filename;
		gfx::TextureHandle texture = CreateTexture(pixels, width, height, nChannel, generateMipmap);
		Utils::ImageLoader::Free(pixels);
		*/
		gAllTextureName[texture.handle] = filename;
		gAllTextures[texture.handle] = texture;
		gAllTextureIndex[filename] = texture;
		return texture.handle;

	}

	std::string GetTextureName(uint32_t index)
	{
		// @TODO Remove
		return gAllTextureName[index];
	}

	gfx::TextureHandle GetByIndex(uint32_t index)
	{
		assert(index < gAllTextures.size());
		return gAllTextures[index];
	}

	gfx::DescriptorInfo GetDescriptorInfo()
	{
		return gfx::DescriptorInfo{gAllTextures.data(), 0,  (uint32_t)gAllTextures.size(), gfx::DescriptorType::ImageArray};
	}

	void Shutdown()
	{
		gfx::GraphicsDevice* device = gfx::GetDevice();
		device->Destroy(gSolidTexture);
		for (auto& [key, val] : gAllTextureIndex)
			device->Destroy(val);
		gAllTextures.clear();
		gAllTextureName.clear();
		gAllTextureIndex.clear();
	}
}
