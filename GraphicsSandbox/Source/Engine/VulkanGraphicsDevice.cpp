#define VMA_IMPLEMENTATION 
#include "VulkanGraphicsDevice.h"
#include "Logger.h"
#include "Timer.h"

#define SPIRV_REFLECT_USE_SYSTEM_SPIRV_H
#include "spirv_reflect.h"

#include <unordered_set>

namespace gfx {

#define VK_LOAD_FUNCTION(instance, pFuncName) (vkGetInstanceProcAddr(instance, pFuncName))

    struct ShaderReflection
    {
        VkShaderStageFlagBits shaderStage;
        std::vector<VkVertexInputAttributeDescription> inputDesc;
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBinding;
        std::vector<VkPushConstantRange> pushConstantRanges;
    };

    struct VulkanDescriptorInfo
    {
        union {
            VkDescriptorBufferInfo bufferInfo;
            VkDescriptorImageInfo imageInfo;
        };
    };

    struct ResourceAllocationHandler
    {
        std::vector<VkImageView> destroyedImageViews_;
        std::vector<std::pair<VkImage, VmaAllocation>> destroyedImages_;
        std::vector<VkFramebuffer> destroyedFramebuffers_;
        std::vector<VkRenderPass> destroyedRenderPass_;
        std::vector<VkPipeline> destroyedPipeline_;
        std::vector<VkPipelineLayout> destroyedPipelineLayout_;
        std::vector<VkShaderModule> destroyedShaders_;
        std::vector<std::pair<VkBuffer, VmaAllocation>> destroyedBuffers_;
        std::vector<VkDescriptorSetLayout> destroyedSetLayout_;
        std::vector<VkDescriptorUpdateTemplate> destroyedDescriptorUpdateTemplate_;
        std::vector<std::pair<VkDescriptorSet, VkDescriptorPool>> destroyedDescriptorSet_;
 
       void destroyReleasedResource(VkDevice device, VmaAllocator vmaAllocator)
        {
            for (auto& setLayout : destroyedSetLayout_)
                vkDestroyDescriptorSetLayout(device, setLayout, nullptr);

            for (auto& imageView : destroyedImageViews_)
                vkDestroyImageView(device, imageView, nullptr);

            for (auto& image : destroyedImages_)
            {
                if (image.second == nullptr)
                    vkDestroyImage(device, image.first, nullptr);
                else
                    vmaDestroyImage(vmaAllocator, image.first, image.second);
            }

            for (auto& fb : destroyedFramebuffers_)
                vkDestroyFramebuffer(device, fb, nullptr);

            for (auto& rp : destroyedRenderPass_)
                vkDestroyRenderPass(device, rp, nullptr);

            for (auto& pipeline : destroyedPipeline_)
                vkDestroyPipeline(device, pipeline, nullptr);

            for (auto& pl : destroyedPipelineLayout_)
                vkDestroyPipelineLayout(device, pl, nullptr);

            for (auto& shader : destroyedShaders_)
                vkDestroyShaderModule(device, shader, nullptr);

            for (auto& buffer : destroyedBuffers_)
                vmaDestroyBuffer(vmaAllocator, buffer.first, buffer.second);

            for (auto& descriptorSet : destroyedDescriptorSet_)
                vkFreeDescriptorSets(device, descriptorSet.second, 1, &descriptorSet.first);

            for (auto& updateTemplate : destroyedDescriptorUpdateTemplate_)
                vkDestroyDescriptorUpdateTemplate(device, updateTemplate, nullptr);

            destroyedDescriptorSet_.clear();
            destroyedDescriptorUpdateTemplate_.clear();
            destroyedSetLayout_.clear();
            destroyedImageViews_.clear();
            destroyedImages_.clear();
            destroyedFramebuffers_.clear();
            destroyedRenderPass_.clear();
            destroyedPipeline_.clear();
            destroyedPipelineLayout_.clear();
            destroyedShaders_.clear();
            destroyedBuffers_.clear();
        }

    } gAllocationHandler;
  
    struct VulkanRenderPass
    {
        VkRenderPass renderPass;

        ~VulkanRenderPass()
        {
            gAllocationHandler.destroyedRenderPass_.push_back(renderPass);
        }
    };

    struct VulkanPipeline
    {
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        std::vector<VkShaderModule> shaderModules;
        VkPipelineBindPoint bindPoint;
        VkDescriptorSetLayout setLayout;
        VkDescriptorUpdateTemplate updateTemplate;
        VkDescriptorSet descriptorSet;
        ~VulkanPipeline()
        {
            gAllocationHandler.destroyedPipeline_.push_back(pipeline);
            gAllocationHandler.destroyedPipelineLayout_.push_back(pipelineLayout);
            gAllocationHandler.destroyedShaders_.insert(gAllocationHandler.destroyedShaders_.end(), shaderModules.begin(), shaderModules.end());
            gAllocationHandler.destroyedSetLayout_.push_back(setLayout);
            gAllocationHandler.destroyedDescriptorUpdateTemplate_.push_back(updateTemplate);
            shaderModules.clear();
        }
    };

    struct VulkanBuffer
    {
        VmaAllocation allocation = nullptr;
        VkBuffer buffer = VK_NULL_HANDLE;

        ~VulkanBuffer()
        {
            gAllocationHandler.destroyedBuffers_.push_back(std::make_pair(buffer, allocation));
        }
    };

    /***********************************************************************************************/

    VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            Logger::Error(pCallbackData->pMessage);
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            Logger::Warn(pCallbackData->pMessage);
        else
            Logger::Debug(pCallbackData->pMessage);
        return VK_FALSE;
    }

