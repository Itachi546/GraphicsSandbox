#pragma once

#include "Platform.h"
#ifdef PLATFORM_WINDOW
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include "CommonInclude.h"
#include "GraphicsDevice.h"
#include "Utils.h"
#include "EventDispatcher.h"
#include "VulkanResources.h"
#include "ResourcePool.h"
#include <assert.h>

#define VK_CHECK(result)\
if(result != VK_SUCCESS){\
   assert(false);\
}\

namespace gfx
{
	class VulkanGraphicsDevice : public GraphicsDevice
	{
	public:
		VulkanGraphicsDevice(Platform::WindowType window, ValidationMode validationMode = ValidationMode::Enabled);

		VulkanGraphicsDevice(const VulkanGraphicsDevice&) = delete;
		void operator=(const VulkanGraphicsDevice&) = delete;

		bool               CreateSwapchain(Platform::WindowType window)  override;
		RenderPassHandle   CreateRenderPass(const RenderPassDesc* desc)                                      override;
		PipelineHandle     CreateGraphicsPipeline(const PipelineDesc* desc)                                  override;
		PipelineHandle     CreateComputePipeline(const PipelineDesc* desc)                                   override;
		BufferHandle       CreateBuffer(const GPUBufferDesc* desc)                                           override;
		TextureHandle      CreateTexture(const GPUTextureDesc* desc)                                         override;
		//SemaphoreHandle    CreateSemaphore(const SemaphoreDesc* desc)                                        override;
		FramebufferHandle  CreateFramebuffer(const FramebufferDesc* desc)                                    override;
		void               CreateQueryPool(QueryPool* out, uint32_t count, QueryType type)                   override;

		void ResetQueryPool(CommandList* commandList, QueryPool* pool, uint32_t first, uint32_t count)                   override;

		void Query(CommandList* commandList, QueryPool* pool, uint32_t index)                  override;
		void ResolveQuery(QueryPool* pool, uint32_t index, uint32_t count, uint64_t* result) override;

		double GetTimestampFrequency() override { return properties2_.properties.limits.timestampPeriod; }

		void CopyToSwapchain(CommandList* commandList, TextureHandle texture, ImageLayout finalSwapchainImageLayout, uint32_t arrayLevel = 0, uint32_t mipLevel = 0) override;

		void CopyToBuffer(BufferHandle buffer, void* data, uint32_t offset, uint32_t size) override;
		void CopyBuffer(BufferHandle dst, BufferHandle src, uint32_t dstOffset = 0)                override;
		void CopyTexture(TextureHandle dst, BufferHandle src, PipelineBarrierInfo* barrier, uint32_t arrayLevel = 0, uint32_t mipLevel = 0) override;
		void CopyTexture(TextureHandle dst, void* src, uint32_t sizeInByte, uint32_t arrayLevel = 0, uint32_t mipLevel = 0, bool generateMipMap = false) override;
		void PipelineBarrier(CommandList* commandList, PipelineBarrierInfo* barriers)          override;

		void* GetMappedDataPtr(BufferHandle buffer) override;
		uint32_t GetBufferSize(BufferHandle handle) override;

		void GenerateMipmap(TextureHandle src, uint32_t mipCount)                                override;

		CommandList BeginCommandList()                                                         override;

		void BeginFrame() override;
		void PrepareSwapchain(CommandList* commandList)             override;
		void BeginRenderPass(CommandList* commandList, RenderPassHandle renderPass, FramebufferHandle fb)  override;
		void EndRenderPass(CommandList* commandList)                                             override;
		void SubmitComputeLoad(CommandList* commandList)                                         override;
		void Present(CommandList* commandList)                                                   override;
		void WaitForGPU()                                                                        override;
		void PrepareSwapchainForPresent(CommandList* commandList)                                override;

		void BindPipeline(CommandList* commandList, PipelineHandle pipeline)                        override;
		void BindIndexBuffer(CommandList* commandList, BufferHandle buffer)                      override;

		void BeginDebugLabel(CommandList* commandList, const char* name, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f) override;
		void EndDebugLabel(CommandList* commandList) override;
		// Dynamic field allows to create new descriptor set 
		// However it shouldn't be used in loop because there  
		// is currently no way to free allocated descriptor set
		// except by destroying descriptor pool.
		void UpdateDescriptor(PipelineHandle pipeline, DescriptorInfo* descriptorInfo, uint32_t descriptorInfoCount)    override;
		void PushConstants(CommandList* commandList, PipelineHandle pipeline, ShaderStage shaderStages, void* value, uint32_t size, uint32_t offset = 0) override;
		void Draw(CommandList* commandList, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount)         override;
		void DrawIndexed(CommandList* commandList, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex)  override;
		void DrawIndexedIndirect(CommandList* commandList, BufferHandle indirectBuffer, uint32_t offset, uint32_t drawCount, uint32_t stride) override;
		void DispatchCompute(CommandList* commandList, uint32_t groupCountX, uint32_t groupCountY, uint32_t workGroupZ)         override;

		bool IsSwapchainReady() override;

		VkInstance GetInstance() { return instance_; }
		VkDevice GetDevice() { return device_; }
		VkPhysicalDevice GetPhysicalDevice() { return physicalDevice_; }
		VkQueue GetQueue() { return queue_; }
		uint32_t GetSwapchainImageCount() { return swapchain_->imageCount; }

