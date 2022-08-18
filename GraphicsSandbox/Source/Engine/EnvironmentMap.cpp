#include "EnvironmentMap.h"
#include "Utils.h"
#include "ImageLoader.h"
#include "Logger.h"

#include <assert.h>

static void CreateComputePipeline(const char* filename, gfx::GraphicsDevice* device, gfx::Pipeline* out)
{
	uint32_t codeLen = 0;
	char* code = Utils::ReadFile(filename, &codeLen);
	gfx::ShaderDescription shaderDesc{ code, codeLen };
	gfx::PipelineDesc pipelineDesc;
	pipelineDesc.shaderCount = 1;
	pipelineDesc.shaderDesc = &shaderDesc;
	device->CreateComputePipeline(&pipelineDesc, out);
	delete[] code;
}

EnvironmentMap::EnvironmentMap()
{
	mDevice = gfx::GetDevice();

	mHdriToCubemap = std::make_shared<gfx::Pipeline>();
	CreateComputePipeline("Assets/SPIRV/hdri_converter.comp.spv", mDevice, mHdriToCubemap.get());
	mIrradiancePipeline = std::make_shared<gfx::Pipeline>();
	CreateComputePipeline("Assets/SPIRV/irradiance.comp.spv", mDevice, mIrradiancePipeline.get());
	mPrefilterPipeline = std::make_shared<gfx::Pipeline>();
	CreateComputePipeline("Assets/SPIRV/prefilter_env.comp.spv", mDevice, mPrefilterPipeline.get());
	mBRDFPipeline = std::make_shared<gfx::Pipeline>();
	CreateComputePipeline("Assets/SPIRV/brdf_lut.comp.spv", mDevice, mBRDFPipeline.get());

}

void EnvironmentMap::CreateFromHDRI(const char* hdri)
{
	Logger::Debug("Converting hdri: " + std::string(hdri));
	mCubemapTexture = std::make_shared<gfx::GPUTexture>();

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
	mDevice->CreateTexture(&hdriDesc, &hdriTexture);

	// Create Staging buffer to transfer hdri data
	const uint32_t imageDataSize = width * height * 4 * sizeof(float);
	gfx::GPUBuffer stagingBuffer;
	gfx::GPUBufferDesc bufferDesc;
	bufferDesc.bindFlag = gfx::BindFlag::None;
	bufferDesc.usage = gfx::Usage::Upload;
	bufferDesc.size = imageDataSize;
	mDevice->CreateBuffer(&bufferDesc, &stagingBuffer);
	std::memcpy(stagingBuffer.mappedDataPtr, hdriData, imageDataSize);

	// Copy from staging buffer to hdri texture
	gfx::ImageBarrierInfo transferBarrier{ gfx::AccessFlag::None, gfx::AccessFlag::TransferWriteBit,gfx::ImageLayout::TransferDstOptimal };
	gfx::PipelineBarrierInfo transferBarrierInfo = {
		&transferBarrier, 1,
		gfx::PipelineStage::TransferBit,
		gfx::PipelineStage::TransferBit
	};
	mDevice->CopyTexture(&hdriTexture, &stagingBuffer, &transferBarrierInfo, 0, 0);
	Utils::ImageLoader::Free(hdriData);

	// Create Destination Cubemap texture
	gfx::GPUTextureDesc cubemapDesc{};
	cubemapDesc.width = cubemapDesc.height = mCubemapDims;
	cubemapDesc.bindFlag = gfx::BindFlag::ShaderResource;
	cubemapDesc.imageViewType = gfx::ImageViewType::IVCubemap;
	cubemapDesc.arrayLayers = 6;
	cubemapDesc.format = gfx::Format::R16B16G16A16_SFLOAT;
	cubemapDesc.bCreateSampler = true;
	mDevice->CreateTexture(&cubemapDesc, mCubemapTexture.get());

	// Begin Compute Shader
	gfx::CommandList commandList = mDevice->BeginCommandList();

	// Layout transition for shader read/write
	gfx::ImageBarrierInfo imageBarrier[] = { 
		gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ShaderRead, gfx::ImageLayout::ShaderReadOptimal, &hdriTexture},
		gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ShaderWrite, gfx::ImageLayout::General, mCubemapTexture.get()}
	};

	gfx::PipelineBarrierInfo computeBarrier = {
		imageBarrier, static_cast<uint32_t>(std::size(imageBarrier)),
		gfx::PipelineStage::BottomOfPipe,
		gfx::PipelineStage::ComputeShader,
	};
	mDevice->PipelineBarrier(&commandList, &computeBarrier);

	// Bind resources
	gfx::DescriptorInfo descriptorInfos[2] = {};
	descriptorInfos[0].resource = &hdriTexture;
	descriptorInfos[0].offset = 0;
	descriptorInfos[0].type = gfx::DescriptorType::Image;

	descriptorInfos[1].resource = mCubemapTexture.get();
	descriptorInfos[1].offset = 0;
	descriptorInfos[1].type = gfx::DescriptorType::Image;

	float shaderData[] = { (float)mCubemapDims };
	mDevice->UpdateDescriptor(mHdriToCubemap.get(), descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
	mDevice->BindPipeline(&commandList, mHdriToCubemap.get());
	mDevice->PushConstants(&commandList, mHdriToCubemap.get(), gfx::ShaderStage::Compute, shaderData, sizeof(float) * static_cast<uint32_t>(std::size(shaderData)));
	mDevice->DispatchCompute(&commandList, mCubemapDims / 32, mCubemapDims / 32, 6);
	mDevice->SubmitCommandList(&commandList);
	mDevice->WaitForGPU();
	Logger::Debug("HDRI Converted Successfully");

}