/***********************************************************************************************/

    VkBlendFactor _ConvertBlendFactor(BlendFactor blendFactor)
    {
        switch (blendFactor)
        {
		case BlendFactor::OneMinusSrcAlpha:
			return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::SrcAlpha:
            return VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::SrcColor:
            return VK_BLEND_FACTOR_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case BlendFactor::SrcAlphaOne:
            return VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::SrcColorOne:
            return VK_BLEND_FACTOR_SRC_COLOR;
        default:
            return VK_BLEND_FACTOR_MAX_ENUM;
        }
            
	}

    VkCompareOp _ConvertCompareOp(CompareOp op)
    {
        switch (op)
        {
        case CompareOp::Less:
            return VK_COMPARE_OP_LESS;
        case CompareOp::Greater:
            return VK_COMPARE_OP_GREATER;
        case CompareOp::Equal:
            return VK_COMPARE_OP_EQUAL;
        case CompareOp::LessOrEqual:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareOp::GreaterOrEqual:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CompareOp::Always:
            return VK_COMPARE_OP_ALWAYS;
        default:
            return VK_COMPARE_OP_MAX_ENUM;
        }
    }

    VkCullModeFlagBits _ConvertCullMode(CullMode cullMode)
    {
        switch (cullMode)
        {
        case CullMode::Back:
            return VK_CULL_MODE_BACK_BIT;
        case CullMode::Front:
			return VK_CULL_MODE_FRONT_BIT;
        case CullMode::None:
        default:
			return VK_CULL_MODE_NONE;
        }
    }

    VkPolygonMode _ConvertPolygonMode(PolygonMode polygonMode)
    {
        switch (polygonMode)
        {
        case PolygonMode::Fill:
            return VK_POLYGON_MODE_FILL;
        case PolygonMode::Line:
            return VK_POLYGON_MODE_LINE;
        default:
            return VK_POLYGON_MODE_FILL;
        }
    }

    VkPrimitiveTopology _ConvertTopology(Topology topology)
    {
        switch (topology)
        {
        case Topology::TriangleList:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case Topology::TriangleStrip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case Topology::Line:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        default:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        };
    }


    VkFormat _ConvertFormat(Format format)
    {
        switch (format)
        {
        case Format::R10G10B10A2_UNORM:
			return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case Format::R8G8B8A8_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::B8G8R8A8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::D32_SFLOAT:
            return VK_FORMAT_D32_SFLOAT;
        case Format::D32_SFLOAT_S8_UINT:
            return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case Format::D24_UNORM_S8_UINT:
            return VK_FORMAT_D24_UNORM_S8_UINT;
        default:
            assert(!"Undefined Format");
            return VK_FORMAT_R8G8B8A8_UNORM;
        }
    }

    VkImageLayout _ConvertLayout(ImageLayout layout)
    {
        switch (layout)
        {
        case ImageLayout::ColorAttachmentOptimal:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthStencilAttachmentOptimal:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthAttachmentOptimal:
            return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        default:
            assert(!"Undefined Image Layout");
            return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }

    VkShaderStageFlagBits _ConvertShaderStage(ShaderStage shaderStage)
    {
        switch (shaderStage)
        {
        case ShaderStage::Vertex:
			return VK_SHADER_STAGE_VERTEX_BIT;
		case ShaderStage::Fragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Geometry:
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::Compute:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        default:
			return VK_SHADER_STAGE_ALL;
        }
    }
    /***********************************************************************************************/

    uint32_t FindGraphicsQueueIndex(VkPhysicalDevice physicalDevice)
    {
        // Find suitable graphics queue
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

        uint32_t familyIndex = 0;
        for (const auto& queueFamily : queueFamilyProperties)
        {
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                return familyIndex;
                break;
            }
            familyIndex++;
        }

        return 0;
    }

    VkBool32 CheckPhysicalDevicePresentationSupport(VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t queueFamily)
    {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
        static PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR vkCheckPresentationSupport = (PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR)VK_LOAD_FUNCTION(instance,
            "vkGetPhysicalDeviceWin32PresentationSupportKHR");
        return vkCheckPresentationSupport(physicalDevice, queueFamily);
#endif
        return VK_TRUE;
    }

    VkSurfaceKHR CreateSurface(VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndices, Platform::WindowType window)
    {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
        // Create Surface
        VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
        win32SurfaceCreateInfo.hinstance = GetModuleHandle(0);
#if defined GLFW_WINDOW
        win32SurfaceCreateInfo.hwnd = glfwGetWin32Window(window);
#else
        Utils::ShowMessageBox("Unsupported WindowType", "Error", MessageType::Error);
        exit(EXIT_ERROR);
#endif
        PFN_vkCreateWin32SurfaceKHR createWin32Surface = (PFN_vkCreateWin32SurfaceKHR)VK_LOAD_FUNCTION(instance, "vkCreateWin32SurfaceKHR");
        VK_CHECK(createWin32Surface(instance, &win32SurfaceCreateInfo, nullptr, &surface));
#endif

        VkBool32 presentSupport = false;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndices, surface, &presentSupport));

        if (!presentSupport)
        {
            Utils::ShowMessageBox("Presentation Not Supported", "Error", MessageType::Error);
            exit(EXIT_ERROR);
        }
        return surface;
    }

    VkSemaphore CreateVkSemaphore(VkDevice device)
    {
        VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

        VkSemaphore semaphore = 0;
        VK_CHECK(vkCreateSemaphore(device, &createInfo, nullptr, &semaphore));
        return semaphore;
    }

	std::pair<VkImage, VmaAllocation> CreateSwapchainDepthImage(VmaAllocator vmaAllocator, VkFormat depthFormat, uint32_t width, uint32_t height)
    {
        VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        createInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        createInfo.format = depthFormat;
        createInfo.arrayLayers = 1;
        createInfo.mipLevels = 1;
        createInfo.extent.width = width;
        createInfo.extent.height = height;
        createInfo.extent.depth = 1;
        createInfo.imageType = VK_IMAGE_TYPE_2D;
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        VkImage image = 0;
        VmaAllocation allocation = {};
        VK_CHECK(vmaCreateImage(vmaAllocator, &createInfo, &allocCreateInfo, &image, &allocation, nullptr));

        return { image, allocation };
    }

    VkImageView CreateImageView(VkDevice device, VkImage image, VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspect)
    {
        VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        createInfo.image = image;
        createInfo.viewType = viewType;
        createInfo.format = format;
        createInfo.subresourceRange.aspectMask = aspect;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;

        VkImageView imageView = 0;
        VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &imageView));

        return imageView;
    }

    VkFramebuffer CreateFramebuffer(VkDevice device, VkRenderPass renderPass, VkImageView* imageViews, uint32_t imageViewCount, uint32_t width, uint32_t height)
    {
        VkFramebufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        createInfo.renderPass = renderPass;
        createInfo.attachmentCount = imageViewCount;
        createInfo.pAttachments = imageViews;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.layers = 1;

        VkFramebuffer framebuffer = 0;
        VK_CHECK(vkCreateFramebuffer(device, &createInfo, nullptr, &framebuffer));
        return framebuffer;
    }

    VkCommandBuffer CreateCommandBuffer(VkDevice device, VkCommandPool commandPool)
    {
        VkCommandBufferAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocateInfo.commandBufferCount = 1;
        allocateInfo.commandPool = commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VkCommandBuffer commandBuffer = 0;
        VK_CHECK(vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer));
        return commandBuffer;
    }

    VkCommandPool CreateCommandPool(VkDevice device, uint32_t queueFamilyIndex)
    {
        VkCommandPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        createInfo.queueFamilyIndex = queueFamilyIndex;
        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

        VkCommandPool commandPool = 0;
        VK_CHECK(vkCreateCommandPool(device, &createInfo, nullptr, &commandPool));
        return commandPool;
    }

    VkImageMemoryBarrier CreateImageBarrier(VkImage image, VkImageAspectFlagBits aspect, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkImageMemoryBarrier result = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        result.srcAccessMask = srcAccessMask;
        result.dstAccessMask = dstAccessMask;
        result.image = image;
        result.oldLayout = oldLayout;
        result.newLayout = newLayout;
        result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        result.subresourceRange.aspectMask = aspect;
        result.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        result.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        return result;
    }

    VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& setBindings)
    {
        VkDescriptorSetLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        createInfo.flags = 0;
        createInfo.bindingCount = uint32_t(setBindings.size());
        createInfo.pBindings = setBindings.data();

        VkDescriptorSetLayout descriptorSetLayout = 0;
		VK_CHECK(vkCreateDescriptorSetLayout(device, &createInfo, 0, &descriptorSetLayout));
        return descriptorSetLayout;
    }

    VkDescriptorPool CreateDescriptorPool(VkDevice device)
    {
        VkDescriptorPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        uint32_t descriptorCount = 128;
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, descriptorCount },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, descriptorCount },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, descriptorCount },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptorCount },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorCount },
        };
        createInfo.maxSets = descriptorCount;
        createInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
		createInfo.pPoolSizes = poolSizes;

        VkDescriptorPool descriptorPool = 0;
		VK_CHECK(vkCreateDescriptorPool(device, &createInfo, nullptr, &descriptorPool));
        return descriptorPool;
    }

    VkDescriptorUpdateTemplate CreateUpdateTemplate(VkDevice device, VkPipelineBindPoint bindPoint, VkPipelineLayout layout, VkDescriptorSetLayout setLayout, const std::vector<ShaderReflection>& shaders)
    {
        std::vector<VkDescriptorUpdateTemplateEntry> entries;

        uint32_t i = 0;
        for (auto& shader : shaders)
        {
            for (auto& descriptorSet : shader.descriptorSetLayoutBinding)
            {
                VkDescriptorUpdateTemplateEntry entry = {};
                entry.dstBinding = descriptorSet.binding;
                entry.dstArrayElement = 0;
                entry.descriptorCount = 1;
                entry.descriptorType = descriptorSet.descriptorType;
                entry.offset = sizeof(VulkanDescriptorInfo) * i;
                entry.stride = sizeof(VulkanDescriptorInfo);
                entries.push_back(entry);
                i++;
			}
        }

        VkDescriptorUpdateTemplateCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO };
        createInfo.descriptorUpdateEntryCount = uint32_t(entries.size());
        createInfo.pDescriptorUpdateEntries = entries.data();

        createInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
        createInfo.descriptorSetLayout = setLayout;
        createInfo.pipelineBindPoint = bindPoint;
        createInfo.pipelineLayout = layout;

        VkDescriptorUpdateTemplate updateTemplate = 0;
		VK_CHECK(vkCreateDescriptorUpdateTemplate(device, &createInfo, nullptr, &updateTemplate));
        return updateTemplate;
	}

    /***********************************************************************************************/

    VkShaderStageFlagBits GetShaderStage(SpvReflectShaderStageFlagBits shaderStage)
    {
        switch (shaderStage)
        {
        case SpvReflectShaderStageFlagBits::SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case SpvReflectShaderStageFlagBits::SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case SpvReflectShaderStageFlagBits::SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        case SpvReflectShaderStageFlagBits::SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        default:
            assert(!"Undefined shader stage");
            return VkShaderStageFlagBits(0);
        }
    }

    void ParseShaderReflection(const void* code, uint32_t sizeInByte, ShaderReflection* out)
    {
        SpvReflectShaderModule module;
        SpvReflectResult result = spvReflectCreateShaderModule(sizeInByte, code, &module);

        assert(result == SPV_REFLECT_RESULT_SUCCESS);
        out->shaderStage = GetShaderStage(module.shader_stage);

        uint32_t inputCount = 0;
        result = spvReflectEnumerateInputVariables(&module, &inputCount, nullptr);
        std::vector<SpvReflectInterfaceVariable*> inputAttributes(inputCount);
        result = spvReflectEnumerateInputVariables(&module, &inputCount, inputAttributes.data());

        // Parse Input Variables
        for (uint32_t i = 0; i < inputCount && out->shaderStage != VK_SHADER_STAGE_FRAGMENT_BIT; ++i)
        {
            auto& input = inputAttributes[i];
            if (input->location > 32)
                continue;

            VkVertexInputAttributeDescription attributeDesc = {};
            attributeDesc.binding = 0;
            attributeDesc.location = input->location;
            attributeDesc.format = static_cast<VkFormat>(input->format);
            attributeDesc.offset = 0;
            out->inputDesc.push_back(attributeDesc);
        }

        // DescriptorSet Layouts
        uint32_t descriptorSetCounts = 0;
        result = spvReflectEnumerateDescriptorSets(&module, &descriptorSetCounts, nullptr);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);
        std::vector<SpvReflectDescriptorSet*> reflDescriptorSets(descriptorSetCounts);
        result = spvReflectEnumerateDescriptorSets(&module, &descriptorSetCounts, reflDescriptorSets.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        for (const auto& reflDescSet : reflDescriptorSets)
        {
            for (uint32_t i = 0; i < reflDescSet->binding_count; ++i)
            {
                const auto& reflBinding = reflDescSet->bindings[i];
                VkDescriptorSetLayoutBinding layoutBinding = {};
                layoutBinding.binding = reflBinding->binding;
                layoutBinding.descriptorType = static_cast<VkDescriptorType>(reflBinding->descriptor_type);
                layoutBinding.descriptorCount = 1;
                layoutBinding.stageFlags = out->shaderStage;

                out->descriptorSetLayoutBinding.push_back(layoutBinding);
            }
        }

        // Push Constants
        uint32_t pushConstantCount = 0;
        result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, nullptr);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);
		std::vector<SpvReflectBlockVariable*> pushConstants(pushConstantCount);
        result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, pushConstants.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        for (auto& pushConstant : pushConstants)
        {
            VkPushConstantRange pushConstantRange = {};
            pushConstantRange.size = pushConstant->size;
            pushConstantRange.offset = pushConstant->offset;
            pushConstantRange.stageFlags = out->shaderStage;
            out->pushConstantRanges.push_back(pushConstantRange);
        }
        spvReflectDestroyShaderModule(&module);
    }


    /***********************************************************************************************/  

    bool VulkanGraphicsDevice::createSwapchainInternal(VkRenderPass renderPass)
    {
        VkSurfaceKHR surface = swapchain_->surface;
        SwapchainDesc& desc = swapchain_->desc;

        VkSurfaceCapabilitiesKHR surfaceCaps = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface, &surfaceCaps));

        if (surfaceCaps.currentExtent.width == 0 || surfaceCaps.currentExtent.height == 0)
            return false;

        desc.width = surfaceCaps.currentExtent.width;
        desc.height = surfaceCaps.currentExtent.height;

        VkCompositeAlphaFlagBitsKHR surfaceComposite =
            (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
            ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
            : (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
            ? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
            : (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
            ? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
            : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;


		uint32_t formatCount = 0;
		VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface, &formatCount, nullptr));

		std::vector<VkSurfaceFormatKHR> formats(formatCount);
		VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface, &formatCount, formats.data()));

        uint32_t presentModeCount;
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface, &presentModeCount, nullptr));

        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface, &presentModeCount, presentModes.data()));

        VkSurfaceFormatKHR surfaceFormat = {};
        surfaceFormat.format = _ConvertFormat(desc.colorFormat);
        surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        VkFormat requiredFormat = _ConvertFormat(desc.colorFormat);
        for(auto& supportedFormat : formats)
        {
            if (supportedFormat.format == requiredFormat && surfaceFormat.colorSpace == supportedFormat.colorSpace)
            {
                surfaceFormat = supportedFormat;
                break;
            }
        }
        swapchain_->format = surfaceFormat;

        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        if (!desc.vsync)
        {
            for (auto& supportedPresentMode : presentModes)
            {
                if (supportedPresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    presentMode = supportedPresentMode;
                    break;
                }
                else if (supportedPresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
                {
                    presentMode = supportedPresentMode;
                    break;
                }
            }
        }
        else {
            presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }

        VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        createInfo.surface = surface;
        createInfo.minImageCount = std::max(2u, surfaceCaps.minImageCount);
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent.width = desc.width;
        createInfo.imageExtent.height = desc.height;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.queueFamilyIndexCount = 1;
        createInfo.pQueueFamilyIndices = &queueFamilyIndices_;
        createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        createInfo.compositeAlpha = surfaceComposite;
        createInfo.presentMode = presentMode;
        createInfo.oldSwapchain = swapchain_->swapchain;

  		VK_CHECK(vkCreateSwapchainKHR(device_, &createInfo, 0, &swapchain_->swapchain));

        if (swapchain_->imageCount > 0)
        {
            gAllocationHandler.destroyedImageViews_.insert(gAllocationHandler.destroyedImageViews_.end(), swapchain_->imageViews.begin(), swapchain_->imageViews.end());
            swapchain_->images.clear();
            swapchain_->imageViews.clear();

            gAllocationHandler.destroyedFramebuffers_.insert(gAllocationHandler.destroyedFramebuffers_.end(), swapchain_->framebuffers.begin(), swapchain_->framebuffers.end());
            swapchain_->framebuffers.clear();

            if (swapchain_->depthImages.size() > 0)
            {
                gAllocationHandler.destroyedImages_.insert(gAllocationHandler.destroyedImages_.end(), swapchain_->depthImages.begin(), swapchain_->depthImages.end());
                gAllocationHandler.destroyedImageViews_.insert(gAllocationHandler.destroyedImageViews_.end(), swapchain_->depthImageViews.begin(), swapchain_->depthImageViews.end());

                swapchain_->depthImages.clear();
                swapchain_->depthImageViews.clear();

            }
        }

        uint32_t imageCount = 0;
        VK_CHECK(vkGetSwapchainImagesKHR(device_, swapchain_->swapchain, &imageCount, nullptr));

        swapchain_->imageCount = imageCount;
        swapchain_->images.resize(imageCount);
        VK_CHECK(vkGetSwapchainImagesKHR(device_, swapchain_->swapchain, &imageCount, swapchain_->images.data()));

        swapchain_->imageViews.resize(imageCount);
        swapchain_->framebuffers.resize(imageCount);
        if (desc.enableDepth)
        {
            swapchain_->depthImages.resize(imageCount);
            swapchain_->depthImageViews.resize(imageCount);
        }

        uint32_t width  = swapchain_->desc.width;
        uint32_t height = swapchain_->desc.height;

        VkFormat depthFormat = _ConvertFormat(desc.depthFormat);
        for (uint32_t i = 0; i < imageCount; ++i)
        {
            if (desc.enableDepth)
            {
                swapchain_->depthImages[i] = CreateSwapchainDepthImage(vmaAllocator_, depthFormat, width, height);
                swapchain_->depthImageViews[i] = CreateImageView(device_, swapchain_->depthImages[i].first, VK_IMAGE_VIEW_TYPE_2D, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
            }
            swapchain_->imageViews[i] = CreateImageView(device_, swapchain_->images[i], VK_IMAGE_VIEW_TYPE_2D, swapchain_->format.format, VK_IMAGE_ASPECT_COLOR_BIT);

            std::vector<VkImageView> imageViews = { swapchain_->imageViews[i] };
            if (desc.enableDepth)
                imageViews.push_back(swapchain_->depthImageViews[i]);
            swapchain_->framebuffers[i] = CreateFramebuffer(device_, renderPass, imageViews.data(), static_cast<uint32_t>(imageViews.size()), width, height);
        }

        return true;
    }

    void VulkanGraphicsDevice::findAvailableInstanceExtensions(const std::vector<VkExtensionProperties>& availableExtensions, std::vector<const char*>& outExtensions)
    {
        const char* requiredExtensions[] = {
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
            VK_KHR_SURFACE_EXTENSION_NAME,
    #if defined VK_USE_PLATFORM_WIN32_KHR
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    #endif
        };

        for (auto required : requiredExtensions)
        {
            for (auto& extension : availableExtensions)
            {
                if (strcmp(extension.extensionName, required) == 0)
                {
                    outExtensions.push_back(required);
                    break;
                }
            }
        }
    }

    void VulkanGraphicsDevice::findAvailableInstanceLayer(const std::vector<VkLayerProperties>& availableLayers, std::vector<const char*>& outLayers)
    {
        std::vector<const char*> requiredLayers;
        if (mValidationMode == ValidationMode::Enabled)
        {
            requiredLayers.push_back("VK_LAYER_KHRONOS_validation");
        }

        for (auto required : requiredLayers)
        {
            for (auto& layer : availableLayers)
            {
                if (strcmp(layer.layerName, required) == 0)
                {
                    outLayers.push_back(required);
                    break;
                }
            }
        }
    }

    VkPhysicalDevice VulkanGraphicsDevice::findSuitablePhysicalDevice(const std::vector<const char*>& requiredDeviceExtensions)
    {
        uint32_t deviceCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr));
        if (deviceCount == 0)
        {
            Utils::ShowMessageBox("Not GPU Found that support Vulkan", "Error", MessageType::Error);
            exit(EXIT_ERROR);
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data()));

        for (const auto& physicalDevice : devices)
        {
            uint32_t extensionCount;
            VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr));
            std::vector<VkExtensionProperties> availableExtension(extensionCount);
            VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtension.data()));

            bool suitable = true;
            for (auto& extension : requiredDeviceExtensions)
            {
                bool found = false;
                for (auto& available : availableExtension)
                {
                    if (std::strcmp(extension, available.extensionName) == 0)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    suitable = false;
                    break;
                }
            }

            if (!suitable) continue;


            vkGetPhysicalDeviceProperties2(physicalDevice, &properties2_);
            if (properties2_.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                return physicalDevice;
            }
        }

        return devices[0];
    }

    VkShaderModule VulkanGraphicsDevice::createShader(VkDevice device, const char* data, uint32_t sizeInByte, VkShaderStageFlagBits shaderStage)
    {
        VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        createInfo.codeSize = sizeInByte;
        createInfo.pCode = reinterpret_cast<const uint32_t*>(data);

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));
        return shaderModule;
    }

    VkPipelineLayout VulkanGraphicsDevice::createPipelineLayout(VkDescriptorSetLayout setLayout, const std::vector<VkPushConstantRange>& ranges)
    {
        VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        createInfo.setLayoutCount = 1;
        createInfo.pSetLayouts = &setLayout;
		createInfo.pushConstantRangeCount = static_cast<uint32_t>(ranges.size());
		createInfo.pPushConstantRanges = ranges.data();

        VkPipelineLayout pipelineLayout = 0;
        VK_CHECK(vkCreatePipelineLayout(device_, &createInfo, nullptr, &pipelineLayout));
        return pipelineLayout;
    }

    VkPipeline VulkanGraphicsDevice::createGraphicsPipeline(VulkanShader& vertexShader, VulkanShader& fragmentShader, VkPipelineLayout pipelineLayout, VkPipelineCache pipelineCache, VkRenderPass renderPass)
    {
        VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = vertexShader.stage;
        stages[0].module = vertexShader.module;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = fragmentShader.stage;
        stages[1].module = fragmentShader.module;
        stages[1].pName = "main";

        createInfo.stageCount = static_cast<uint32_t>(std::size(stages));
        createInfo.pStages = stages;

        VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        createInfo.pVertexInputState = &vertexInput;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        createInfo.pInputAssemblyState = &inputAssembly;

        VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        createInfo.pViewportState = &viewportState;

        VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizationState.lineWidth = 1.f;
        createInfo.pRasterizationState = &rasterizationState;

        VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        createInfo.pMultisampleState = &multisampleState;

        VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        createInfo.pDepthStencilState = &depthStencilState;

        VkPipelineColorBlendAttachmentState colorAttachmentState = {};
        colorAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlendState.attachmentCount = 1;
        colorBlendState.pAttachments = &colorAttachmentState;
        createInfo.pColorBlendState = &colorBlendState;

        VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
        dynamicState.pDynamicStates = dynamicStates;
        createInfo.pDynamicState = &dynamicState;

        createInfo.layout = pipelineLayout;
        createInfo.renderPass = renderPass;

        // TODO pipeline cache 
        VkPipeline pipeline = 0;
        VK_CHECK(vkCreateGraphicsPipelines(device_, 0, 1, &createInfo, 0, &pipeline));
        return pipeline;
    }


    bool VulkanGraphicsDevice::isSwapchainResized()
    {
        VkSurfaceCapabilitiesKHR surfaceCaps = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, swapchain_->surface, &surfaceCaps));

        int width = swapchain_->desc.width;
        int height = swapchain_->desc.height;

        if (surfaceCaps.currentExtent.width != width || surfaceCaps.currentExtent.height != height)
            return true;

        return false;
    }


    VulkanGraphicsDevice::VulkanGraphicsDevice(Platform::WindowType window, ValidationMode validationMode)
    {
        // Begin Timer
        Timer timer;

        // Initialize Volk make sure not to link vulkan-1.lib
        VK_CHECK(volkInitialize());

        VkApplicationInfo applicationInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
        applicationInfo.pApplicationName = "GraphicsSandbox Application";
        applicationInfo.pEngineName = "Graphics Sandbox";
        applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.apiVersion = VK_API_VERSION_1_2;

        uint32_t instanceLayerCount;
        VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));
        std::vector<VkLayerProperties> availableInstanceLayers(instanceLayerCount);
        VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, availableInstanceLayers.data()));

        uint32_t extensionCount;
        VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));
        std::vector<VkExtensionProperties> availableInstanceExtensions(extensionCount);
        VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableInstanceExtensions.data()));

        std::vector<const char*> instanceLayers;
        std::vector<const char*> instanceExtensions;

        // List instance layer and extension, check the presence of required one
        findAvailableInstanceExtensions(availableInstanceExtensions, instanceExtensions);
        findAvailableInstanceLayer(availableInstanceLayers, instanceLayers);

        VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        createInfo.pApplicationInfo = &applicationInfo;
        createInfo.ppEnabledExtensionNames = instanceExtensions.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
        createInfo.ppEnabledLayerNames = instanceLayers.data();
        createInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());

        // Create debug messenger
        VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        if (mValidationMode == ValidationMode::Enabled)
        {
            debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
            debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
            debugUtilsCreateInfo.pfnUserCallback = DebugUtilsMessengerCallback;
        }
        createInfo.pNext = &debugUtilsCreateInfo;

        // Create Instance
        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance_));

        volkLoadInstanceOnly(instance_);

        if (mValidationMode == ValidationMode::Enabled)
        {
            VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance_, &debugUtilsCreateInfo, nullptr, &messenger_));
        }

        // List required device extension
        std::vector<const char*> requiredDeviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        // Create physical device
        physicalDevice_ = findSuitablePhysicalDevice(requiredDeviceExtensions);
        Logger::Debug("Selected Physical Device: " + std::string(properties2_.properties.deviceName));

        queueFamilyIndices_ = FindGraphicsQueueIndex(physicalDevice_);

        // TODO Check presentation support at the time of choosing physical device
        if (!CheckPhysicalDevicePresentationSupport(instance_, physicalDevice_, queueFamilyIndices_))
        {
            Utils::ShowMessageBox("Device Doesn't Support Presentation", "Error", MessageType::Error);
            exit(EXIT_ERROR);
        }

        float queuePriorities = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queueCreateInfo.pQueuePriorities = &queuePriorities;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndices_;

        // Enable required features
        features2_.features.multiDrawIndirect = true;
        features2_.features.pipelineStatisticsQuery = true;
        features2_.features.shaderInt16 = true;

        features11_.shaderDrawParameters = true;
        features12_.drawIndirectCount = true;
        features12_.separateDepthStencilLayouts = true;
        features11_.pNext = &features12_;
        features2_.pNext = &features11_;

        // Create logical device
        VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.pEnabledFeatures = nullptr;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
        deviceCreateInfo.pNext = &features2_;

        VK_CHECK(vkCreateDevice(physicalDevice_, &deviceCreateInfo, nullptr, &device_));
        volkLoadDevice(device_);

        // Get device Queue
        vkGetDeviceQueue(device_, queueFamilyIndices_, 0, &queue_);

        // Create Acquire/Release Semaphore
        acquireSemaphore_ = CreateVkSemaphore(device_);
        releaseSemaphore_ = CreateVkSemaphore(device_);

        commandPool_ = CreateCommandPool(device_, queueFamilyIndices_);
        commandBuffer_ = CreateCommandBuffer(device_, commandPool_);

        stagingCmdPool_   = CreateCommandPool(device_, queueFamilyIndices_);
        stagingCmdBuffer_ = CreateCommandBuffer(device_, stagingCmdPool_);

        commandList_ = std::make_shared<VulkanCommandList>();
        commandList_->commandBuffer = commandBuffer_;
        commandList_->commandPool = commandPool_;

        VmaVulkanFunctions vulkanFunctions = {};
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
        vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
        vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
        vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
        vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
        vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
        vulkanFunctions.vkCreateImage = vkCreateImage;
        vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
        vulkanFunctions.vkDestroyImage = vkDestroyImage;
        vulkanFunctions.vkFreeMemory = vkFreeMemory;
        vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
        vulkanFunctions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;
        vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
        vulkanFunctions.vkMapMemory = vkMapMemory;
        vulkanFunctions.vkUnmapMemory = vkUnmapMemory;

        VmaAllocatorCreateInfo vmaCreateInfo = {};
        vmaCreateInfo.device = device_;
        vmaCreateInfo.instance = instance_;
        vmaCreateInfo.physicalDevice = physicalDevice_;
        vmaCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
        vmaCreateInfo.pVulkanFunctions = &vulkanFunctions;

		VK_CHECK(vmaCreateAllocator(&vmaCreateInfo, &vmaAllocator_));

        descriptorPool_ = CreateDescriptorPool(device_);

        Logger::Debug("Created VulkanGraphicsDevice (" + std::to_string((int)std::round(timer.elapsed())) + "ms)");
    }

    bool VulkanGraphicsDevice::CreateSwapchain(const SwapchainDesc* swapchainDesc, Platform::WindowType window)
    {
        if (swapchain_ == nullptr)
        {
			swapchain_ = std::make_shared<VulkanSwapchain>();
            swapchain_->desc = *swapchainDesc;
			swapchain_->surface = CreateSurface(instance_, physicalDevice_, queueFamilyIndices_, window);
        }

        auto vkRenderPass = std::static_pointer_cast<VulkanRenderPass>(swapchainDesc->renderPass->internalState);
        assert(vkRenderPass != nullptr);
		createSwapchainInternal(vkRenderPass->renderPass);

        return true;
    }

