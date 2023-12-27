#include "BlurPass.h"

#include "../Renderer.h"
#include "../Utils.h"
#include "../StringConstants.h"
#include "../GraphicsUtils.h"

namespace gfx {

	BlurPass::BlurPass(Renderer* renderer, uint32_t width, uint32_t height) : width(width), height(height)
	{
	}

	void BlurPass::Initialize(RenderPassHandle renderPass)
	{
		ShaderPathInfo* shaderPathInfo = ShaderPath::get("blur_pass");
		pipeline = gfx::CreateComputePipeline(shaderPathInfo->shaders[0], gfx::GetDevice());
	}

	void BlurPass::Render(CommandList* commandList, Scene* scene)
	{
		descriptorInfos[0] = { &inputTexture, 0, 0, gfx::DescriptorType::Image };
		descriptorInfos[1] = { &outputTexture, 0, 0, gfx::DescriptorType::Image };

		gfx::ResourceBarrierInfo barrierInfos[] = { gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::ColorAttachmentWrite,
			gfx::AccessFlag::ShaderRead,
			gfx::ImageLayout::ShaderReadOptimal,
			inputTexture),
			{ gfx::ResourceBarrierInfo::CreateImageBarrier(gfx::AccessFlag::None,
			gfx::AccessFlag::ShaderWrite,
			gfx::ImageLayout::General,
			outputTexture)
		} };

		gfx::PipelineBarrierInfo pipelineBarrierInfo = gfx::PipelineBarrierInfo{
			barrierInfos,
			(uint32_t)std::size(barrierInfos),
			gfx::PipelineStage::ColorAttachmentOutput,
			gfx::PipelineStage::ComputeShader
		};

		gfx::GraphicsDevice* device = gfx::GetDevice();
		device->PipelineBarrier(commandList, &pipelineBarrierInfo);

		device->UpdateDescriptor(pipeline, descriptorInfos, (uint32_t)std::size(descriptorInfos));

		uint32_t pushConstants[] = { width, height };
		device->PushConstants(commandList, pipeline, ShaderStage::Compute, pushConstants, static_cast<uint32_t>(sizeof(uint32_t) * 2));

		device->BindPipeline(commandList, pipeline);

		uint32_t workGroupX = gfx::GetWorkSize(width, 8);
		uint32_t wworkGroupY = gfx::GetWorkSize(height, 8);
		device->DispatchCompute(commandList, workGroupX, wworkGroupY, 1);
	}

	void BlurPass::Shutdown()
	{
		gfx::GetDevice()->Destroy(pipeline);
	}
}