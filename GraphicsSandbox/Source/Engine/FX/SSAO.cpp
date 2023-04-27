#include "SSAO.h"
#include "../../Shared/MathUtils.h"
#include "../Interpolator.h"
#include "../GraphicsUtils.h"
#include "../StringConstants.h"

namespace fx
{
	SSAO::SSAO(gfx::GraphicsDevice* device, uint32_t width, uint32_t height) : 
		mWidth(width), mHeight(height), mDevice(device)
	{
		Initialize();
	}

	void SSAO::Generate(gfx::CommandList* commandList, gfx::TextureHandle depthTexture, float blurRadius)
	{
		mDevice->BeginDebugMarker(commandList, "SSAO");
		gfx::DescriptorInfo descriptorInfos[] = {
			gfx::DescriptorInfo{&depthTexture, 0, 0, gfx::DescriptorType::Image},
			gfx::DescriptorInfo{&mNoiseTexture, 0, 0, gfx::DescriptorType::Image},
			gfx::DescriptorInfo{mKernelBuffer, 0, 0, gfx::DescriptorType::UniformBuffer}
		};
		/*
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
		*/
		mDevice->UpdateDescriptor(mPipeline, descriptorInfos, static_cast<uint32_t>(std::size(descriptorInfos)));
		mDevice->BindPipeline(commandList, mPipeline);
		mDevice->DispatchCompute(commandList, gfx::GetWorkSize(mWidth, 8), gfx::GetWorkSize(mHeight, 8), 1);

		mDevice->EndDebugMarker(commandList);
	}

	void SSAO::Shutdown()
	{
		mDevice->Destroy(mPipeline);
		mDevice->Destroy(mTexture);
		mDevice->Destroy(mNoiseTexture);
		mDevice->Destroy(mKernelBuffer);
	}

	void SSAO::Initialize()
	{
		// Generate random vector around the hemisphere
		// More concentrated towards the origin 
		std::array<glm::vec3, 64> sampleKernel;
		for (uint32_t i = 0; i < sampleKernel.size(); ++i)
		{
			glm::vec3 sample(MathUtils::Rand01() * 2.0f - 1.0f,
				MathUtils::Rand01() * 2.0f - 1.0f,
				MathUtils::Rand01());
			sample = glm::normalize(sample);

			float scale = float(i) / 64.0f;
			// Ease-In
			scale = Interpolator::Lerp(0.1f, 1.0f, scale * scale);
			sample *= scale;
			sampleKernel[i] = sample;
		}
		// Transfer kernel data to uniform buffer
		{
			uint32_t kernelDataSize = static_cast<uint32_t>(sizeof(glm::vec3) * sampleKernel.size());
			gfx::GPUBufferDesc desc;
			desc.bindFlag = gfx::BindFlag::ConstantBuffer;
			desc.size = kernelDataSize;
			desc.usage = gfx::Usage::Upload;
			mKernelBuffer = mDevice->CreateBuffer(&desc);
			mDevice->CopyToCPUBuffer(mKernelBuffer, sampleKernel.data(), 0, kernelDataSize);
		}

		// Generate rotation vector in tangent space
		std::array<glm::vec3, 16> sampleRotation;
		for (uint32_t i = 0; i < 16; ++i)
		{
			sampleRotation[i] = glm::vec3(MathUtils::Rand01() * 2.0f - 1.0f,
				MathUtils::Rand01() * 2.0f - 1.0f,
				0);;
		}

		// Generate rotation texture and copy data to it
		{
			gfx::GPUTextureDesc textureDesc = {};
			gfx::SamplerInfo samplerInfo = {};
			textureDesc.bCreateSampler = true;
			textureDesc.width = 4;
			textureDesc.height = 4;
			textureDesc.bindFlag = gfx::BindFlag::ShaderResource;
			textureDesc.format = gfx::Format::R16B16G16_SFLOAT;
			textureDesc.bAddToBindless = false;
			mNoiseTexture = mDevice->CreateTexture(&textureDesc);

			uint32_t imageDataSize = static_cast<uint32_t>(sizeof(glm::vec3) * sampleRotation.size());
			//mDevice->CopyTexture(mNoiseTexture, sampleRotation.data(), imageDataSize);
		}

		mPipeline = gfx::CreateComputePipeline(StringConstants::SSAO_COMP_PATH, mDevice);

		// Generate SSAO output texture
		{
			gfx::GPUTextureDesc textureDesc = {};
			gfx::SamplerInfo samplerInfo = {};
			textureDesc.bCreateSampler = true;
			textureDesc.width = mWidth;
			textureDesc.height = mHeight;
			textureDesc.bindFlag = gfx::BindFlag::ShaderResource | gfx::BindFlag::StorageImage;
			textureDesc.format = gfx::Format::R16_SFLOAT;
			textureDesc.bAddToBindless = false;
			mTexture = mDevice->CreateTexture(&textureDesc);
		}

	}
}