void EnvironmentMap::CalculateIrradiance()
{
	Logger::Debug("Generating Irradiance Texture");
	assert(mCubemapTexture != nullptr);

	mIrradianceTexture = std::make_shared<gfx::GPUTexture>();
	gfx::GPUTextureDesc irrDesc{};
	irrDesc.width = irrDesc.height = mIrrTexDims;
	irrDesc.bindFlag = gfx::BindFlag::ShaderResource;
	irrDesc.imageViewType = gfx::ImageViewType::IVCubemap;
	irrDesc.arrayLayers = 6;
	irrDesc.format = gfx::Format::R16B16G16A16_SFLOAT;
	irrDesc.bCreateSampler = true;
	mDevice->CreateTexture(&irrDesc, mIrradianceTexture.get());

	// Begin Compute Shader
	gfx::CommandList commandList = mDevice->BeginCommandList();
	// Layout transition for shader read/write
	gfx::ImageBarrierInfo imageBarrier[] = {
		gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ShaderWrite, gfx::ImageLayout::General, mIrradianceTexture.get()},
	};

	gfx::PipelineBarrierInfo computeBarrier = {
		imageBarrier, static_cast<uint32_t>(std::size(imageBarrier)),
		gfx::PipelineStage::BottomOfPipe,
		gfx::PipelineStage::ComputeShader,
	};

	mDevice->PipelineBarrier(&commandList, &computeBarrier);

	// Bind resources
	gfx::DescriptorInfo descriptorInfos[2] = {};
	descriptorInfos[0].resource = mCubemapTexture.get();
	descriptorInfos[0].offset = 0;
	descriptorInfos[0].type = gfx::DescriptorType::Image;

	descriptorInfos[1].resource = mIrradianceTexture.get();
	descriptorInfos[1].offset = 0;
	descriptorInfos[1].type = gfx::DescriptorType::Image;

	float shaderData[] = { (float)mIrrTexDims };
	mDevice->UpdateDescriptor(mIrradiancePipeline.get(), descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
	mDevice->BindPipeline(&commandList, mIrradiancePipeline.get());
	mDevice->PushConstants(&commandList, mHdriToCubemap.get(), gfx::ShaderStage::Compute, shaderData, sizeof(float) * static_cast<uint32_t>(std::size(shaderData)));
	mDevice->DispatchCompute(&commandList, mIrrTexDims / 8, mIrrTexDims / 8, 6);
	mDevice->SubmitCommandList(&commandList);
	mDevice->WaitForGPU();
	Logger::Debug("Irradiance Texture Generated");
}

