#include "TextureCache.h"

#include "GraphicsDevice.h"
#include "Logger.h"
#include "ImageLoader.h"
#include "../Shared/MeshData.h"

#include <vector>
#include <cassert>
#include <unordered_map>
#include <algorithm>

namespace TextureCache
{
	std::vector<gfx::GPUTexture> gAllTextures;
	std::unordered_map<std::string, uint32_t> gAllTextureIndex;
	uint32_t nTexture = 0;
	gfx::GPUTexture* gSolidTexture;
	const uint32_t kMaxMipLevel = 6;

	gfx::GPUTexture* GetDefaultTexture()
	{
		return gSolidTexture;
	}

	void CreateTexture(gfx::GPUTexture* texture, unsigned char* pixels, int width, int height, int nChannel, bool generateMipmap)
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

		device->CreateTexture(&desc, texture);
		{
			// Create Staging buffer to transfer image data
			const uint32_t imageDataSize = width * height * nChannel * sizeof(uint8_t);
			gfx::GPUBuffer stagingBuffer;
			gfx::GPUBufferDesc bufferDesc;
			bufferDesc.bindFlag = gfx::BindFlag::None;
			bufferDesc.usage = gfx::Usage::Upload;
			bufferDesc.size = imageDataSize;
			device->CreateBuffer(&bufferDesc, &stagingBuffer);
			std::memcpy(stagingBuffer.mappedDataPtr, pixels, imageDataSize);

			// Copy from staging buffer to GPUTexture
			gfx::ImageBarrierInfo transferBarrier{ gfx::AccessFlag::None, gfx::AccessFlag::TransferWriteBit,gfx::ImageLayout::TransferDstOptimal };
			gfx::PipelineBarrierInfo transferBarrierInfo = {
				&transferBarrier, 1,
				gfx::PipelineStage::TransferBit,
				gfx::PipelineStage::TransferBit
			};
			device->CopyTexture(texture, &stagingBuffer, &transferBarrierInfo, 0, 0);
			// If miplevels is greater than  1 the mip are generated
			// else the imagelayout is transitioned to shader attachment optimal
			device->GenerateMipmap(texture, desc.mipLevels);
		}
	}

	void CreateSolidRGBATexture(gfx::GPUTexture* out, uint32_t width, uint32_t height)
	{
		std::vector<uint8_t> pixels(width * height * 4);
		std::fill(pixels.begin(), pixels.end(), 255);
		CreateTexture(out, pixels.data(), width, height, 4, false);

	}
	
	void Initialize()
	{
		gSolidTexture = new gfx::GPUTexture;
		CreateSolidRGBATexture(gSolidTexture, 512, 512);
	}

	uint32_t LoadTexture(const std::string& filename, bool generateMipmap)
	{
		auto found = gAllTextureIndex.find(filename);
		if (found != gAllTextureIndex.end())
			return found->second;

		Logger::Debug("Loading Texture: " + filename);
		int width, height, nChannel;
		
		uint8_t* pixels = Utils::ImageLoader::Load(filename.c_str(), width, height, nChannel);
		if (pixels == nullptr)
		{
			Logger::Warn("Failed to load texture: " + filename);
			return INVALID_TEXTURE;
		}
		assert(nChannel == 4);
		gfx::GPUTexture texture;
		texture.name = filename;
		CreateTexture(&texture, pixels, width, height, nChannel, generateMipmap);

		uint32_t nTexture = static_cast<uint32_t>(gAllTextures.size());
		gAllTextures.push_back(texture);
		gAllTextureIndex[filename] = nTexture;
		Utils::ImageLoader::Free(pixels);
		return nTexture++;
	}

	gfx::GPUTexture* GetByIndex(uint32_t index)
	{
		assert(index < gAllTextures.size());
		return &gAllTextures[index];
	}

	gfx::DescriptorInfo GetDescriptorInfo()
	{
		return gfx::DescriptorInfo{gAllTextures.data(), 0,  (uint32_t)gAllTextures.size(), gfx::DescriptorType::ImageArray};
	}

	void Free()
	{
		delete gSolidTexture;
		gAllTextures.clear();
	}
}
