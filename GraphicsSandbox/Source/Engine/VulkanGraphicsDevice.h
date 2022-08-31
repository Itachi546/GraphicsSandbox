#pragma once

#include "CommonInclude.h"
#include "Platform.h"
#include "GraphicsDevice.h"
#include "Utils.h"
#include "EventDispatcher.h"

#include<vector>
#include <assert.h>

#ifdef PLATFORM_WINDOW
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#define VK_NO_PROTOTYPES
#include<Volk/volk.h>

#include <vma/vk_mem_alloc.h>

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

		bool CreateSwapchain(const SwapchainDesc* swapchainDesc, Platform::WindowType window)  override;
		void CreateRenderPass(const RenderPassDesc* desc, RenderPass* out)                     override;
		void CreateGraphicsPipeline(const PipelineDesc* desc, Pipeline* out)                   override;
		void CreateComputePipeline(const PipelineDesc* desc, Pipeline* out)                    override;
		void CreateBuffer(const GPUBufferDesc* desc, GPUBuffer* out)                           override;
		void CreateTexture(const GPUTextureDesc* desc, GPUTexture* out)                        override;
		void CreateSemaphore(Semaphore* out)                                                   override;
		void CreateFramebuffer(RenderPass* renderPass, Framebuffer* out)                       override;          
		void CreateQueryPool(QueryPool* out, uint32_t count, QueryType type)                   override;
		void ResetQueryPool(QueryPool* pool, uint32_t first, uint32_t count)                   override;

		void Query(CommandList* commandList, QueryPool* pool, uint32_t index)                  override;
		void ResolveQuery(QueryPool* pool, uint32_t index, uint32_t count, uint64_t* result) override;

		double GetTimestampFrequency() override { return properties2_.properties.limits.timestampPeriod; }

		void CopyToSwapchain(CommandList* commandList, GPUTexture* texture, uint32_t arrayLevel = 0, uint32_t mipLevel = 0) override;
		void CopyBuffer(GPUBuffer* dst, GPUBuffer* src, uint32_t dstOffset = 0)                override;
		void CopyTexture(GPUTexture* dst, GPUBuffer* src, PipelineBarrierInfo* barrier, uint32_t arrayLevel = 0, uint32_t mipLevel = 0);
		void PipelineBarrier(CommandList* commandList, PipelineBarrierInfo* barriers)          override;

		CommandList BeginCommandList()                                                         override;

		void PrepareSwapchain(CommandList* commandList, Semaphore* acquireSemaphore)             override;
		void BeginRenderPass(CommandList* commandList, RenderPass* renderPass, Framebuffer* fb)  override;
		void EndRenderPass(CommandList* commandList)                                             override;
		void SubmitCommandList(CommandList* commandList, Semaphore* signalSemaphore)             override;
		void Present(Semaphore* waitSemaphore)                                                   override;
		void WaitForGPU()                                                                        override;
		void PrepareSwapchainForPresent(CommandList* commandList)                                override;
		
		void BindPipeline(CommandList* commandList, Pipeline* pipeline)                        override;
		void BindIndexBuffer(CommandList* commandList, GPUBuffer* buffer)                      override;

		void BeginDebugMarker(CommandList* commandList, const char* name, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f) override;
		void EndDebugMarker(CommandList* commandList) override;
		// Dynamic field allows to create new descriptor set 
		// However it shouldn't be used in loop because there  
		// is currently no way to free allocated descriptor set
		// except by destroying descriptor pool.
		void UpdateDescriptor(Pipeline* pipeline, DescriptorInfo* descriptorInfo, uint32_t descriptorInfoCount)    override;
		void PushConstants(CommandList* commandList, Pipeline* pipeline, ShaderStage shaderStages, void* value, uint32_t size, uint32_t offset = 0) override;
		void DrawTriangle(CommandList* commandList, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount)         override;
		void DrawTriangleIndexed(CommandList* commandList, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex)  override;
		void DrawIndexedIndirect(CommandList* commandList, GPUBuffer* indirectBuffer, uint32_t offset, uint32_t drawCount, uint32_t stride) override;
		void DispatchCompute(CommandList* commandList, uint32_t groupCountX, uint32_t groupCountY, uint32_t workGroupZ)         override;

		bool IsSwapchainReady(RenderPass* rp) override;

		VkInstance GetInstance() { return instance_; }
		VkDevice GetDevice() { return device_; }
		VkPhysicalDevice GetPhysicalDevice() { return physicalDevice_; }
		VkQueue GetQueue() { return queue_; }
		uint32_t GetSwapchainImageCount() { return swapchain_->imageCount; }

		VkRenderPass Get(RenderPass* rp);
		VkCommandBuffer Get(CommandList* commandList);
		VkDescriptorPool GetDescriptorPool() { return descriptorPools_[swapchain_->imageCount]; }

		~VulkanGraphicsDevice();
	private:
		VkInstance instance_                = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice_    = VK_NULL_HANDLE;
		VkDevice device_                    = VK_NULL_HANDLE;
		VkQueue queue_                      = VK_NULL_HANDLE;
		VkSurfaceKHR surface_               = VK_NULL_HANDLE;

		VkCommandPool   commandPool_        = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer_      = VK_NULL_HANDLE;
		VkCommandPool   stagingCmdPool_     = VK_NULL_HANDLE;
		VkCommandBuffer stagingCmdBuffer_   = VK_NULL_HANDLE;
		
		std::vector<VkDescriptorPool> descriptorPools_;

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

		struct VulkanShader
		{
			VkShaderModule module;
			VkShaderStageFlagBits stage;
		};

		struct VulkanCommandList
		{
			VkCommandBuffer commandBuffer;
			VkCommandPool commandPool;

			std::vector<VkSemaphore> waitSemaphore;
			std::vector<VkSemaphore> signalSemaphore;

			std::vector<VkPipelineStageFlags> waitStages;
		};
		std::shared_ptr<VulkanCommandList> commandList_;		


		struct VulkanSwapchain
		{
			SwapchainDesc desc;
			VkSwapchainKHR swapchain;

			std::vector<VkImage> images;
			std::vector<VkImageView> imageViews;

			std::vector<std::pair<VkImage, VmaAllocation>> depthImages;
			std::vector<VkImageView> depthImageViews;

			std::vector<VkFramebuffer> framebuffers;
			VkSurfaceKHR surface;
			VkSurfaceFormatKHR format;
			uint32_t imageCount;
			uint32_t currentImageIndex;
		};
		std::shared_ptr<VulkanSwapchain> swapchain_ = nullptr;

		void findAvailableInstanceLayer(const std::vector<VkLayerProperties>& availableLayers, std::vector<const char*>& outLayers);
		void findAvailableInstanceExtensions(const std::vector<VkExtensionProperties>& availableExtensions, std::vector<const char*>& outExtensions);
		bool isSwapchainResized();
		VkPhysicalDevice findSuitablePhysicalDevice(const std::vector<const char*>& requiredDeviceExtensions);
		bool createSwapchainInternal(VkRenderPass renderPass);
		VkShaderModule createShader(VkDevice device, const char* data, uint32_t sizeInByte, VkShaderStageFlagBits shaderStage);
		void destroyReleasedResources();

		VkRenderPass createRenderPass(const RenderPassDesc* desc);

		VkPipeline createGraphicsPipeline(VulkanShader& vertexShader,
			VulkanShader& fragmentShader,
			VkPipelineLayout pipelineLayout,
			VkPipelineCache pipelineCache,
			VkRenderPass renderPass);

		VkPipelineLayout createPipelineLayout(VkDescriptorSetLayout setLayout, const std::vector<VkPushConstantRange>& ranges);


		VkRenderPass createDefaultRenderPass(VkFormat colorFormat);
		inline VulkanCommandList* GetCommandList(CommandList* commandList) { return (VulkanCommandList*)commandList->internalState; }


	};
};