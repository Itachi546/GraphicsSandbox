#include "EnvironmentMap.h"
#include "Utils.h"
#include "ImageLoader.h"

EnvironmentMap::EnvironmentMap()
{
	mDevice = gfx::GetDevice();
	uint32_t codeLen = 0;
	char* code = Utils::ReadFile("Assets/SPIRV/cubemap_converter.comp.spv", &codeLen);

	gfx::ShaderDescription shaderDesc{ code, codeLen };

	gfx::PipelineDesc pipelineDesc;
	pipelineDesc.shaderCount = 1;
	pipelineDesc.shaderDesc = &shaderDesc;

	mHdriToCubemap = std::make_shared<gfx::Pipeline>();
	mDevice->CreateComputePipeline(&pipelineDesc, mHdriToCubemap.get());
	delete[] code;

	mCubemap = std::make_shared<gfx::GPUTexture>();
}

void EnvironmentMap::CreateFromHDRI(const char* hdri)
{
	gfx::GraphicsDevice* device = gfx::GetDevice();

	// Load HDRI Image File 
	int width, height, nComponent;
	float* hdriData = Utils::ImageLoader::LoadHDRI(hdri, width, height, nComponent);

	// Create Source HDRI Texture 
	gfx::GPUTextureDesc hdriDesc{};
	hdriDesc.width = width;
	hdriDesc.height = height;
	hdriDesc.bindFlag = gfx::BindFlag::ShaderResource;
	hdriDesc.imageViewType = gfx::ImageViewType::IV2D;
	hdriDesc.format = gfx::Format::R32B32G32A32_SFLOAT;
	hdriDesc.bCreateSampler = true;
	gfx::GPUTexture hdriTexture;
	device->CreateTexture(&hdriDesc, &hdriTexture);

	// Create Staging buffer to transfer hdri data
	const uint32_t imageDataSize = width * height * 4 * sizeof(float);
	gfx::GPUBuffer stagingBuffer;
	gfx::GPUBufferDesc bufferDesc;
	bufferDesc.bindFlag = gfx::BindFlag::None;
	bufferDesc.usage = gfx::Usage::Upload;
	bufferDesc.size = imageDataSize;
	device->CreateBuffer(&bufferDesc, &stagingBuffer);
	std::memcpy(stagingBuffer.mappedDataPtr, hdriData, imageDataSize);

	// Copy from staging buffer to hdri texture
	gfx::ImageBarrierInfo transferBarrier{ gfx::AccessFlag::None, gfx::AccessFlag::TransferWriteBit,gfx::ImageLayout::TransferDstOptimal };
	gfx::PipelineBarrierInfo transferBarrierInfo = {
		&transferBarrier, 1,
		gfx::PipelineStage::TransferBit,
		gfx::PipelineStage::TransferBit
	};
	device->CopyTexture(&hdriTexture, &stagingBuffer, &transferBarrierInfo, 0, 0);
	Utils::ImageLoader::Free(hdriData);

	// Create Destination Cubemap texture
	gfx::GPUTextureDesc cubemapDesc{};
	cubemapDesc.width = mCubemapWidth;
	cubemapDesc.height = mCubemapHeight;
	cubemapDesc.bindFlag = gfx::BindFlag::ShaderResource;
	cubemapDesc.imageViewType = gfx::ImageViewType::IVCubemap;
	cubemapDesc.arrayLayers = 6;
	cubemapDesc.format = gfx::Format::R16B16G16A16_SFLOAT;
	cubemapDesc.bCreateSampler = true;
	device->CreateTexture(&cubemapDesc, mCubemap.get());

	// Begin Compute Shader
	gfx::CommandList commandList = device->BeginCommandList();

	// Layout transition for shader read/write
	gfx::ImageBarrierInfo imageBarrier[] = { 
		gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ShaderRead, gfx::ImageLayout::ShaderReadOptimal, &hdriTexture},
		gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ShaderWrite, gfx::ImageLayout::General, mCubemap.get()}
	};

	gfx::PipelineBarrierInfo computeBarrier = {
		imageBarrier, static_cast<uint32_t>(std::size(imageBarrier)),
		gfx::PipelineStage::BottomOfPipe,
		gfx::PipelineStage::ComputeShader,
	};
	device->PipelineBarrier(&commandList, &computeBarrier);

	// Bind resourcezs
	gfx::DescriptorInfo descriptorInfos[2] = {};
	descriptorInfos[0].resource = &hdriTexture;
	descriptorInfos[0].offset = 0;
	descriptorInfos[0].type = gfx::DescriptorType::Image;

	descriptorInfos[1].resource = mCubemap.get();
	descriptorInfos[1].offset = 0;
	descriptorInfos[1].type = gfx::DescriptorType::Image;

	mDevice->UpdateDescriptor(mHdriToCubemap.get(), descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
	device->BindPipeline(&commandList, mHdriToCubemap.get());
	device->DispatchCompute(&commandList, mCubemapWidth / 32, mCubemapHeight / 32, 6);
	device->SubmitCommandList(&commandList);
	device->WaitForGPU();
}
