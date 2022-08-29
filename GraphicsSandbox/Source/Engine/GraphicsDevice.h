#pragma once

#include "Graphics.h"
#include "Platform.h"

namespace gfx
{
	struct CommandList
	{
		void* internalState = nullptr;
		constexpr bool IsValid() const { return internalState != nullptr; }
	};

	class GraphicsDevice
	{
	public:
		virtual ~GraphicsDevice() = default;

		virtual bool CreateSwapchain(const SwapchainDesc* swapchainDesc, Platform::WindowType window) = 0;
		virtual void CreateRenderPass(const RenderPassDesc* desc, RenderPass* out) = 0;
		virtual void CreateGraphicsPipeline(const PipelineDesc* desc, Pipeline* out) =  0;
		virtual void CreateComputePipeline(const PipelineDesc* desc, Pipeline* out) = 0;
		virtual void CreateBuffer(const GPUBufferDesc* desc, GPUBuffer* out) = 0;
		virtual void CreateTexture(const GPUTextureDesc* desc, GPUTexture* out) = 0;
		virtual void CopyBuffer(GPUBuffer* dst, GPUBuffer* src, uint32_t dstOffset = 0) = 0;
		virtual void CopyTexture(GPUTexture* dst, GPUBuffer* src, PipelineBarrierInfo* barrier = nullptr, uint32_t arrayLevel = 0, uint32_t mipLevel = 0) = 0;
		virtual void CreateSemaphore(Semaphore* out) = 0;
		virtual void CreateFramebuffer(RenderPass* renderPass, Framebuffer* out) = 0;

		virtual void UpdateDescriptor(Pipeline* pipeline, DescriptorInfo* descriptorInfo, uint32_t descriptorInfoCount) = 0;
		virtual void PushConstants(CommandList* commandList, Pipeline* pipeline, ShaderStage shaderStages, void* value, uint32_t size, uint32_t offset = 0) = 0;

		virtual void PipelineBarrier(CommandList* commandList, PipelineBarrierInfo* barriers) = 0;
		virtual void CopyToSwapchain(CommandList* commandList, GPUTexture* texture, uint32_t arrayLevel = 0, uint32_t mipLevel = 0) = 0;

		virtual void BeginDebugMarker(CommandList* commandList, const char* name, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f) = 0;
		virtual void EndDebugMarker(CommandList* commandList) = 0;


		virtual void PrepareSwapchain(CommandList* commandList, Semaphore* acquireSemaphore) = 0;
		virtual CommandList BeginCommandList() = 0;
		virtual void BeginRenderPass(CommandList* commandList, RenderPass* renderPass, Framebuffer* fb) = 0;
		virtual void EndRenderPass(CommandList* commandList) = 0;
		virtual void SubmitCommandList(CommandList* commandList, Semaphore* signalSemaphore = nullptr) = 0;
		virtual void Present(Semaphore* waitSemaphore) = 0;
		virtual void WaitForGPU() = 0;

		//virtual void BindViewport(Viewport* viewports, int count, CommandList* commandList) = 0;
		virtual void BindPipeline(CommandList* commandList, Pipeline* pipeline)   = 0;
		virtual void BindIndexBuffer(CommandList* commandList, GPUBuffer* buffer) = 0;
		
		virtual void DrawTriangle(CommandList* commandList, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount) = 0;
		virtual void DrawTriangleIndexed(CommandList* commandList, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex) = 0;
		virtual void DrawIndexedIndirect(CommandList* commandList, GPUBuffer* indirectBuffer, uint32_t offset, uint32_t drawCount, uint32_t stride) = 0;
		virtual void DispatchCompute(CommandList* commandList, uint32_t groupCountX, uint32_t groupCountY, uint32_t workGroupZ) = 0;

		virtual void PrepareSwapchainForPresent(CommandList* commandList) = 0;
		/*
		* This function is used to determine whether the swapchain is 
		* ready or not. When window is minimized the width and height is
		* zero. So in such case the swapchain is not ready and we skip rendering
		* the frame.
		*/
		virtual bool IsSwapchainReady(RenderPass* rp) = 0;

	protected:
		ValidationMode mValidationMode = ValidationMode::Enabled;
	};

	inline GraphicsDevice*& GetDevice()
	{
		static GraphicsDevice* device = nullptr;
		return device;
	}
};