#include "EnvironmentMap.h"
#include "GraphicsUtils.h"
#include "ImageLoader.h"
#include "Logger.h"
#include "StringConstants.h"
#include <assert.h>

EnvironmentMap::EnvironmentMap()
{
	mDevice = gfx::GetDevice();

	mHdriToCubemap = gfx::CreateComputePipeline(StringConstants::HDRI_CONVERTER_COMP_PATH, mDevice);
	mIrradiancePipeline = gfx::CreateComputePipeline(StringConstants::IRRADIANCE_COMP_PATH, mDevice);
	mPrefilterPipeline = gfx::CreateComputePipeline(StringConstants::PREFILTER_COMP_PATH, mDevice);
	mBRDFPipeline = gfx::CreateComputePipeline(StringConstants::BRDF_LUT_COMP_PATH, mDevice);

}

void EnvironmentMap::CreateFromHDRI(const char* hdri)
{
	Logger::Debug("Converting hdri: " + std::string(hdri));

	// Load HDRI Image File 
	int width, height, nComponent;
	float* hdriData = Utils::ImageLoader::LoadHDRI(hdri, width, height, nComponent);

	// Create Source HDRI Texture 
	gfx::GPUTextureDesc hdriDesc{};
	hdriDesc.width = width;
	hdriDesc.height = height;
	hdriDesc.bindFlag = gfx::BindFlag::StorageImage | gfx::BindFlag::ShaderResource;
	hdriDesc.imageViewType = gfx::ImageViewType::IV2D;
	hdriDesc.format = gfx::Format::R32B32G32A32_SFLOAT;
	hdriDesc.bCreateSampler = true;
	hdriDesc.bAddToBindless = false;
	gfx::TextureHandle hdriTexture = mDevice->CreateTexture(&hdriDesc);

	// Create Staging buffer to transfer hdri data
	const uint32_t imageDataSize = width * height * 4 * sizeof(float);
	gfx::GPUBufferDesc bufferDesc;
	bufferDesc.bindFlag = gfx::BindFlag::None;
	bufferDesc.usage = gfx::Usage::Upload;
	bufferDesc.size = imageDataSize;
	gfx::BufferHandle stagingBuffer = mDevice->CreateBuffer(&bufferDesc);
	mDevice->CopyToBuffer(stagingBuffer, hdriData, 0, imageDataSize);

	// Copy from staging buffer to hdri texture
	gfx::ResourceBarrierInfo transferBarrier = gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::None, gfx::AccessFlag::TransferWriteBit,gfx::ImageLayout::TransferDstOptimal, gfx::INVALID_TEXTURE);
	gfx::PipelineBarrierInfo transferBarrierInfo = {
		&transferBarrier, 1,
		gfx::PipelineStage::Transfer,
		gfx::PipelineStage::Transfer
	};
	mDevice->CopyTexture(hdriTexture, stagingBuffer, &transferBarrierInfo, 0, 0);
	Utils::ImageLoader::Free(hdriData);

	// Destroy intermediate resources
	mDevice->Destroy(stagingBuffer);

	// Create Destination Cubemap texture
	gfx::GPUTextureDesc cubemapDesc{};
	cubemapDesc.width = cubemapDesc.height = mCubemapDims;
	cubemapDesc.bindFlag = gfx::BindFlag::StorageImage | gfx::BindFlag::ShaderResource;
	cubemapDesc.imageViewType = gfx::ImageViewType::IVCubemap;
	cubemapDesc.arrayLayers = 6;
	cubemapDesc.format = gfx::Format::R16B16G16A16_SFLOAT;
	cubemapDesc.bCreateSampler = true;
	cubemapDesc.bAddToBindless = true;
	mCubemapTexture = mDevice->CreateTexture(&cubemapDesc); 

	// Begin Compute Shader
	gfx::CommandList commandList = mDevice->BeginCommandList();

	// Layout transition for shader read/write
	gfx::ResourceBarrierInfo imageBarrier[] = { 
		gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::None, gfx::AccessFlag::ShaderRead, gfx::ImageLayout::ShaderReadOptimal, hdriTexture),
		gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::None, gfx::AccessFlag::ShaderWrite, gfx::ImageLayout::General, mCubemapTexture)
	};

	gfx::PipelineBarrierInfo computeBarrier = {
		imageBarrier, static_cast<uint32_t>(std::size(imageBarrier)),
		gfx::PipelineStage::BottomOfPipe,
		gfx::PipelineStage::ComputeShader,
	};
	mDevice->PipelineBarrier(&commandList, &computeBarrier);

	// Bind resources
	gfx::DescriptorInfo descriptorInfos[2] = {};
	descriptorInfos[0].texture = &hdriTexture;
	descriptorInfos[0].offset = 0;
	descriptorInfos[0].type = gfx::DescriptorType::Image;

	descriptorInfos[1].texture = &mCubemapTexture;
	descriptorInfos[1].offset = 0;
	descriptorInfos[1].type = gfx::DescriptorType::Image;

	float shaderData[] = { (float)mCubemapDims };
	mDevice->UpdateDescriptor(mHdriToCubemap, descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
	mDevice->BindPipeline(&commandList, mHdriToCubemap);
	mDevice->PushConstants(&commandList, mHdriToCubemap, gfx::ShaderStage::Compute, shaderData, sizeof(float) * static_cast<uint32_t>(std::size(shaderData)));
	mDevice->DispatchCompute(&commandList, gfx::GetWorkSize(mCubemapDims, 32), gfx::GetWorkSize(mCubemapDims, 32), 6);
	mDevice->SubmitComputeLoad(&commandList);
	mDevice->WaitForGPU();
	Logger::Debug("HDRI Converted Successfully");

	mDevice->Destroy(hdriTexture);
}

