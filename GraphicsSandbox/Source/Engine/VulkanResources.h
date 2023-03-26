#pragma once

#include "Graphics.h"

#define VK_NO_PROTOTYPES
#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>

#include <vector>
#include <memory>

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
	VmaAllocation allocation = nullptr;
	VkBuffer buffer = VK_NULL_HANDLE;
	gfx::GPUBufferDesc desc;
	void* mappedDataPtr = nullptr;

	~VulkanBuffer()
	{
		/*
		gAllocationHandler.destroyedBuffers_.push_back(std::make_pair(buffer, allocation));
		*/
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
	~VulkanTexture()
	{
		/*
		gAllocationHandler.destroyedImages_.push_back(std::make_pair(image, allocation));
		gAllocationHandler.destroyedImageViews_.insert(gAllocationHandler.destroyedImageViews_.end(), imageViews.begin(), imageViews.end());
		gAllocationHandler.destroyedSamplers_.push_back(sampler);
		*/
	}
};


struct VulkanFramebuffer
{
	VkFramebuffer framebuffer;

	~VulkanFramebuffer()
	{
		/*
		gAllocationHandler.destroyedFramebuffers_.push_back(framebuffer);
		*/
	}
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