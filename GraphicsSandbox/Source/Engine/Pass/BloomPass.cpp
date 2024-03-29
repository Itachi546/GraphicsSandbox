#include "BloomPass.h"

#include "../Renderer.h"
#include "../GraphicsUtils.h"
#include "../Profiler.h"
#include "../StringConstants.h"

namespace gfx
{
	BloomPass::BloomPass(Renderer* renderer, uint32_t width, uint32_t height) : 
		mRenderer(renderer),
		mWidth(width),
		mHeight(height)
	{
		mDevice = gfx::GetDevice();
		mLightingTexture = renderer->mFrameGraphBuilder.AccessResource("lighting")->info.texture.texture;
	}

	void BloomPass::Initialize(RenderPassHandle renderPass)
	{
		ShaderPathInfo* upSamplePath = ShaderPath::get("upsample_pass");
		ShaderPathInfo* downSamplePath = ShaderPath::get("downsample_pass");
		ShaderPathInfo* compositePath = ShaderPath::get("bloom_composite_pass");

		mDownSamplePipeline = gfx::CreateComputePipeline(downSamplePath->shaders[0], mDevice);
		mUpSamplePipeline = gfx::CreateComputePipeline(upSamplePath->shaders[0], mDevice);
		mCompositePipeline = gfx::CreateComputePipeline(compositePath->shaders[0], mDevice);

		gfx::GPUTextureDesc textureDesc = {};
		gfx::SamplerInfo samplerInfo = {};
		textureDesc.bCreateSampler = true;
		textureDesc.width = mWidth >> 1;
		textureDesc.height = mHeight >> 1;
		textureDesc.mipLevels = kMaxMipLevel;
		textureDesc.bindFlag = gfx::BindFlag::ShaderResource | gfx::BindFlag::StorageImage;
		textureDesc.format = gfx::Format::R16B16G16A16_SFLOAT;
		textureDesc.bAddToBindless = false;
		mDownSampleTexture = mDevice->CreateTexture(&textureDesc);
	}

	void BloomPass::Render(CommandList* commandList, Scene* scene)
	{
		gfx::TextureHandle inputTexture = mRenderer->mFrameGraphBuilder.AccessResource("luminance")->info.texture.texture;
		GenerateDownSamples(commandList, inputTexture);

		GenerateUpSamples(commandList, mBlurRadius);

		Composite(commandList);
	}