void EnvironmentMap::CalculateIrradiance()
{
	Logger::Debug("Generating Irradiance Texture");
	assert(mCubemapTexture.handle != gfx::K_INVALID_RESOURCE_HANDLE);

	gfx::GPUTextureDesc irrDesc{};
	irrDesc.width = irrDesc.height = mIrrTexDims;
	irrDesc.bindFlag = gfx::BindFlag::ShaderResource | gfx::BindFlag::StorageImage;
	irrDesc.imageViewType = gfx::ImageViewType::IVCubemap;
	irrDesc.arrayLayers = 6;
	irrDesc.format = gfx::Format::R16B16G16A16_SFLOAT;
	irrDesc.bCreateSampler = true;
	irrDesc.bAddToBindless = true;
	mIrradianceTexture = mDevice->CreateTexture(&irrDesc);

	// Begin Compute Shader
	gfx::CommandList commandList = mDevice->BeginCommandList();
	// Layout transition for shader read/write
	gfx::ResourceBarrierInfo imageBarrier[] = {
		gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::None, gfx::AccessFlag::ShaderWrite, gfx::ImageLayout::General, mIrradianceTexture),
	};

	gfx::PipelineBarrierInfo computeBarrier = {
		imageBarrier, static_cast<uint32_t>(std::size(imageBarrier)),
		gfx::PipelineStage::BottomOfPipe,
		gfx::PipelineStage::ComputeShader,
	};

	mDevice->PipelineBarrier(&commandList, &computeBarrier);

	// Bind resources
	gfx::DescriptorInfo descriptorInfos[2] = {};
	descriptorInfos[0].texture = &mCubemapTexture;
	descriptorInfos[0].offset = 0;
	descriptorInfos[0].type = gfx::DescriptorType::Image;

	descriptorInfos[1].texture = &mIrradianceTexture;
	descriptorInfos[1].offset = 0;
	descriptorInfos[1].type = gfx::DescriptorType::Image;

	float shaderData[] = { (float)mIrrTexDims };
	mDevice->UpdateDescriptor(mIrradiancePipeline, descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
	mDevice->BindPipeline(&commandList, mIrradiancePipeline);
	mDevice->PushConstants(&commandList, mHdriToCubemap, gfx::ShaderStage::Compute, shaderData, sizeof(float) * static_cast<uint32_t>(std::size(shaderData)));
	mDevice->DispatchCompute(&commandList, gfx::GetWorkSize(mIrrTexDims, 8), gfx::GetWorkSize(mIrrTexDims, 8), 6);
	mDevice->SubmitComputeLoad(&commandList);
	mDevice->WaitForGPU();
	Logger::Debug("Irradiance Texture Generated");
}

