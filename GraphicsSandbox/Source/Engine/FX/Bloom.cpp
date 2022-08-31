#include "Bloom.h"
#include "../GraphicsUtils.h"

namespace fx
{
	
	Bloom::Bloom(gfx::GraphicsDevice* device, uint32_t width, uint32_t height, gfx::Format attachmentFormat) : 
		mDevice(device),
		mWidth(width),
		mHeight(height),
		mFormat(attachmentFormat)
	{
		Initialize();
	}

	void Bloom::Generate(gfx::CommandList* commandList, gfx::GPUTexture* inputTexture, float blurRadius)
	{
		GenerateDownSamples(commandList, inputTexture);

		GenerateUpSamples(commandList, blurRadius);
	}

	void Bloom::Composite(gfx::CommandList* commandList, gfx::GPUTexture* hdrTexture, float bloomStrength)
	{
		// Composite Pass
		mDevice->BeginDebugMarker(commandList, "Bloom Composite Pass");
		gfx::ImageBarrierInfo imageBarrierInfo[] = {
			gfx::ImageBarrierInfo{gfx::AccessFlag::ShaderRead, gfx::AccessFlag::ShaderReadWrite, gfx::ImageLayout::General, hdrTexture, 0, 0, 1, 1},
			gfx::ImageBarrierInfo{gfx::AccessFlag::ShaderReadWrite, gfx::AccessFlag::ShaderRead, gfx::ImageLayout::General, &mDownSampleTexture, 0, 0, 1, 1}
		};

		gfx::PipelineBarrierInfo barrier = {
			imageBarrierInfo, static_cast<uint32_t>(std::size(imageBarrierInfo)),
			gfx::PipelineStage::ComputeShader,
			gfx::PipelineStage::ComputeShader,
			};

		mDevice->PipelineBarrier(commandList, &barrier);
		gfx::DescriptorInfo descriptorInfos[] = {
			gfx::DescriptorInfo{hdrTexture, 0, 0, gfx::DescriptorType::Image},
			gfx::DescriptorInfo{&mDownSampleTexture, 0, 0, gfx::DescriptorType::Image},
		};

		uint32_t width = mWidth;
		uint32_t height = mHeight;

		float shaderData[] = {bloomStrength, 0.0f, 0.0f, 0.0f};
		mDevice->PushConstants(commandList, mUpSamplePipeline.get(), gfx::ShaderStage::Compute, shaderData, (uint32_t)(sizeof(float) * 4), 0);

		mDevice->UpdateDescriptor(mCompositePipeline.get(), descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
		mDevice->BindPipeline(commandList, mCompositePipeline.get());
		mDevice->DispatchCompute(commandList, gfx::GetWorkSize(width, 8), gfx::GetWorkSize(height, 8), 1);
		mDevice->EndDebugMarker(commandList);
	}

	void Bloom::GenerateDownSamples(gfx::CommandList* commandList, gfx::GPUTexture* brightTexture)
	{
		mDevice->BeginDebugMarker(commandList, "Bloom Downsample");

		// Generate DownSamples
		// Convert the textureFormat
		gfx::ImageBarrierInfo imageBarrierInfo[] = {
			gfx::ImageBarrierInfo{gfx::AccessFlag::ColorAttachmentWrite, gfx::AccessFlag::ShaderRead, gfx::ImageLayout::ShaderReadOptimal, brightTexture},
			gfx::ImageBarrierInfo{gfx::AccessFlag::None, gfx::AccessFlag::ShaderWrite, gfx::ImageLayout::General, &mDownSampleTexture}
		};

		gfx::PipelineBarrierInfo barrier = {
			imageBarrierInfo, static_cast<uint32_t>(std::size(imageBarrierInfo)),
			gfx::PipelineStage::ColorAttachmentOutput,
			gfx::PipelineStage::ComputeShader,
		};

		//mDevice->PipelineBarrier(commandList, &barrier);
		// Describe Descriptors
		gfx::DescriptorInfo descriptorInfos[] = {
			gfx::DescriptorInfo{brightTexture, 0, 0, gfx::DescriptorType::Image},
			gfx::DescriptorInfo{&mDownSampleTexture, 0, 0, gfx::DescriptorType::Image},
		};

		for (uint32_t i = 0; i < kMaxMipLevel; ++i)
		{
			descriptorInfos[1].mipLevel = i;
			if (i > 0)
			{
				imageBarrierInfo[0] = { gfx::AccessFlag::ShaderWrite, gfx::AccessFlag::ShaderRead };
				imageBarrierInfo[0].baseMipLevel = i - 1;
				imageBarrierInfo[0].resource = &mDownSampleTexture;
				imageBarrierInfo[1].baseMipLevel = i;

				barrier.srcStage = gfx::PipelineStage::ComputeShader;

				descriptorInfos[0].resource = &mDownSampleTexture;
				descriptorInfos[0].mipLevel = i - 1;
			}

			mDevice->PipelineBarrier(commandList, &barrier);

			uint32_t width = mWidth >> (i + 1);
			uint32_t height = mHeight >> (i + 1);
			float shaderData[] = { (float)width, (float)height, 0.0f, 0.0f};
			mDevice->PushConstants(commandList, mDownSamplePipeline.get(), gfx::ShaderStage::Compute, shaderData, (uint32_t)(sizeof(float) * 4), 0);
			mDevice->UpdateDescriptor(mDownSamplePipeline.get(), descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
			mDevice->BindPipeline(commandList, mDownSamplePipeline.get());
			mDevice->DispatchCompute(commandList, gfx::GetWorkSize(width, 8), gfx::GetWorkSize(height, 8), 1);

		}
		mDevice->EndDebugMarker(commandList);
	}

	void Bloom::GenerateUpSamples(gfx::CommandList* commandList, float blurRadius)
	{
		mDevice->BeginDebugMarker(commandList, "Bloom Upsample");
		gfx::DescriptorInfo descriptorInfos[] = {
			gfx::DescriptorInfo{&mDownSampleTexture, 0, 0, gfx::DescriptorType::Image},
			gfx::DescriptorInfo{&mDownSampleTexture, 0, 0, gfx::DescriptorType::Image},
		};

		for (uint32_t i = kMaxMipLevel - 1; i > 0; i--)
		{
			// Generate Upsamples
			gfx::ImageBarrierInfo imageBarrierInfo[] = {
				gfx::ImageBarrierInfo{gfx::AccessFlag::ShaderReadWrite, gfx::AccessFlag::ShaderRead, gfx::ImageLayout::General, &mDownSampleTexture, i, 0, 1, 1},
				gfx::ImageBarrierInfo{gfx::AccessFlag::ShaderRead, gfx::AccessFlag::ShaderReadWrite, gfx::ImageLayout::General, &mDownSampleTexture, i - 1, 0, 1, 1}
			};

			gfx::PipelineBarrierInfo barrier = {
				imageBarrierInfo, static_cast<uint32_t>(std::size(imageBarrierInfo)),
				gfx::PipelineStage::ComputeShader,
				gfx::PipelineStage::ComputeShader,
			};
			mDevice->PipelineBarrier(commandList, &barrier);

			descriptorInfos[0].mipLevel = i;
			descriptorInfos[1].mipLevel = i - 1;

			uint32_t width = mWidth >> i;
			uint32_t height = mHeight >> i;
			float shaderData[] = { (float)width, (float)height, blurRadius };
			mDevice->PushConstants(commandList, mUpSamplePipeline.get(), gfx::ShaderStage::Compute, shaderData, (uint32_t)(sizeof(float) * 3), 0);
			mDevice->UpdateDescriptor(mUpSamplePipeline.get(), descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
			mDevice->BindPipeline(commandList, mUpSamplePipeline.get());
			mDevice->DispatchCompute(commandList, gfx::GetWorkSize(width, 8), gfx::GetWorkSize(height, 8), 1);
		}
		mDevice->EndDebugMarker(commandList);
	}

	void Bloom::Initialize()
	{
		mDownSamplePipeline = std::make_shared<gfx::Pipeline>();
		gfx::CreateComputePipeline("Assets/SPIRV/downsample.comp.spv", mDevice, mDownSamplePipeline.get());

		mUpSamplePipeline = std::make_shared<gfx::Pipeline>();
		gfx::CreateComputePipeline("Assets/SPIRV/upsample.comp.spv", mDevice, mUpSamplePipeline.get());

		mCompositePipeline = std::make_shared<gfx::Pipeline>();
		gfx::CreateComputePipeline("Assets/SPIRV/bloom-final.comp.spv", mDevice, mCompositePipeline.get());

		gfx::GPUTextureDesc textureDesc = {};
		gfx::SamplerInfo samplerInfo = {};
		textureDesc.bCreateSampler = true;
		textureDesc.width = mWidth >> 1;
		textureDesc.height = mHeight >> 1;
		textureDesc.mipLevels = kMaxMipLevel;
		textureDesc.bindFlag = gfx::BindFlag::ShaderResource;
		textureDesc.format = mFormat;
		mDevice->CreateTexture(&textureDesc, &mDownSampleTexture);
	}
};