	void BloomPass::Composite(gfx::CommandList* commandList)
	{
		// Composite Pass
		mDevice->BeginDebugLabel(commandList, "Bloom Composite Pass");
		gfx::ResourceBarrierInfo imageBarrierInfo[] = {
			gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::ShaderRead, gfx::AccessFlag::ShaderReadWrite, gfx::ImageLayout::General, mLightingTexture, 0, 0, 1, 1),
			gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::ShaderReadWrite, gfx::AccessFlag::ShaderRead, gfx::ImageLayout::General, mDownSampleTexture, 0, 0, 1, 1)
		};

		gfx::PipelineBarrierInfo barrier = {
			imageBarrierInfo, static_cast<uint32_t>(std::size(imageBarrierInfo)),
			gfx::PipelineStage::ComputeShader,
			gfx::PipelineStage::ComputeShader,
		};

		mDevice->PipelineBarrier(commandList, &barrier);
		gfx::DescriptorInfo descriptorInfos[] = {
			gfx::DescriptorInfo{&mLightingTexture, 0, 0, gfx::DescriptorType::Image},
			gfx::DescriptorInfo{&mDownSampleTexture, 0, 0, gfx::DescriptorType::Image},
		};

		uint32_t width = mWidth;
		uint32_t height = mHeight;

		float shaderData[] = { mBloomStrength, mRenderer->mEnvironmentData.exposure, 0.0f, 0.0f };
		mDevice->PushConstants(commandList, mUpSamplePipeline, gfx::ShaderStage::Compute, shaderData, (uint32_t)(sizeof(float) * 4), 0);

		mDevice->UpdateDescriptor(mCompositePipeline, descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
		mDevice->BindPipeline(commandList, mCompositePipeline);
		mDevice->DispatchCompute(commandList, gfx::GetWorkSize(width, 32), gfx::GetWorkSize(height, 32), 1);
		mDevice->EndDebugLabel(commandList);
	}

	void BloomPass::GenerateDownSamples(gfx::CommandList* commandList, gfx::TextureHandle brightTexture)
	{
		gfx::GraphicsDevice* device = gfx::GetDevice();
		device->BeginDebugLabel(commandList, "Bloom Downsample");

		// Generate DownSamples
		// Convert the textureFormat
		gfx::ResourceBarrierInfo imageBarrierInfo[] = {
			gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::ColorAttachmentWrite, gfx::AccessFlag::ShaderRead, gfx::ImageLayout::ShaderReadOptimal, brightTexture),
			gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::None, gfx::AccessFlag::ShaderWrite, gfx::ImageLayout::General, mDownSampleTexture)
		};

		gfx::PipelineBarrierInfo barrier = {
			imageBarrierInfo, static_cast<uint32_t>(std::size(imageBarrierInfo)),
			gfx::PipelineStage::ColorAttachmentOutput,
			gfx::PipelineStage::ComputeShader,
		};

		//mDevice->PipelineBarrier(commandList, &barrier);
		// Describe Descriptors
		gfx::DescriptorInfo descriptorInfos[] = {
			gfx::DescriptorInfo{&brightTexture, 0, 0, gfx::DescriptorType::Image},
			gfx::DescriptorInfo{&mDownSampleTexture, 0, 0, gfx::DescriptorType::Image},
		};

		for (uint32_t i = 0; i < kMaxMipLevel; ++i)
		{
			descriptorInfos[1].mipLevel = i;
			if (i > 0)
			{
				imageBarrierInfo[0] = { gfx::AccessFlag::ShaderWrite, gfx::AccessFlag::ShaderRead };
				imageBarrierInfo[0].resourceInfo.texture.baseMipLevel = i - 1;
				imageBarrierInfo[0].resourceInfo.texture.texture = mDownSampleTexture;
				imageBarrierInfo[0].resourceInfo.texture.mipCount = 1;
				imageBarrierInfo[0].resourceInfo.texture.layerCount = ~0u;

				imageBarrierInfo[1].resourceInfo.texture.baseMipLevel = i;

				barrier.srcStage = gfx::PipelineStage::ComputeShader;

				descriptorInfos[0].texture = &mDownSampleTexture;
				descriptorInfos[0].mipLevel = i - 1;
			}

			device->PipelineBarrier(commandList, &barrier);

			uint32_t width = mWidth >> (i + 1);
			uint32_t height = mHeight >> (i + 1);
			float shaderData[] = { (float)width, (float)height, 0.0f, 0.0f };
			device->PushConstants(commandList, mDownSamplePipeline, gfx::ShaderStage::Compute, shaderData, (uint32_t)(sizeof(float) * 4), 0);
			device->UpdateDescriptor(mDownSamplePipeline, descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
			device->BindPipeline(commandList, mDownSamplePipeline);
			device->DispatchCompute(commandList, gfx::GetWorkSize(width, 8), gfx::GetWorkSize(height, 8), 1);

		}
		device->EndDebugLabel(commandList);
	}

	void BloomPass::GenerateUpSamples(gfx::CommandList* commandList, float blurRadius)
	{
		mDevice->BeginDebugLabel(commandList, "Bloom Upsample");
		gfx::DescriptorInfo descriptorInfos[] = {
			gfx::DescriptorInfo{&mDownSampleTexture, 0, 0, gfx::DescriptorType::Image},
			gfx::DescriptorInfo{&mDownSampleTexture, 0, 0, gfx::DescriptorType::Image},
		};

		for (uint32_t i = kMaxMipLevel - 1; i > 0; i--)
		{
			// Generate Upsamples
			gfx::ResourceBarrierInfo imageBarrierInfo[] = {
				gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::ShaderReadWrite, gfx::AccessFlag::ShaderRead, gfx::ImageLayout::General, mDownSampleTexture, i, 0, 1, 1),
				gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::ShaderRead, gfx::AccessFlag::ShaderReadWrite, gfx::ImageLayout::General, mDownSampleTexture, i - 1, 0, 1, 1)
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
			mDevice->PushConstants(commandList, mUpSamplePipeline, gfx::ShaderStage::Compute, shaderData, (uint32_t)(sizeof(float) * 3), 0);
			mDevice->UpdateDescriptor(mUpSamplePipeline, descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
			mDevice->BindPipeline(commandList, mUpSamplePipeline);
			mDevice->DispatchCompute(commandList, gfx::GetWorkSize(width, 8), gfx::GetWorkSize(height, 8), 1);
		}
		mDevice->EndDebugLabel(commandList);
	}


	void BloomPass::Shutdown()
	{
		gfx::GraphicsDevice* device = gfx::GetDevice();
		device->Destroy(mDownSampleTexture);
		device->Destroy(mDownSamplePipeline);
		device->Destroy(mUpSamplePipeline);
		device->Destroy(mCompositePipeline);
	}

}