void EnvironmentMap::Prefilter()
{
	Logger::Debug("Generating Prefiltered Environment Texture");
	assert(mCubemapTexture.handle != gfx::K_INVALID_RESOURCE_HANDLE);

	gfx::GPUTextureDesc desc{};
	desc.width = desc.height = mPrefilterDims;
	desc.bindFlag = gfx::BindFlag::ShaderResource | gfx::BindFlag::StorageImage;
	desc.imageViewType = gfx::ImageViewType::IVCubemap;
	desc.arrayLayers = 6;
	desc.mipLevels = nMaxMipLevel;
	desc.format = gfx::Format::R16B16G16A16_SFLOAT;
	desc.bCreateSampler = true;
	desc.bAddToBindless = true;
	mPrefilterTexture = mDevice->CreateTexture(&desc);

	// Begin Compute Shader
	gfx::CommandList commandList = mDevice->BeginCommandList();

	// Layout transition for shader read/write
	gfx::ResourceBarrierInfo imageBarrier[] = {
		gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::None, gfx::AccessFlag::ShaderWrite, gfx::ImageLayout::General, mPrefilterTexture),
	};

	gfx::PipelineBarrierInfo computeBarrier = {
		imageBarrier, static_cast<uint32_t>(std::size(imageBarrier)),
		gfx::PipelineStage::BottomOfPipe,
		gfx::PipelineStage::ComputeShader,
	};

	mDevice->PipelineBarrier(&commandList, &computeBarrier);

	// Bind resources
	gfx::DescriptorInfo descriptorInfos[2] = {};
	descriptorInfos[0].texture = &mCubemapTexture;
	descriptorInfos[0].offset = 0;
	descriptorInfos[0].type = gfx::DescriptorType::Image;

	uint32_t dims = mPrefilterDims;
	for (uint32_t i = 0; i < nMaxMipLevel; ++i)
	{
		float roughness = i / float(nMaxMipLevel - 1);
		descriptorInfos[1].texture = &mPrefilterTexture;
		descriptorInfos[1].mipLevel = i;
		descriptorInfos[1].type = gfx::DescriptorType::Image;

		float shaderData[] = { (float)dims, roughness };

		mDevice->UpdateDescriptor(mPrefilterPipeline, descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
		mDevice->BindPipeline(&commandList, mPrefilterPipeline);
		mDevice->PushConstants(&commandList, mPrefilterPipeline, gfx::ShaderStage::Compute, shaderData, (uint32_t)(sizeof(float) * std::size(shaderData)));
		mDevice->DispatchCompute(&commandList, gfx::GetWorkSize(dims, 8), gfx::GetWorkSize(dims, 8), 6);
		dims /= 2;
	}
	mDevice->SubmitComputeLoad(&commandList);
	mDevice->WaitForGPU();
	Logger::Debug("Prefiltered Environment Texture Generated");
}

void EnvironmentMap::CalculateBRDFLUT()
{
	Logger::Debug("Generating BRDF LUT");
	assert(mCubemapTexture.handle != gfx::K_INVALID_RESOURCE_HANDLE);

	gfx::GPUTextureDesc desc{};
	desc.width = desc.height = mBRDFDims;
	desc.bindFlag = gfx::BindFlag::ShaderResource | gfx::BindFlag::StorageImage;
	desc.imageViewType = gfx::ImageViewType::IV2D;
	desc.format = gfx::Format::R16G16_SFLOAT;
	desc.imageAspect = gfx::ImageAspect::Color;
	desc.bCreateSampler = true;
	desc.bAddToBindless = true;
	mBRDFTexture = mDevice->CreateTexture(&desc);

	// Begin Compute Shader
	gfx::CommandList commandList = mDevice->BeginCommandList();

	// Layout transition for shader read/write
	gfx::ResourceBarrierInfo imageBarrier[] = {
		gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::None, gfx::AccessFlag::ShaderWrite, gfx::ImageLayout::General, mBRDFTexture),
	};

	gfx::PipelineBarrierInfo computeBarrier = {
		imageBarrier, static_cast<uint32_t>(std::size(imageBarrier)),
		gfx::PipelineStage::BottomOfPipe,
		gfx::PipelineStage::ComputeShader,
	};
	mDevice->PipelineBarrier(&commandList, &computeBarrier);

	gfx::DescriptorInfo descriptorInfo = {};
	descriptorInfo.texture = &mBRDFTexture;
	descriptorInfo.mipLevel = 0;
	descriptorInfo.type = gfx::DescriptorType::Image;

	float shaderData[] = { float(mBRDFDims) };
	mDevice->UpdateDescriptor(mBRDFPipeline, &descriptorInfo, 1);
	mDevice->BindPipeline(&commandList, mBRDFPipeline);
	mDevice->PushConstants(&commandList, mBRDFPipeline, gfx::ShaderStage::Compute, shaderData, (uint32_t)(sizeof(float) * std::size(shaderData)));
	mDevice->DispatchCompute(&commandList, gfx::GetWorkSize(mBRDFDims, 32), gfx::GetWorkSize(mBRDFDims, 32), 1);

	// Layout transition for shader read/write
	gfx::ResourceBarrierInfo imageBarrier2[] = {
		gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::ShaderWrite, gfx::AccessFlag::ShaderRead, gfx::ImageLayout::ShaderReadOptimal, mBRDFTexture),
	};

	gfx::PipelineBarrierInfo computeBarrier2 = {
		imageBarrier, static_cast<uint32_t>(std::size(imageBarrier2)),
		gfx::PipelineStage::ComputeShader,
		gfx::PipelineStage::FragmentShader,
	};
	mDevice->PipelineBarrier(&commandList, &computeBarrier2);

	mDevice->SubmitComputeLoad(&commandList);
	mDevice->WaitForGPU();
	Logger::Debug("BRDF LUT Generated");
}

void EnvironmentMap::Shutdown()
{
	mDevice->Destroy(mHdriToCubemap);
	mDevice->Destroy(mIrradiancePipeline);
	mDevice->Destroy(mPrefilterPipeline);
	mDevice->Destroy(mBRDFPipeline);

	mDevice->Destroy(mCubemapTexture);
	mDevice->Destroy(mIrradianceTexture);
	mDevice->Destroy(mPrefilterTexture);
	mDevice->Destroy(mBRDFTexture);
}
