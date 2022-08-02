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
		virtual void CreateBuffer(const GPUBufferDesc* desc, GPUBuffer* out) = 0;

		virtual void CopyBuffer(GPUBuffer* dst, GPUBuffer* src) = 0;

		virtual void UpdateDescriptor(Pipeline* pipeline, DescriptorInfo* descriptorInfo, uint32_t descriptorInfoCount) = 0;


		virtual CommandList BeginCommandList() = 0;
		virtual void BeginRenderPass(CommandList* commandList, RenderPass* renderPass) = 0;
		virtual void EndRenderPass(CommandList* commandList) = 0;
		virtual void SubmitCommandList(CommandList* commandList) = 0;
		virtual void WaitForGPU() = 0;
		virtual void Present() = 0;

		//virtual void BindViewport(Viewport* viewports, int count, CommandList* commandList) = 0;
		virtual void BindPipeline(CommandList* commandList, Pipeline* pipeline)   = 0;
		virtual void BindIndexBuffer(CommandList* commandList, GPUBuffer* buffer) = 0;
		
		virtual void DrawTriangle(CommandList* commandList, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount) = 0;
		virtual void DrawTriangleIndexed(CommandList* commandList, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount) = 0;


	protected:
		ValidationMode mValidationMode = ValidationMode::Enabled;
	};

	inline GraphicsDevice*& GetDevice()
	{
		static GraphicsDevice* device = nullptr;
		return device;
	}
};