		VkRenderPass GetSwapchainRenderPass();
		VkCommandBuffer Get(CommandList* commandList);
		VkDescriptorPool GetDescriptorPool() { return descriptorPools_[swapchain_->imageCount]; }

		void Destroy(RenderPassHandle renderPass) override;
		void Destroy(PipelineHandle pipeline) override;
		void Destroy(BufferHandle buffer) override;
		void Destroy(TextureHandle texture) override;
		void Destroy(FramebufferHandle framebuffer) override;
		//void Destroy(SemaphoreHandle semaphore) override;

		TextureHandle GetFramebufferAttachment(FramebufferHandle, uint32_t index) override;

		void Shutdown() override;
		virtual ~VulkanGraphicsDevice() = default;
	private:
		VkInstance instance_ = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
		VkDevice device_ = VK_NULL_HANDLE;
		VkQueue queue_ = VK_NULL_HANDLE;

		bool debugMarkerEnabled_ = false;

		// Feature support flag
		bool supportBindless = false;
		bool supportTimelineSemaphore = false;
		bool supportSynchronization2 = false;
		bool supportMeshShader = false;

		VkCommandPool* commandPool_ = nullptr;
		VkCommandBuffer* commandBuffer_ = nullptr;
		VkCommandPool   stagingCmdPool_ = VK_NULL_HANDLE;
		VkCommandBuffer stagingCmdBuffer_ = VK_NULL_HANDLE;
		uint32_t previousFrame = 0;
		uint32_t currentFrame = 0;
		uint32_t lastComputeSemaphoreValue = 0;
		uint32_t absoluteFrame = 0;

		std::vector<VkDescriptorPool> descriptorPools_;

		// Bindless Resources
		const uint32_t kMaxBindlessResources = 16536;
		const uint32_t kBindlessTextureBinding = 10;
		const uint32_t kBindlessSet = 1;
		VkDescriptorPool bindlessDescriptorPool_;
		VkDescriptorSetLayout bindlessDescriptorLayout_;
		VkDescriptorSet bindlessDescriptorSet_;
		std::vector<TextureHandle> textureToUpdateBindless_;

		// Synchronization 
        // Acquire Semaphore
		// @Note if this is going to be greater than 1, this must be handle in
		// the timeline semaphore
		static const uint32_t kMaxFrame = 1;
		VkSemaphore mImageAcquireSemaphore = VK_NULL_HANDLE;

		// In flight fences
		VkSemaphore mRenderCompleteSemaphore[kMaxFrame];
		VkFence mInFlightFences[kMaxFrame];

		// Timeline semaphore
		VkSemaphore mRenderTimelineSemaphore = VK_NULL_HANDLE;
		VkSemaphore mComputeTimelineSemaphore = VK_NULL_HANDLE;
		VkFence mComputeFence_ = VK_NULL_HANDLE;


		struct VulkanQueryPool
		{
			VkQueryPool queryPool;
		};
		std::vector<VkQueryPool> queryPools_;

		VmaAllocator vmaAllocator_ = nullptr;

		VkPhysicalDeviceProperties2 properties2_ = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		VkPhysicalDeviceFeatures2 features2_ = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		VkPhysicalDeviceVulkan11Features features11_ = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
		VkPhysicalDeviceVulkan12Features features12_ = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };

		uint32_t queueFamilyIndices_ = VK_QUEUE_FAMILY_IGNORED;

		struct CommandQueue
		{
			VkQueue queue = VK_NULL_HANDLE;
		} queues;

		std::unique_ptr<VulkanSwapchain> swapchain_ = nullptr;
		RenderPassHandle mSwapchainRP;
		std::shared_ptr<VulkanCommandList> commandList_;

		void findAvailableInstanceLayer(const std::vector<VkLayerProperties>& availableLayers, std::vector<const char*>& outLayers);
		void findAvailableInstanceExtensions(const std::vector<VkExtensionProperties>& availableExtensions, std::vector<const char*>& outExtensions);
		bool isSwapchainResized();
		VkPhysicalDevice findSuitablePhysicalDevice(const std::vector<const char*>& requiredDeviceExtensions);
		bool createSwapchainInternal();
		VkShaderModule createShader(VkDevice device, const char* data, uint32_t sizeInByte, VkShaderStageFlagBits shaderStage);
		void checkExtensionSupportForPhysicalDevice(VkPhysicalDevice physicalDevice,
			const std::vector<const char*>& requiredDeviceExtensions,
			std::vector<const char*>& availableExtensions);

		void destroyReleasedResources();

		VkRenderPass createRenderPass(const RenderPassDesc* desc);

		VkPipeline createGraphicsPipeline(VulkanShader& vertexShader,
			VulkanShader& fragmentShader,
			VkPipelineLayout pipelineLayout,
			VkPipelineCache pipelineCache,
			VkRenderPass renderPass);

		VkPipelineLayout createPipelineLayout(VkDescriptorSetLayout* setLayout, uint32_t setLayoutCount, const std::vector<VkPushConstantRange>& ranges);


		VkRenderPass createDefaultRenderPass(VkFormat colorFormat);
		inline VulkanCommandList* GetCommandList(CommandList* commandList) { return (VulkanCommandList*)commandList->internalState; }

		void AdvanceFrameCounter();

		// Resource Pools
		ResourcePool<VulkanRenderPass> renderPasses;
		ResourcePool<VulkanPipeline> pipelines;
		ResourcePool<VulkanBuffer> buffers;
		ResourcePool<VulkanTexture> textures;
		ResourcePool<VulkanFramebuffer> framebuffers;
	};
};