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
	// Used to transition layout for newly added texture
	std::vector<gfx::GPUTexture> gNewTextures;

	void CreateTexture(gfx::GPUTexture* texture, unsigned char* pixels, int width, int height, int nChannel)
	{
		gfx::GPUTextureDesc desc;
		desc.width = width;
		desc.height = height;
		desc.depth = 1;
		desc.mipLevels = 1;
		desc.arrayLayers = 1;
		desc.bCreateSampler = true;
		desc.bindFlag = gfx::BindFlag::ShaderResource;
		desc.format = gfx::Format::R8G8B8A8_UNORM;
		desc.samplerInfo.wrapMode = gfx::TextureWrapMode::Repeat;

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
		}
	}

	void CreateSolidRGBATexture(gfx::GPUTexture* out, uint32_t width, uint32_t height)
	{
		std::vector<uint8_t> pixels(width * height * 4);
		std::fill(pixels.begin(), pixels.end(), 255);
		CreateTexture(out, pixels.data(), width, height, 4);
	}
	
	void Initialize()
	{
		if (gAllTextures.size() == 0)
		{
			gfx::GPUTexture texture;
			CreateSolidRGBATexture(&texture, 512, 512);
			gAllTextures.resize(64);
			std::fill(gAllTextures.begin(), gAllTextures.end(), texture);
			gNewTextures.push_back(texture);
		}
	}

	uint32_t LoadTexture(const std::string& filename)
	{
		auto found = gAllTextureIndex.find(filename);
		if (found != gAllTextureIndex.end())
			return found->second;

		Logger::Debug("Loading Texture: " + filename);
		int width, height, nChannel;
		
		uint8_t* pixels = Utils::ImageLoader::Load(filename.c_str(), width, height, nChannel);
		assert(nChannel == 4);
		if (pixels == nullptr)
			return INVALID_TEXTURE;

		gfx::GPUTexture texture;
		CreateTexture(&texture, pixels, width, height, nChannel);

		gAllTextures[nTexture] = texture;
		gAllTextureIndex[filename] = nTexture;
		gNewTextures.push_back(texture);
		Utils::ImageLoader::Free(pixels);
		return (uint32_t)nTexture++;
	}

	gfx::DescriptorInfo GetDescriptorInfo()
	{
		return gfx::DescriptorInfo{gAllTextures.data(), 0,  (uint32_t)gAllTextures.size(), gfx::DescriptorType::ImageArray};
	}

	void PrepareTexture(gfx::CommandList* commandList)
	{
		if (gNewTextures.size() > 0)
		{
			std::vector<gfx::ImageBarrierInfo> imageBarriers(gNewTextures.size());
			// Layout transition for shader read/write
			for (uint32_t i = 0; i < imageBarriers.size(); ++i)
			{
				imageBarriers[i] = {
				gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ShaderRead, gfx::ImageLayout::ShaderReadOptimal, &gNewTextures[i]},
				};
			}

			gfx::PipelineBarrierInfo barrier = {
				imageBarriers.data(), static_cast<uint32_t>(imageBarriers.size()),
				gfx::PipelineStage::BottomOfPipe,
				gfx::PipelineStage::FragmentShader,
			};
			gfx::GetDevice()->PipelineBarrier(commandList, &barrier);
			gNewTextures.clear();
		}
	}

	void Free()
	{
		gAllTextures.clear();
	}
}