/*
    VkRenderPass VulkanGraphicsDevice::createDefaultRenderPass(VkFormat colorFormat)
    {
        VkAttachmentDescription attachment = {};
        attachment.format = colorFormat;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference attachmentRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        VkRenderPassCreateInfo createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &attachment;

        VkSubpassDescription subpassDescription = {};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &attachmentRef;

        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpassDescription;

        VkRenderPass renderPass = 0;
        VK_CHECK(vkCreateRenderPass(device_, &createInfo, nullptr, &renderPass));
        return renderPass;

    }
    */

    VkRenderPass VulkanGraphicsDevice::createRenderPass(const RenderPassDesc* desc)
    {
        std::vector<VkAttachmentDescription> attachments(desc->attachmentCount);
        std::vector<VkAttachmentReference> attachmentRef(desc->attachmentCount);

        bool hasDepthAttachment = false;
        uint32_t depthAttachmentIndex = 0;
        for (uint32_t i = 0; i < desc->attachmentCount; ++i)
        {
            const Attachment& attachment = desc->attachments[i];
            VkFormat format = _ConvertFormat(attachment.format);
            VkImageLayout layout = _ConvertLayout(attachment.layout);

            if (format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT)
            {
                hasDepthAttachment = true;
                depthAttachmentIndex = i;
            }

            attachments[i].format = format;
            attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
            attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[i].initialLayout = layout;
            attachments[i].finalLayout = layout;
            attachmentRef[i] = { attachment.index, layout};
        }

        VkRenderPassCreateInfo createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        createInfo.attachmentCount = desc->attachmentCount;
        createInfo.pAttachments = attachments.data();

        VkSubpassDescription subpassDescription = {};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        if (hasDepthAttachment)
        {
            VkAttachmentReference depthAttachmentRef = attachmentRef[depthAttachmentIndex];
            subpassDescription.pDepthStencilAttachment = &depthAttachmentRef;
            attachmentRef.erase(attachmentRef.begin() + depthAttachmentIndex);
        }

        subpassDescription.colorAttachmentCount = static_cast<uint32_t>(attachmentRef.size());
        subpassDescription.pColorAttachments = attachmentRef.data();


        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpassDescription;

        VkRenderPass renderPass = 0;
        VK_CHECK(vkCreateRenderPass(device_, &createInfo, nullptr, &renderPass));
        return renderPass;

    }

    void VulkanGraphicsDevice::CreateRenderPass(const RenderPassDesc* desc, RenderPass* out)
    {
        auto vulkanRenderPass = std::make_shared<VulkanRenderPass>();
        vulkanRenderPass->renderPass = createRenderPass(desc);
        out->internalState = vulkanRenderPass;
        out->desc = *desc;
    }

    void VulkanGraphicsDevice::CreateGraphicsPipeline(const PipelineDesc* desc, Pipeline* out)
    {
        auto vkPipeline = std::make_shared<VulkanPipeline>();
        vkPipeline->bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

        std::vector<VkPipelineShaderStageCreateInfo> stages(desc->shaderCount);
        std::vector<ShaderReflection> shaderReflections(desc->shaderCount);

        for (uint32_t i = 0; i < desc->shaderCount; ++i)
        {
            ShaderDescription& shader = desc->shaderDesc[i];
            ParseShaderReflection(shader.code, shader.sizeInByte, &shaderReflections[i]);

            VkShaderStageFlagBits shaderStage = shaderReflections[i].shaderStage;
            VkShaderModule shaderModule = createShader(device_, shader.code, shader.sizeInByte, shaderStage);

            stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[i].stage = shaderStage;
            stages[i].module = shaderModule;
            stages[i].pName = "main";

            vkPipeline->shaderModules.push_back(shaderModule);
        }

        createInfo.stageCount = desc->shaderCount;
        createInfo.pStages = stages.data();

        // TODO Should be created from SPV Reflection
        VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        createInfo.pVertexInputState = &vertexInput;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssembly.topology = _ConvertTopology(desc->topology);
        createInfo.pInputAssemblyState = &inputAssembly;

        VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        createInfo.pViewportState = &viewportState;

        const RasterizationState& rs = desc->rasterizationState;
        VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizationState.lineWidth = rs.lineWidth;
        rasterizationState.cullMode = _ConvertCullMode(rs.cullMode);
        rasterizationState.polygonMode = _ConvertPolygonMode(rs.polygonMode);
        createInfo.pRasterizationState = &rasterizationState;

        VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        createInfo.pMultisampleState = &multisampleState;

        VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        if (rs.enableDepthTest)
        {
            depthStencilState.depthTestEnable = rs.enableDepthTest;
            depthStencilState.depthWriteEnable = rs.enableDepthWrite;
            depthStencilState.depthCompareOp = _ConvertCompareOp(rs.depthTestFunc);
            depthStencilState.maxDepthBounds = 1.0f;
            depthStencilState.minDepthBounds = 0.0f;
        }
        createInfo.pDepthStencilState = &depthStencilState;

        const BlendState& bs = desc->blendState;
        VkPipelineColorBlendAttachmentState colorAttachmentState = {};
        if (bs.enable)
        {
            colorAttachmentState.blendEnable = true;
            colorAttachmentState.srcColorBlendFactor = _ConvertBlendFactor(bs.srcColor);
            colorAttachmentState.dstColorBlendFactor = _ConvertBlendFactor(bs.dstColor);
            colorAttachmentState.srcAlphaBlendFactor = _ConvertBlendFactor(bs.srcAlpha);
            colorAttachmentState.dstAlphaBlendFactor = _ConvertBlendFactor(bs.dstAlpha);
        }
        colorAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlendState.attachmentCount = 1;
        colorBlendState.pAttachments = &colorAttachmentState;
        createInfo.pColorBlendState = &colorBlendState;

        VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
        dynamicState.pDynamicStates = dynamicStates;
        createInfo.pDynamicState = &dynamicState;

        /*
        // Pipeline metadata initialization
        std::vector<SpirvReflection::SPIRVModule> spvModules(desc->shaderCount);
        VkShaderStageFlags pushConstantStages = 0;
        for (uint32_t i = 0; i < desc->shaderCount; ++i)
        {
            if (!SpirvReflection::Parse(reinterpret_cast<const uint32_t*>(desc->shaderDesc[i].code), desc->shaderDesc[i].sizeInByte / 4, &spvModules[i]))
                Logger::Error("Failed to parse spvModule");
            if (spvModules[i].usesPushConstants)
                pushConstantStages |= spvModules[i].shaderStage;

        }
        */
        std::vector<VkPushConstantRange> pushConstantRanges;
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
        for (auto& shader : shaderReflections)
        {
            if (shader.pushConstantRanges.size() > 0)
                pushConstantRanges.insert(pushConstantRanges.end(), shader.pushConstantRanges.begin(), shader.pushConstantRanges.end());

            if (shader.descriptorSetLayoutBinding.size() > 0)
                setLayoutBindings.insert(setLayoutBindings.end(), shader.descriptorSetLayoutBinding.begin(), shader.descriptorSetLayoutBinding.end());

        }

		vkPipeline->setLayout = CreateDescriptorSetLayout(device_, setLayoutBindings);

		VkPipelineLayout pipelineLayout = createPipelineLayout(vkPipeline->setLayout, pushConstantRanges);
        createInfo.layout = pipelineLayout;

		createInfo.renderPass = std::static_pointer_cast<VulkanRenderPass>(desc->renderPass->internalState)->renderPass;

        VkPipeline pipeline = 0;
        VK_CHECK(vkCreateGraphicsPipelines(device_, 0, 1, &createInfo, 0, &pipeline));

        vkPipeline->pipelineLayout = pipelineLayout;
        vkPipeline->pipeline = pipeline;
        vkPipeline->updateTemplate = CreateUpdateTemplate(device_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, vkPipeline->setLayout, shaderReflections);
        out->internalState = vkPipeline;
    }

    void VulkanGraphicsDevice::CreateBuffer(const GPUBufferDesc* desc, GPUBuffer* out)
    {
        auto internalState = std::make_shared<VulkanBuffer>();

        out->internalState = internalState;
        out->type = GPUResource::Type::Buffer;
        out->mappedDataSize = 0;
        out->mappedDataPtr = nullptr;
        out->desc = *desc;

        VkBufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        createInfo.size = desc->size;

        VkBufferUsageFlags usage = 0;
        if (HasFlag(desc->bindFlag, BindFlag::VertexBuffer))
            usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (HasFlag(desc->bindFlag, BindFlag::IndexBuffer))
            usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (HasFlag(desc->bindFlag, BindFlag::ConstantBuffer))
            usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (HasFlag(desc->bindFlag, BindFlag::ShaderResource))
            usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        if (desc->usage == Usage::ReadBack)
            allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        else if (desc->usage == Usage::Upload)
            allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        createInfo.usage = usage;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 1;
        createInfo.pQueueFamilyIndices = &queueFamilyIndices_;

        VK_CHECK(vmaCreateBuffer(vmaAllocator_, &createInfo, &allocCreateInfo, &internalState->buffer, &internalState->allocation, nullptr));
        if (desc->usage == Usage::ReadBack || desc->usage == Usage::Upload)
        {
            out->mappedDataPtr = internalState->allocation->GetMappedData();
            out->mappedDataSize = internalState->allocation->GetSize();
        }
    }

    CommandList VulkanGraphicsDevice::BeginCommandList()
    {
        CommandList commandList = {};
        commandList.internalState = commandList_.get();

        VulkanCommandList* cmdList = GetCommandList(&commandList);
		VK_CHECK(vkResetCommandPool(device_, cmdList->commandPool, 0));

        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandBuffer_, &beginInfo));
        return commandList;
    }

    void VulkanGraphicsDevice::BeginRenderPass(CommandList* commandList, RenderPass* renderPass)
    {
        auto vkRenderpass = std::static_pointer_cast<VulkanRenderPass>(renderPass->internalState);
        VkRenderPass rp = vkRenderpass->renderPass;

        auto vkCmdList = GetCommandList(commandList);

        uint32_t imageIndex = 0;
        VK_CHECK(vkAcquireNextImageKHR(device_, swapchain_->swapchain, ~0ull, acquireSemaphore_, VK_NULL_HANDLE, &imageIndex));
        vkCmdList->waitSemaphore.push_back(acquireSemaphore_);

        swapchain_->currentImageIndex = imageIndex;

        std::vector<VkClearValue> clearValues;
        clearValues.push_back({ 48.f / 255.f, 10.f / 255.f, 36.f / 255.f, 1 });

        std::vector<VkImageMemoryBarrier> renderBeginBarrier = {
            CreateImageBarrier(swapchain_->images[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        };

        if (renderPass->desc.hasDepthAttachment)
        {
            renderBeginBarrier.push_back(CreateImageBarrier(swapchain_->depthImages[imageIndex].first, VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
            clearValues.push_back({ 0.0f, 0 });
        }
        vkCmdPipelineBarrier(vkCmdList->commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, static_cast<uint32_t>(renderBeginBarrier.size()), renderBeginBarrier.data());

        uint32_t width = (uint32_t)swapchain_->desc.width;
        uint32_t height = (uint32_t)swapchain_->desc.height;

        VkRenderPassBeginInfo passBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        passBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        passBeginInfo.pClearValues = clearValues.data();
        passBeginInfo.framebuffer = swapchain_->framebuffers[imageIndex];
        passBeginInfo.renderPass = rp;
        passBeginInfo.renderArea.extent.width = width;
        passBeginInfo.renderArea.extent.height = height;
        vkCmdBeginRenderPass(vkCmdList->commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdList->waitStages.push_back(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

        VkViewport viewport = { 0.0f, (float)height, (float)width, -float(height),0.0f, 1.0f};
        VkRect2D scissor = { {0, 0}, {width, height} };

        vkCmdSetViewport(vkCmdList->commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(vkCmdList->commandBuffer, 0, 1, &scissor);
    }

    void VulkanGraphicsDevice::BindPipeline(CommandList* commandList, Pipeline* pipeline)
    {
        auto cmdList = GetCommandList(commandList);
        auto vkPipeline = std::static_pointer_cast<VulkanPipeline>(pipeline->internalState);
        vkCmdBindPipeline(cmdList->commandBuffer, vkPipeline->bindPoint, vkPipeline->pipeline);

        vkCmdBindDescriptorSets(cmdList->commandBuffer, vkPipeline->bindPoint, vkPipeline->pipelineLayout, 0, 1, &vkPipeline->descriptorSet, 0, 0);
    }

    void VulkanGraphicsDevice::DrawTriangle(CommandList* commandList, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex)
    {
        auto cmd = GetCommandList(commandList);
        vkCmdDraw(cmd->commandBuffer, vertexCount, instanceCount, firstVertex, 0);
    }

    void VulkanGraphicsDevice::DrawTriangleIndexed(CommandList* commandList, uint32_t indexCount, uint32_t instanceCount, uint32_t firstVertex)
    {
        auto cmd = GetCommandList(commandList);
        vkCmdDrawIndexed(cmd->commandBuffer, indexCount, instanceCount, firstVertex, 0, 0);
    }

    bool VulkanGraphicsDevice::IsSwapchainReady(RenderPass* rp)
    {
        auto vkRenderpass = std::static_pointer_cast<VulkanRenderPass>(rp->internalState);

        if (isSwapchainResized())
			return createSwapchainInternal(vkRenderpass->renderPass);
        return true;
    }

    void VulkanGraphicsDevice::BindIndexBuffer(CommandList* commandList, GPUBuffer* buffer)
    {
        auto cmd = GetCommandList(commandList);
        auto gpuBuffer = std::static_pointer_cast<VulkanBuffer>(buffer->internalState);
        vkCmdBindIndexBuffer(cmd->commandBuffer, gpuBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void VulkanGraphicsDevice::EndRenderPass(CommandList* commandList)
    {
        uint32_t imageIndex = swapchain_->currentImageIndex;
        vkCmdEndRenderPass(commandBuffer_);

		VkImageMemoryBarrier renderEndBarrier = CreateImageBarrier(swapchain_->images[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderEndBarrier);
    }

    void VulkanGraphicsDevice::SubmitCommandList(CommandList* commandList)
    {
        VulkanCommandList* cmdList = GetCommandList(commandList);

        VK_CHECK(vkEndCommandBuffer(cmdList->commandBuffer));

        cmdList->signalSemaphore.push_back(releaseSemaphore_);

        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.waitSemaphoreCount = (uint32_t)cmdList->waitSemaphore.size();
        submitInfo.pWaitSemaphores = cmdList->waitSemaphore.data();
        submitInfo.pWaitDstStageMask = cmdList->waitStages.data();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdList->commandBuffer;
        submitInfo.signalSemaphoreCount = (uint32_t)cmdList->signalSemaphore.size();
        submitInfo.pSignalSemaphores = cmdList->signalSemaphore.data();

        vkQueueSubmit(queue_, 1, &submitInfo, VK_NULL_HANDLE);
    }

    void VulkanGraphicsDevice::Present()
    {
        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.pImageIndices = &swapchain_->currentImageIndex;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &releaseSemaphore_;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain_->swapchain;

        VK_CHECK(vkQueuePresentKHR(queue_, &presentInfo));

        commandList_->signalSemaphore.clear();
        commandList_->waitSemaphore.clear();
        commandList_->waitStages.clear();

        destroyReleasedResources();
    }

    void VulkanGraphicsDevice::CopyBuffer(GPUBuffer* dst, GPUBuffer* src)
    {
        uint32_t copySize = static_cast<uint32_t>(std::min(dst->desc.size, src->desc.size));
        if (dst->mappedDataPtr && src->mappedDataPtr)
            std::memcpy(dst->mappedDataPtr, src->mappedDataPtr, copySize);
        else 
        {
			auto srcBuffer = std::static_pointer_cast<VulkanBuffer>(src->internalState);
            auto dstBuffer = std::static_pointer_cast<VulkanBuffer>(dst->internalState);

			VK_CHECK(vkResetCommandPool(device_, stagingCmdPool_, 0));
            VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			VK_CHECK(vkBeginCommandBuffer(stagingCmdBuffer_, &beginInfo));

            VkBufferCopy region = {0, 0, VkDeviceSize(copySize)};
			vkCmdCopyBuffer(stagingCmdBuffer_, srcBuffer->buffer, dstBuffer->buffer, 1, &region);
            
			VK_CHECK(vkEndCommandBuffer(stagingCmdBuffer_));
            VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &stagingCmdBuffer_;
            VK_CHECK(vkQueueSubmit(queue_, 1, &submitInfo, VK_NULL_HANDLE));
            vkQueueWaitIdle(queue_);
        }
    }

    void VulkanGraphicsDevice::UpdateDescriptor(Pipeline* pipeline, DescriptorInfo* descriptorInfo, uint32_t descriptorInfoCount)
    {

        auto vkPipeline = std::static_pointer_cast<VulkanPipeline>(pipeline->internalState);

        // TODO: This is currently static and needs to be changed later when we want to use buffer with offset and size varying every frame
        if (vkPipeline->descriptorSet)
            return;

        std::vector<VulkanDescriptorInfo> descriptorInfos(descriptorInfoCount);
        for (uint32_t i = 0; i < descriptorInfoCount; ++i)
        {
            DescriptorInfo info = *(descriptorInfo + i);
            if (info.resource->IsBuffer())
            {
                auto vkBuffer = std::static_pointer_cast<VulkanBuffer>(info.resource->internalState);
                descriptorInfos[i].bufferInfo.buffer = vkBuffer->buffer;
                descriptorInfos[i].bufferInfo.offset = info.offset;
                descriptorInfos[i].bufferInfo.range = info.size;
            }
            else
                assert(!"Unsupported Descriptor Type");
        }

        VkDescriptorSetAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocateInfo.descriptorPool = descriptorPool_;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &vkPipeline->setLayout;
        
        VkDescriptorSet set = 0;
		VK_CHECK(vkAllocateDescriptorSets(device_, &allocateInfo, &set));
        vkPipeline->descriptorSet = set;

        vkUpdateDescriptorSetWithTemplate(device_, set, vkPipeline->updateTemplate,  descriptorInfos.data());
    }

    void VulkanGraphicsDevice::WaitForGPU()
    {
        VK_CHECK(vkDeviceWaitIdle(device_));
    }


    VulkanGraphicsDevice::~VulkanGraphicsDevice()
    {
        gAllocationHandler.destroyedImageViews_.insert(gAllocationHandler.destroyedImageViews_.end(), swapchain_->imageViews.begin(), swapchain_->imageViews.end());
        swapchain_->images.clear();
        swapchain_->imageViews.clear();

        if (swapchain_->depthImages.size() > 0)
        {
            gAllocationHandler.destroyedImages_.insert(gAllocationHandler.destroyedImages_.end(), swapchain_->depthImages.begin(), swapchain_->depthImages.end());
            gAllocationHandler.destroyedImageViews_.insert(gAllocationHandler.destroyedImageViews_.end(), swapchain_->depthImageViews.begin(), swapchain_->depthImageViews.end());
            swapchain_->depthImages.clear();
            swapchain_->depthImageViews.clear();
        }

        gAllocationHandler.destroyedFramebuffers_.insert(gAllocationHandler.destroyedFramebuffers_.end(), swapchain_->framebuffers.begin(), swapchain_->framebuffers.end());
        swapchain_->framebuffers.clear();

        destroyReleasedResources();

        vmaDestroyAllocator(vmaAllocator_);
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer_);
        vkFreeCommandBuffers(device_, stagingCmdPool_, 1, &stagingCmdBuffer_);
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        vkDestroyCommandPool(device_, stagingCmdPool_, nullptr);
        vkDestroySwapchainKHR(device_, swapchain_->swapchain, nullptr);
        vkDestroySurfaceKHR(instance_, swapchain_->surface, nullptr);
        vkDestroySemaphore(device_, acquireSemaphore_, nullptr);
        vkDestroySemaphore(device_, releaseSemaphore_, nullptr);
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyDevice(device_, nullptr);
        if (mValidationMode == ValidationMode::Enabled)
            vkDestroyDebugUtilsMessengerEXT(instance_, messenger_, nullptr);

        vkDestroyInstance(instance_, nullptr);

    }

    void VulkanGraphicsDevice::destroyReleasedResources()
    {
        gAllocationHandler.destroyReleasedResource(device_, vmaAllocator_);
    }





};