void EnvironmentMap::Prefilter()
{
	Logger::Debug("Generating Prefiltered Environment Texture");
	assert(mCubemapTexture != nullptr);

	mPrefilterTexture = std::make_shared<gfx::GPUTexture>();
	gfx::GPUTextureDesc desc{};
	desc.width = desc.height = mPrefilterDims;
	desc.bindFlag = gfx::BindFlag::ShaderResource;
	desc.imageViewType = gfx::ImageViewType::IVCubemap;
	desc.arrayLayers = 6;
	desc.mipLevels = nMaxMipLevel;
	desc.format = gfx::Format::R16B16G16A16_SFLOAT;
	desc.bCreateSampler = true;
	mDevice->CreateTexture(&desc, mPrefilterTexture.get());

	// Begin Compute Shader
	gfx::CommandList commandList = mDevice->BeginCommandList();

	// Layout transition for shader read/write
	gfx::ImageBarrierInfo imageBarrier[] = {
		gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ShaderWrite, gfx::ImageLayout::General, mPrefilterTexture.get()},
	};

	gfx::PipelineBarrierInfo computeBarrier = {
		imageBarrier, static_cast<uint32_t>(std::size(imageBarrier)),
		gfx::PipelineStage::BottomOfPipe,
		gfx::PipelineStage::ComputeShader,
	};

	mDevice->PipelineBarrier(&commandList, &computeBarrier);

	// Bind resources
	gfx::DescriptorInfo descriptorInfos[2] = {};
	descriptorInfos[0].resource = mCubemapTexture.get();
	descriptorInfos[0].offset = 0;
	descriptorInfos[0].type = gfx::DescriptorType::Image;

	uint32_t dims = mPrefilterDims;
	for (uint32_t i = 0; i < nMaxMipLevel; ++i)
	{
		float roughness = i / float(nMaxMipLevel - 1);
		descriptorInfos[1].resource = mPrefilterTexture.get();
		descriptorInfos[1].mipLevel = i;
		descriptorInfos[1].type = gfx::DescriptorType::Image;

		float shaderData[] = { (float)dims, roughness };

		mDevice->UpdateDescriptor(mPrefilterPipeline.get(), descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)), true);
		mDevice->BindPipeline(&commandList, mPrefilterPipeline.get());
		mDevice->PushConstants(&commandList, mPrefilterPipeline.get(), gfx::ShaderStage::Compute, shaderData, (uint32_t)(sizeof(float) * std::size(shaderData)));
		mDevice->DispatchCompute(&commandList, dims / 8, dims / 8, 6);
		dims /= 2;
	}
	mDevice->SubmitCommandList(&commandList);
	mDevice->WaitForGPU();
	Logger::Debug("Prefiltered Environment Texture Generated");
}

void EnvironmentMap::CalculateBRDFLUT()
{
	Logger::Debug("Generating BRDF LUT");
	assert(mCubemapTexture != nullptr);

	mBRDFTexture = std::make_shared<gfx::GPUTexture>();

	gfx::GPUTextureDesc desc{};
	desc.width = desc.height = mBRDFDims;
	desc.bindFlag = gfx::BindFlag::ShaderResource;
	desc.imageViewType = gfx::ImageViewType::IV2D;
	desc.format = gfx::Format::R16G16_SFLOAT;
	desc.imageAspect = gfx::ImageAspect::Color;
	desc.bCreateSampler = true;
	mDevice->CreateTexture(&desc, mBRDFTexture.get());

	// Begin Compute Shader
	gfx::CommandList commandList = mDevice->BeginCommandList();

	// Layout transition for shader read/write
	gfx::ImageBarrierInfo imageBarrier[] = {
		gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ShaderWrite, gfx::ImageLayout::General, mBRDFTexture.get()},
	};

	gfx::PipelineBarrierInfo computeBarrier = {
		imageBarrier, static_cast<uint32_t>(std::size(imageBarrier)),
		gfx::PipelineStage::BottomOfPipe,
		gfx::PipelineStage::ComputeShader,
	};
	mDevice->PipelineBarrier(&commandList, &computeBarrier);

	gfx::DescriptorInfo descriptorInfo = {};
	descriptorInfo.resource = mBRDFTexture.get();
	descriptorInfo.mipLevel = 0;
	descriptorInfo.type = gfx::DescriptorType::Image;

	float shaderData[] = { float(mBRDFDims) };
	mDevice->UpdateDescriptor(mBRDFPipeline.get(), &descriptorInfo, 1);
	mDevice->BindPipeline(&commandList, mBRDFPipeline.get());
	mDevice->PushConstants(&commandList, mBRDFPipeline.get(), gfx::ShaderStage::Compute, shaderData, (uint32_t)(sizeof(float) * std::size(shaderData)));
	mDevice->DispatchCompute(&commandList, mBRDFDims / 32, mBRDFDims / 32, 1);
	mDevice->SubmitCommandList(&commandList);
	mDevice->WaitForGPU();
	Logger::Debug("BRDF LUT Generated");
}
