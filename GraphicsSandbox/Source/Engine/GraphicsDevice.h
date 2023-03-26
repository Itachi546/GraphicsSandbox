#pragma once

#include "Graphics.h"
#include "Platform.h"

namespace gfx
{
	class GraphicsDevice
	{
	public:
		virtual ~GraphicsDevice() = default;

		virtual bool CreateSwapchain(const SwapchainDesc* swapchainDesc, Platform::WindowType window) = 0;
		virtual RenderPassHandle CreateRenderPass(const RenderPassDesc* desc) = 0;
		virtual PipelineHandle CreateGraphicsPipeline(const PipelineDesc* desc) =  0;
		virtual PipelineHandle CreateComputePipeline(const PipelineDesc* desc) = 0;
		virtual BufferHandle CreateBuffer(const GPUBufferDesc* desc) = 0;

		virtual void CopyToBuffer(BufferHandle buffer, void* data, uint32_t offset, uint32_t size) = 0;
		virtual void CreateTexture(const GPUTextureDesc* desc, GPUTexture* out) = 0;
		virtual void CopyBuffer(BufferHandle dst, BufferHandle src, uint32_t dstOffset = 0) = 0;
		virtual void CopyTexture(GPUTexture* dst, BufferHandle src, PipelineBarrierInfo* barrier = nullptr, uint32_t arrayLevel = 0, uint32_t mipLevel = 0) = 0;
		virtual void CopyTexture(GPUTexture* dst, void* src, uint32_t sizeInByte, uint32_t arrayLevel = 0, uint32_t mipLevel = 0, bool generateMipMap = false) = 0;
		virtual void CreateSemaphore(Semaphore* out) = 0;
		virtual void CreateFramebuffer(RenderPassHandle renderPass, Framebuffer* out, uint32_t layerCount) = 0;
		virtual void CreateQueryPool(QueryPool* out, uint32_t count, QueryType type) = 0;
		
		virtual void* GetMappedDataPtr(BufferHandle buffer) = 0;
		virtual uint32_t GetBufferSize(BufferHandle handle) = 0;

		virtual void GenerateMipmap(GPUTexture* src, uint32_t mipCount) = 0;

		virtual void ResetQueryPool(CommandList* commandList, QueryPool* pool, uint32_t first, uint32_t count) = 0;
		virtual void Query(CommandList* commandList, QueryPool* pool, uint32_t index) = 0;
		virtual void ResolveQuery(QueryPool* pool, uint32_t index, uint32_t count, uint64_t* result) = 0;
		virtual double GetTimestampFrequency() = 0;

		virtual void UpdateDescriptor(PipelineHandle pipeline, DescriptorInfo* descriptorInfo, uint32_t descriptorInfoCount) = 0;
		virtual void PushConstants(CommandList* commandList, PipelineHandle pipeline, ShaderStage shaderStages, void* value, uint32_t size, uint32_t offset = 0) = 0;

		virtual void PipelineBarrier(CommandList* commandList, PipelineBarrierInfo* barriers) = 0;
		virtual void CopyToSwapchain(CommandList* commandList, GPUTexture* texture, ImageLayout finalSwapchainImageLayout, uint32_t arrayLevel = 0, uint32_t mipLevel = 0) = 0;

		virtual void BeginDebugMarker(CommandList* commandList, const char* name, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f) = 0;
		virtual void EndDebugMarker(CommandList* commandList) = 0;


		virtual void PrepareSwapchain(CommandList* commandList, Semaphore* acquireSemaphore) = 0;
		virtual CommandList BeginCommandList() = 0;
		virtual void BeginRenderPass(CommandList* commandList, RenderPassHandle renderPass, Framebuffer* fb) = 0;
		virtual void EndRenderPass(CommandList* commandList) = 0;
		virtual void SubmitCommandList(CommandList* commandList, Semaphore* signalSemaphore = nullptr) = 0;
		virtual void Present(Semaphore* waitSemaphore) = 0;
		virtual void WaitForGPU() = 0;

		//virtual void BindViewport(Viewport* viewports, int count, CommandList* commandList) = 0;
		virtual void BindPipeline(CommandList* commandList, PipelineHandle pipeline)   = 0;
		virtual void BindIndexBuffer(CommandList* commandList, BufferHandle buffer) = 0;
		
		virtual void Draw(CommandList* commandList, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount) = 0;
		virtual void DrawIndexed(CommandList* commandList, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex) = 0;
		virtual void DrawIndexedIndirect(CommandList* commandList, BufferHandle indirectBuffer, uint32_t offset, uint32_t drawCount, uint32_t stride) = 0;
		virtual void DispatchCompute(CommandList* commandList, uint32_t groupCountX, uint32_t groupCountY, uint32_t workGroupZ) = 0;

		virtual void PrepareSwapchainForPresent(CommandList* commandList) = 0;
		/*
		* This function is used to determine whether the swapchain is 
		* ready or not. When window is minimized the width and height is
		* zero. So in such case the swapchain is not ready and we skip rendering
		* the frame.
		*/
		virtual bool IsSwapchainReady(RenderPassHandle rp) = 0;

		virtual void Destroy(RenderPassHandle renderPass) = 0;
		virtual void Destroy(PipelineHandle pipeline) = 0;

	protected:
		ValidationMode mValidationMode = ValidationMode::Enabled;
	};

	inline GraphicsDevice*& GetDevice()
	{
		static GraphicsDevice* device = nullptr;
		return device;
	}
};