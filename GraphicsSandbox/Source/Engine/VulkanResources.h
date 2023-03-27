#pragma once

#include "Graphics.h"

#define VK_NO_PROTOTYPES
#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>

#include <vector>
#include <memory>

// @TODO Remove dependency on upper layer

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

struct VulkanSwapchain
{
	gfx::SwapchainDesc desc;
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

struct VulkanRenderPass
{
	VkRenderPass renderPass;
	gfx::RenderPassDesc desc;
};

struct VulkanPipeline
{
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	std::vector<VkShaderModule> shaderModules;
	VkPipelineBindPoint bindPoint;
	VkDescriptorSetLayout setLayout;
	VkDescriptorUpdateTemplate updateTemplate;
	// Only stores the reference of descriptor set created from 
	// the pools. It is not created as per pipeline basis but dynamically
	// allocated every frame
	VkDescriptorSet descriptorSet;
};

struct VulkanBuffer
{
	// @TODO Remove BufferDesc from here
	VmaAllocation allocation = nullptr;
	VkBuffer buffer = VK_NULL_HANDLE;
	gfx::GPUBufferDesc desc;
	void* mappedDataPtr = nullptr;

	~VulkanBuffer()
	{
	}
};

struct VulkanTexture
{
	VmaAllocation allocation = nullptr;
	VkImage image = VK_NULL_HANDLE;
	std::vector<VkImageView> imageViews{};
	VkSampler sampler = VK_NULL_HANDLE;

	VkFormat format = VK_FORMAT_UNDEFINED;
	VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageAspectFlagBits imageAspect = VK_IMAGE_ASPECT_NONE;

	uint32_t width = 512;
	uint32_t height = 512;
	uint32_t depth = 1;
	uint32_t mipLevels = 1;
	uint32_t arrayLayers = 1;
	uint32_t sizePerPixelByte = 3;
};


struct VulkanFramebuffer
{
	VkFramebuffer framebuffer;

	uint32_t attachmentCount;
	uint32_t depthAttachmentIndex;
	gfx::TextureHandle attachments[16];
	uint32_t width;
	uint32_t height;
};

struct VulkanSemaphore
{
	VkSemaphore semaphore;

	~VulkanSemaphore()
	{
		/*
		gAllocationHandler.destroyedSemaphore_.push_back(semaphore);
		*/
	}
};