#define VMA_IMPLEMENTATION 
#include "VulkanGraphicsDevice.h"
#include "Logger.h"
#include "Timer.h"

#define SPIRV_REFLECT_USE_SYSTEM_SPIRV_H
#include "spirv_reflect.h"

#include <algorithm>
#include <unordered_set>

namespace gfx {

#define VK_LOAD_FUNCTION(instance, pFuncName) (vkGetInstanceProcAddr(instance, pFuncName))

    struct ShaderReflection
    {
        std::vector<VkVertexInputAttributeDescription> inputDesc;
        std::vector<VkPushConstantRange> pushConstantRanges;

        VkDescriptorSetLayoutBinding descriptorSetLayoutBinding[32];
        uint32_t descriptorSetLayoutCount = 0;
        uint32_t bindingFlags = 0;
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
        std::vector<VkSampler> destroyedSamplers_;
        std::vector<VkSemaphore> destroyedSemaphore_;
        std::vector<VkFence> destroyedFences_;

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

            for (auto& sampler : destroyedSamplers_)
                vkDestroySampler(device, sampler, nullptr);

            for (auto& semaphore : destroyedSemaphore_)
                vkDestroySemaphore(device, semaphore, nullptr);

            for (auto& fence : destroyedFences_)
                vkDestroyFence(device, fence, nullptr);

            destroyedFences_.clear();
            destroyedSemaphore_.clear();
            destroyedSamplers_.clear();
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
    }  gAllocationHandler;


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

    VkPipelineStageFlags _ConvertPipelineStageFlags(PipelineStage pipelineStage)
    {
        switch (pipelineStage)
        {
        case PipelineStage::TopOfPipe:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        case PipelineStage::FragmentShader:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case PipelineStage::VertexShader:
            return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        case PipelineStage::ComputeShader:
            return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case PipelineStage::EarlyFramentTest:
			return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        case PipelineStage::LateFragmentTest:
            return VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        case PipelineStage::BottomOfPipe:
            return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        case PipelineStage::TransferBit:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case PipelineStage::ColorAttachmentOutput:
                return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        default:
            assert(!"Undefined pipeline stage flag");
            return VK_PIPELINE_STAGE_NONE;

        }
    }

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

    VkAccessFlags _ConvertAccessFlags(AccessFlag accessFlag)
    {
        switch (accessFlag)
        {
        case AccessFlag::None:
            return VK_ACCESS_NONE;
        case AccessFlag::ShaderRead:
            return VK_ACCESS_SHADER_READ_BIT;
        case AccessFlag::ShaderWrite:
            return VK_ACCESS_SHADER_WRITE_BIT;
        case AccessFlag::ShaderReadWrite:
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        case AccessFlag::DepthStencilRead:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        case AccessFlag::DepthStencilWrite:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case AccessFlag::TransferReadBit:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case AccessFlag::TransferWriteBit:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case AccessFlag::ColorAttachmentRead:
            return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        case AccessFlag::ColorAttachmentWrite:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        default:
            assert(!"Undefined Access Flags");
            return VK_ACCESS_NONE;
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

    VkFrontFace _ConvertFrontFace(FrontFace frontFace)
    {
        switch (frontFace)
        {
        case FrontFace::Clockwise:
            return VK_FRONT_FACE_CLOCKWISE;
        case FrontFace::Anticlockwise:
            return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        default:
            return VK_FRONT_FACE_MAX_ENUM;
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
        case Format::R16G16_SFLOAT:
            return VK_FORMAT_R16G16_SFLOAT;
        case Format::R16B16G16_SFLOAT:
            return VK_FORMAT_R16G16B16_SFLOAT;
        case Format::R16B16G16A16_SFLOAT:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case Format::R32B32G32A32_SFLOAT:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::R32G32_SFLOAT:
            return VK_FORMAT_R32G32_SFLOAT;
        case Format::B8G8R8A8_UNORM:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::D16_UNORM:
            return VK_FORMAT_D16_UNORM;
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

    uint32_t _FindSizeInByte(Format format)
    {
        switch (format)
        {
        case Format::R8G8B8A8_UNORM:
            return 4;
        case Format::R16G16_SFLOAT:
            return 4;
        case Format::R16B16G16_SFLOAT:
            return 6;
        case Format::R16B16G16A16_SFLOAT:
            return 8;
        case Format::R32B32G32A32_SFLOAT:
            return 16;
        case Format::R32G32_SFLOAT:
            return 8;
        case Format::B8G8R8A8_UNORM:
            return 4;
        case Format::D16_UNORM:
            return 2;
        case Format::D32_SFLOAT:
            return 4;
        case Format::D32_SFLOAT_S8_UINT:
            return 5;
        case Format::D24_UNORM_S8_UINT:
            return 4;
        default:
            assert(!"Undefined Format");
            return VK_FORMAT_R8G8B8A8_UNORM;
        }
    }


    VkImageLayout _ConvertLayout(ImageLayout layout)
    {
        switch (layout)
        {
        case ImageLayout::PresentSrc:
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        case ImageLayout::ShaderReadOptimal:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case ImageLayout::General:
            return VK_IMAGE_LAYOUT_GENERAL;
        case ImageLayout::ColorAttachmentOptimal:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthStencilAttachmentOptimal:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthAttachmentOptimal:
            return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        case ImageLayout::TransferSrcOptimal:
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case ImageLayout::TransferDstOptimal:
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
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

    VkImageUsageFlags _ConvertImageUsage(ImageUsage usage)
    {
        VkImageUsageFlags flags = 0;
        if (HasFlag(usage, ImageUsage::DepthStencilAttachment))
            flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (HasFlag(usage, ImageUsage::ColorAttachment))
            flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (HasFlag(usage, ImageUsage::ShaderResource))
            flags |= VK_IMAGE_USAGE_SAMPLED_BIT;

        return flags;
    }

    VkImageType _ConvertImageType(ImageType imageType)
    {
        switch (imageType)
        {
        case ImageType::I2D:
			return VK_IMAGE_TYPE_2D;
        case ImageType::I3D:
            return VK_IMAGE_TYPE_3D;
        default:
            assert(!"Undefined image type");
            return VK_IMAGE_TYPE_MAX_ENUM;
		}
    }

    VkImageViewType _ConvertImageViewType(ImageViewType imageViewType)
    {
        switch (imageViewType)
        {
        case ImageViewType::IV2D:
            return VK_IMAGE_VIEW_TYPE_2D;
        case ImageViewType::IV2DArray:
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case ImageViewType::IV3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        case ImageViewType::IVCubemap:
            return VK_IMAGE_VIEW_TYPE_CUBE;
        default:
            assert(!"Undefined ImageView Type");
            return VK_IMAGE_VIEW_TYPE_MAX_ENUM;

        }
    }

    VkImageAspectFlagBits _ConvertImageAspect(ImageAspect imageAspect)
    {
        switch (imageAspect)
        {
        case ImageAspect::Color:
            return VK_IMAGE_ASPECT_COLOR_BIT;
        case ImageAspect::Depth:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        default:
            assert(!"Undefined image aspect");
            return VK_IMAGE_ASPECT_NONE;
        }
    }

    VkFilter _ConvertSamplerFilter(TextureFilter filter)
    {
        switch (filter)
        {
        case TextureFilter::Linear:
            return VK_FILTER_LINEAR;
        case TextureFilter::Nearest:
            return VK_FILTER_NEAREST;
        default:
            assert(!"Undefined texture filter");
            return VK_FILTER_MAX_ENUM;
        }
    }

    VkAttachmentLoadOp _ConvertRenderPassLoadOp(RenderPassOperation operation)
    {
        switch (operation)
        {
        case RenderPassOperation::Load:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case RenderPassOperation::Clear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        }
		return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }

    VkSamplerAddressMode _ConvertSamplerAddressMode(TextureWrapMode wrapMode)
    {
        switch (wrapMode)
        {
        case TextureWrapMode::Repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case TextureWrapMode::ClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case TextureWrapMode::ClampToBorder:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default:
            assert(!"Undefined Sampler Address Mode");
            return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
        }
    }

    /***********************************************************************************************/

    bool IsAttachmentTypeDepth(VkFormat format)
    {
        return (format == VK_FORMAT_D32_SFLOAT ||
                format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                format == VK_FORMAT_D16_UNORM ||
                format == VK_FORMAT_D24_UNORM_S8_UINT );
    }

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
        win32SurfaceCreateInfo.hwnd = Platform::GetWindowHandle(window);
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


	std::pair<VkImage, VmaAllocation> CreateSwapchainDepthImage(VmaAllocator vmaAllocator, VkFormat depthFormat, uint32_t width, uint32_t height)
    {
        VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        createInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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

    VkImageView CreateImageView(VkDevice device, VkImage image, VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspect, int layerCount = 1, int levelCount = 1, int baseLevel = 0)
    {
        VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        createInfo.image = image;
        createInfo.viewType = viewType;
        createInfo.format = format;
        createInfo.subresourceRange.aspectMask = aspect;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = layerCount;
        createInfo.subresourceRange.baseMipLevel = baseLevel;
        createInfo.subresourceRange.levelCount = levelCount;

        VkImageView imageView = 0;
        VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &imageView));
        return imageView;
    }

    // TODO: Can create once and reuse
    VkSampler CreateSampler(VkDevice device, const SamplerInfo* samplerInfo, float maxAnisotropy)
    {
        VkSamplerCreateInfo createInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

        VkFilter filter = _ConvertSamplerFilter(samplerInfo->textureFilter);
        createInfo.magFilter = filter;
        createInfo.minFilter = filter;

        VkSamplerAddressMode addressMode = _ConvertSamplerAddressMode(samplerInfo->wrapMode);
        createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        createInfo.addressModeU = addressMode;
        createInfo.addressModeV = addressMode;
        createInfo.addressModeW = addressMode;
        createInfo.anisotropyEnable = samplerInfo->enableAnisotropicFiltering;
        createInfo.maxAnisotropy = maxAnisotropy;
        createInfo.minLod = 0.0f;
        createInfo.maxLod = 16.0f;
        
        if (samplerInfo->enableBorder)
            createInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

        VkSampler sampler = 0;
		VK_CHECK(vkCreateSampler(device, &createInfo, nullptr, &sampler));
        return sampler;
    }

    VkFramebuffer createFramebufferInternal(VkDevice device, VkRenderPass renderPass, VkImageView* imageViews, uint32_t imageViewCount, uint32_t width, uint32_t height, uint32_t layers = 1)
    {
        VkFramebufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        createInfo.renderPass = renderPass;
        createInfo.attachmentCount = imageViewCount;
        createInfo.pAttachments = imageViews;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.layers = layers;

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

    VkImageMemoryBarrier CreateImageBarrier(VkImage image, VkImageAspectFlagBits aspect, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevel = 0, uint32_t arrLayer = 0, uint32_t mipCount = ~0u, uint32_t layerCount = ~0u)
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
        result.subresourceRange.baseMipLevel = mipLevel;
        result.subresourceRange.baseArrayLayer = arrLayer;
        result.subresourceRange.levelCount = mipCount;
        result.subresourceRange.layerCount = layerCount;
        return result;
    }

    VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, VkDescriptorSetLayoutBinding* setBindings, uint32_t count)
    {
        VkDescriptorSetLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        createInfo.flags = 0;
        createInfo.bindingCount = count;
        createInfo.pBindings = setBindings;

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

    VkDescriptorUpdateTemplate CreateUpdateTemplate(VkDevice device, VkPipelineBindPoint bindPoint, VkPipelineLayout layout, VkDescriptorSetLayout setLayout, const ShaderReflection& shaderRefl)
    {
        std::vector<VkDescriptorUpdateTemplateEntry> entries(shaderRefl.descriptorSetLayoutCount);
        for(uint32_t i = 0; i < shaderRefl.descriptorSetLayoutCount; ++i)
        {
			entries[i].dstBinding = shaderRefl.descriptorSetLayoutBinding[i].binding;
			entries[i].dstArrayElement = 0;
			entries[i].descriptorCount = shaderRefl.descriptorSetLayoutBinding[i].descriptorCount;
			entries[i].descriptorType = shaderRefl.descriptorSetLayoutBinding[i].descriptorType;
			entries[i].offset = sizeof(VulkanDescriptorInfo) * i;
			entries[i].stride = sizeof(VulkanDescriptorInfo);
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

    void ParseShaderReflection(const void* code, uint32_t sizeInByte, VkShaderStageFlagBits& shaderStage, ShaderReflection* out)
    {
        SpvReflectShaderModule module;
        SpvReflectResult result = spvReflectCreateShaderModule(sizeInByte, code, &module);

        assert(result == SPV_REFLECT_RESULT_SUCCESS);
		shaderStage = GetShaderStage(module.shader_stage);

        uint32_t inputCount = 0;
        result = spvReflectEnumerateInputVariables(&module, &inputCount, nullptr);
        std::vector<SpvReflectInterfaceVariable*> inputAttributes(inputCount);
        result = spvReflectEnumerateInputVariables(&module, &inputCount, inputAttributes.data());

        // Parse Input Variables
        for (uint32_t i = 0; i < inputCount && shaderStage != VK_SHADER_STAGE_FRAGMENT_BIT; ++i)
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
                uint32_t binding = reflBinding->binding;
                if (out->bindingFlags & (1 << binding))
                {
                    out->descriptorSetLayoutBinding[binding].stageFlags |= shaderStage;
                    continue;
                }
                out->bindingFlags |= (1 << binding);

                VkDescriptorSetLayoutBinding layoutBinding = {};
                layoutBinding.binding = binding;
                layoutBinding.descriptorType = static_cast<VkDescriptorType>(reflBinding->descriptor_type);
                layoutBinding.descriptorCount = reflBinding->count;
                layoutBinding.stageFlags = shaderStage;
                out->descriptorSetLayoutBinding[binding] = layoutBinding;
                out->descriptorSetLayoutCount++;
            }
        }

        // Push Constants
        uint32_t pushConstantCount = 0;
        result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, nullptr);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);
        if (pushConstantCount > 0)
        {
            std::vector<SpvReflectBlockVariable*> pushConstants(pushConstantCount);
            result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, pushConstants.data());
            assert(result == SPV_REFLECT_RESULT_SUCCESS);

            for (auto& pushConstant : pushConstants)
            {
                VkPushConstantRange pushConstantRange = {};
                pushConstantRange.size = pushConstant->size;
                pushConstantRange.offset = pushConstant->offset;
                pushConstantRange.stageFlags = shaderStage;
                out->pushConstantRanges.push_back(pushConstantRange);
            }
        }

        spvReflectDestroyShaderModule(&module);
    }


    /***********************************************************************************************/  

    VkImage createImageInternal(VmaAllocator& allocator, VkImageCreateInfo* createInfo, VmaAllocation* allocation)
    {
        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        VkImage image = 0;
        VK_CHECK(vmaCreateImage(allocator, createInfo, &allocCreateInfo, &image, allocation, nullptr));
        return image;
    }


    bool VulkanGraphicsDevice::createSwapchainInternal()
    {
        VkSurfaceKHR surface = swapchain_->surface;

        VkSurfaceCapabilitiesKHR surfaceCaps = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface, &surfaceCaps));

        if (surfaceCaps.currentExtent.width == 0 || surfaceCaps.currentExtent.height == 0)
            return false;

        swapchain_->width = surfaceCaps.currentExtent.width;
        swapchain_->height = surfaceCaps.currentExtent.height;

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
        surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
        surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        VkFormat requiredFormat = VK_FORMAT_B8G8R8A8_UNORM;
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
        if (swapchain_->vsync)
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
            presentMode = VK_PRESENT_MODE_FIFO_KHR;
        }

        uint32_t width = swapchain_->width;
        uint32_t height = swapchain_->height;

        VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        createInfo.surface = surface;
        createInfo.minImageCount = std::max(2u, surfaceCaps.minImageCount);
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent.width = width;
        createInfo.imageExtent.height = height;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        createInfo.queueFamilyIndexCount = 1;
        createInfo.pQueueFamilyIndices = &queueFamilyIndices_;
        createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        createInfo.compositeAlpha = surfaceComposite;
        createInfo.presentMode = presentMode;

        VkSwapchainKHR oldSwapchain = swapchain_->swapchain;
        createInfo.oldSwapchain = oldSwapchain;

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
		swapchain_->depthImages.resize(imageCount);
		swapchain_->depthImageViews.resize(imageCount);

        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        for (uint32_t i = 0; i < imageCount; ++i)
        {
			swapchain_->depthImages[i] = CreateSwapchainDepthImage(vmaAllocator_, depthFormat, width, height);
			swapchain_->depthImageViews[i] = CreateImageView(device_, swapchain_->depthImages[i].first, VK_IMAGE_VIEW_TYPE_2D, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
            swapchain_->imageViews[i] = CreateImageView(device_, swapchain_->images[i], VK_IMAGE_VIEW_TYPE_2D, swapchain_->format.format, VK_IMAGE_ASPECT_COLOR_BIT);

            std::vector<VkImageView> imageViews = { swapchain_->imageViews[i] };

			imageViews.push_back(swapchain_->depthImageViews[i]);
            swapchain_->framebuffers[i] = createFramebufferInternal(device_, swapchain_->renderPass->renderPass, imageViews.data(), static_cast<uint32_t>(imageViews.size()), width, height);
        }

        vkDestroySwapchainKHR(device_, oldSwapchain, nullptr);

        return true;
    }

    void VulkanGraphicsDevice::findAvailableInstanceExtensions(const std::vector<VkExtensionProperties>& availableExtensions, std::vector<const char*>& outExtensions)
    {
		std::vector<const char*> requiredExtensions = {
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
            VK_KHR_SURFACE_EXTENSION_NAME,
    #if defined VK_USE_PLATFORM_WIN32_KHR
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    #endif
        };

        if (mValidationMode == ValidationMode::Enabled)
        {
            requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			requiredExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        }

        for (auto required : requiredExtensions)
        {
            for (auto& extension : availableExtensions)
            {
                if (strcmp(extension.extensionName, required) == 0)
                {
                    if (std::strcmp(extension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
                        debugMarkerEnabled_ = true;

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

            vkGetPhysicalDeviceProperties2(physicalDevice, &properties2_);
            Logger::Info("Enumerating extension for device: " + std::string(properties2_.properties.deviceName));

            bool suitable = true;
            for (auto& extension : requiredDeviceExtensions)
            {
                bool found = false;
                for (auto& available : availableExtension)
                {
                    if (std::strcmp(extension, available.extensionName) == 0)
					{
                        Logger::Debug("Found extension: " + std::string(extension));
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    Logger::Debug("Extension not available: " + std::string(extension));
                    suitable = false;
                    break;
                }
            }

            if (!suitable) continue;

#if USE_INTEGRATED_GPU
            if (properties2_.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                return physicalDevice;
#else 
            if (properties2_.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                return physicalDevice;
#endif
        }

        for (uint32_t i = 0; i < deviceCount; ++i)
        {
            vkGetPhysicalDeviceProperties2(devices[i], &properties2_);
#if USE_INTEGRATED_GPU
            if (properties2_.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                return devices[i];
#else 
            if (properties2_.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                // Although the debug VK_EXT_DEBUG_MARKER_EXTENSION_NAME is not present
                // in available extension it works in Nvidia GPU
				//debugMarkerEnabled_ = true;
                return devices[i];
            }
#endif
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

    VkPipelineLayout VulkanGraphicsDevice::createPipelineLayout(VkDescriptorSetLayout* setLayout, uint32_t setLayoutCount, const std::vector<VkPushConstantRange>& ranges)
    {
        VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        createInfo.setLayoutCount = setLayoutCount;
        createInfo.pSetLayouts = setLayout;
		createInfo.pushConstantRangeCount = static_cast<uint32_t>(ranges.size());
		createInfo.pPushConstantRanges = ranges.data();

        VkPipelineLayout pipelineLayout = 0;
        VK_CHECK(vkCreatePipelineLayout(device_, &createInfo, nullptr, &pipelineLayout));
        return pipelineLayout;
    }
    bool VulkanGraphicsDevice::isSwapchainResized()
    {
        VkSurfaceCapabilitiesKHR surfaceCaps = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, swapchain_->surface, &surfaceCaps));

        int width = swapchain_->width;
        int height = swapchain_->height;

        if (surfaceCaps.currentExtent.width != width || surfaceCaps.currentExtent.height != height)
            return true;

        return false;
    }


    VulkanGraphicsDevice::VulkanGraphicsDevice(Platform::WindowType window, ValidationMode validationMode)
    {
        // Begin Timer
        Timer timer;

        mValidationMode = validationMode;

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
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
        };

        // Create physical device
        physicalDevice_ = findSuitablePhysicalDevice(requiredDeviceExtensions);

        // Check for extension support
        uint32_t deviceExtensionCount = 0;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &deviceExtensionCount, nullptr));
        std::vector<VkExtensionProperties> availableExtension(deviceExtensionCount);
        VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &deviceExtensionCount, availableExtension.data()));
        for (auto& extension : availableExtension)
        {
            if (std::strcmp(extension.extensionName, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) == 0)
            {
                supportTimelineSemaphore = true;
                Logger::Info("Timeline semaphore supported");
            }
        }

        // Check for bindless support
        VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT, nullptr };
#ifdef ENABLE_BINDLESS
        VkPhysicalDeviceFeatures2 deviceFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &indexingFeatures};
        vkGetPhysicalDeviceFeatures2(physicalDevice_, &deviceFeatures);
        supportBindless = indexingFeatures.descriptorBindingPartiallyBound && 
            indexingFeatures.runtimeDescriptorArray &&
            indexingFeatures.descriptorBindingSampledImageUpdateAfterBind && 
            indexingFeatures.descriptorBindingStorageImageUpdateAfterBind;
#endif

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
        features2_.features.fillModeNonSolid = true;
        features2_.features.wideLines = true;
        features2_.features.samplerAnisotropy = true;
        features2_.features.geometryShader = true;
        features2_.features.depthClamp = true;

        features11_.shaderDrawParameters = true;

        features12_.drawIndirectCount = true;
		features12_.shaderInt8 = true;
        features12_.hostQueryReset = true;
        features12_.uniformAndStorageBuffer8BitAccess = true;
        features12_.separateDepthStencilLayouts = true;
        features12_.shaderSampledImageArrayNonUniformIndexing = true;
        features12_.timelineSemaphore = true;

        if (supportBindless)
        {
            Logger::Info("Bindless support found");
            features12_.descriptorBindingPartiallyBound = VK_TRUE;
            features12_.runtimeDescriptorArray = VK_TRUE;
            features12_.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
            features12_.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
        }
        features2_.pNext = &features11_;
        features11_.pNext = &features12_;

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

        stagingCmdPool_   = CreateCommandPool(device_, queueFamilyIndices_);
        stagingCmdBuffer_ = CreateCommandBuffer(device_, stagingCmdPool_);

        commandList_ = std::make_shared<VulkanCommandList>();

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
		//vulkanFunctions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;
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

        Logger::Debug("Created VulkanGraphicsDevice (" + std::to_string(timer.elapsedSeconds()) + "s)");

        // Initialize Resource Pool
        renderPasses.Initialize(256);
        pipelines.Initialize(128);
        buffers.Initialize(256);
        textures.Initialize(1024);
        framebuffers.Initialize(64);

        if (supportBindless)
        {
            VkDescriptorPoolSize poolSizeBindless[] =
            {
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxBindlessResources },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, kMaxBindlessResources },
            };

            uint32_t poolCount = ARRAYSIZE(poolSizeBindless);
            VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
            poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
            poolInfo.maxSets = kMaxBindlessResources * poolCount;
            poolInfo.poolSizeCount = ARRAYSIZE(poolSizeBindless);
            poolInfo.pPoolSizes = poolSizeBindless;
            VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &bindlessDescriptorPool_));

            VkDescriptorSetLayoutBinding vkBinding[4];
            // Actual descriptor set layout
            VkDescriptorSetLayoutBinding& imageSamplerBinding = vkBinding[0];
            imageSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            imageSamplerBinding.descriptorCount = kMaxBindlessResources;
            imageSamplerBinding.binding = kBindlessTextureBinding;
            imageSamplerBinding.stageFlags = VK_SHADER_STAGE_ALL;
            imageSamplerBinding.pImmutableSamplers = nullptr;

            VkDescriptorSetLayoutBinding& storageImageBinding = vkBinding[1];
            storageImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            storageImageBinding.descriptorCount = kMaxBindlessResources;
            storageImageBinding.binding = kBindlessTextureBinding + 1;
            storageImageBinding.stageFlags = VK_SHADER_STAGE_ALL;
            storageImageBinding.pImmutableSamplers = nullptr;

            VkDescriptorSetLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
            layout_info.bindingCount = poolCount;
            layout_info.pBindings = vkBinding;
            layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;

            VkDescriptorBindingFlags bindlessFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
                /*VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |*/
                VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;

            VkDescriptorBindingFlags bindingFlags[4];
            bindingFlags[0] = bindlessFlags;
            bindingFlags[1] = bindlessFlags;

            VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT, nullptr };
            extended_info.bindingCount = poolCount;
            extended_info.pBindingFlags = bindingFlags;
            layout_info.pNext = &extended_info;
            vkCreateDescriptorSetLayout(device_, &layout_info, nullptr, &bindlessDescriptorLayout_);

            VkDescriptorSetAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
            alloc_info.descriptorPool = bindlessDescriptorPool_;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &bindlessDescriptorLayout_;

            VkDescriptorSetVariableDescriptorCountAllocateInfoEXT count_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT };
            uint32_t maxBindings = kMaxBindlessResources - 1;
            count_info.descriptorSetCount = 1;
            count_info.pDescriptorCounts = &maxBindings;
            VK_CHECK(vkAllocateDescriptorSets(device_, &alloc_info, &bindlessDescriptorSet_));
        }

        // Create synchronization primitives accordingly
        VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VK_CHECK(vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &mImageAcquireSemaphore));

        supportTimelineSemaphore = false;
        if (supportTimelineSemaphore)
        {
            VkSemaphoreTypeCreateInfo typeCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
            typeCreateInfo.initialValue = 0;
            typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
            semaphoreCreateInfo.pNext = &typeCreateInfo;
            VK_CHECK(vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &mRenderSemaphore));
            VK_CHECK(vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &mComputeSemaphore));
        }
        else {
            VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
            fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            for (uint32_t i = 0; i < kMaxFrame; ++i)
            {
				VK_CHECK(vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &mRenderCompleteSemaphore[i]));
                VK_CHECK(vkCreateFence(device_, &fenceCreateInfo, nullptr, &mInFlightFences[i]));
            }
            // Create compute fence
            VK_CHECK(vkCreateFence(device_, &fenceCreateInfo, nullptr, &mComputeFence_));
        }
    }

    bool VulkanGraphicsDevice::CreateSwapchain(Platform::WindowType window)
    {
        if (swapchain_ == nullptr)
        {
			swapchain_ = std::make_unique<VulkanSwapchain>();

            gfx::RenderPassDesc renderPassDesc = {};
            renderPassDesc.colorAttachments.emplace_back(Attachment{ Format::B8G8R8A8_UNORM, RenderPassOperation::Clear, ImageAspect::Color });

            renderPassDesc.hasDepthAttachment = true;
            renderPassDesc.depthAttachment = { Format::D32_SFLOAT, RenderPassOperation::Clear, ImageAspect::Depth };
            mSwapchainRP = CreateRenderPass(&renderPassDesc);
            swapchain_->renderPass = renderPasses.AccessResource(mSwapchainRP.handle);
			swapchain_->surface = CreateSurface(instance_, physicalDevice_, queueFamilyIndices_, window);
            swapchain_->vsync = true;
        }

        VulkanRenderPass* vkRenderPass = swapchain_->renderPass;
        assert(vkRenderPass != nullptr);
		createSwapchainInternal();

		
        if (commandPool_ == nullptr)
        {
            commandPool_ = new VkCommandPool[swapchain_->imageCount];
            commandBuffer_ = new VkCommandBuffer[swapchain_->imageCount];
            for (uint32_t i = 0; i < swapchain_->imageCount; ++i)
            {
                commandPool_[i] = CreateCommandPool(device_, queueFamilyIndices_);
                commandBuffer_[i] = CreateCommandBuffer(device_, commandPool_[i]);
            }
        }
        else {
            commandList_->commandPool = commandPool_[currentFrame];
            commandList_->commandBuffer = commandBuffer_[currentFrame];
        }

        return true;
    }

    VkRenderPass VulkanGraphicsDevice::createRenderPass(const RenderPassDesc* desc)
    {
        uint32_t colorAttachmentCount = (uint32_t)desc->colorAttachments.size();
        std::vector<VkAttachmentDescription> attachments(colorAttachmentCount);
        std::vector<VkAttachmentReference> attachmentRef(colorAttachmentCount);

        bool hasDepthAttachment = desc->hasDepthAttachment;

        for (uint32_t i = 0; i < colorAttachmentCount; ++i)
        {
            const Attachment& attachment = desc->colorAttachments[i];
            VkFormat format = _ConvertFormat(attachment.format);

            VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            assert(i < colorAttachmentCount);

            VkAttachmentLoadOp loadOp = _ConvertRenderPassLoadOp(attachment.operation);
            attachments[i].format = format;
            attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
            attachments[i].loadOp = loadOp;
            attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachments[i].stencilLoadOp = loadOp;
            attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[i].initialLayout = layout;
            attachments[i].finalLayout = layout;
            attachmentRef[i] = { i, layout};
        }

        VkAttachmentReference depthStencilAttachmentRef = {};
        if (desc->hasDepthAttachment)
        {
            const Attachment& attachment = desc->depthAttachment;
            VkFormat format = _ConvertFormat(attachment.format);

            VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            uint32_t index = static_cast<uint32_t>(attachments.size());
            VkAttachmentLoadOp loadOp = _ConvertRenderPassLoadOp(attachment.operation);

            VkAttachmentDescription attachmentDesc = {};
            attachmentDesc.format = format;
            attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
            attachmentDesc.loadOp = loadOp;
            attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachmentDesc.stencilLoadOp = loadOp;
            attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDesc.initialLayout = layout;
            attachmentDesc.finalLayout = layout;
            attachments.push_back(attachmentDesc);

            depthStencilAttachmentRef.attachment = index;
			depthStencilAttachmentRef.layout = layout;
        }

        VkRenderPassCreateInfo createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        createInfo.attachmentCount = (uint32_t)attachments.size();
        createInfo.pAttachments = attachments.data();

        VkSubpassDescription subpassDescription = {};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        subpassDescription.colorAttachmentCount = static_cast<uint32_t>(attachmentRef.size());
        subpassDescription.pColorAttachments = attachmentRef.data();

        if(desc->hasDepthAttachment)
			subpassDescription.pDepthStencilAttachment = &depthStencilAttachmentRef;

        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpassDescription;

        VkRenderPass renderPass = 0;
        VK_CHECK(vkCreateRenderPass(device_, &createInfo, nullptr, &renderPass));
        return renderPass;
    }

    RenderPassHandle VulkanGraphicsDevice::CreateRenderPass(const RenderPassDesc* desc)
    {
        RenderPassHandle handle = { renderPasses.ObtainResource()};
        VulkanRenderPass* vkRenderPass = renderPasses.AccessResource(handle.handle);
        vkRenderPass->hasDepthAttachment = desc->hasDepthAttachment;
        vkRenderPass->numColorAttachments = (uint32_t)desc->colorAttachments.size();
        vkRenderPass->renderPass = createRenderPass(desc);
        return handle;
    }

    FramebufferHandle VulkanGraphicsDevice::CreateFramebuffer(const FramebufferDesc* desc)
    {
        auto rp = renderPasses.AccessResource(desc->renderPass.handle);
        FramebufferHandle fbHandle = { framebuffers.ObtainResource() };
        VulkanFramebuffer* vkFramebuffer = framebuffers.AccessResource(fbHandle.handle);
		uint32_t attachmentCount = (uint32_t)desc->outputTextures.size();

        std::vector<VkImageView> imageViews(attachmentCount);
        for (uint32_t i = 0; i < desc->outputTextures.size(); ++i)
        {
            TextureHandle attachment = desc->outputTextures[i];
            vkFramebuffer->attachments[i] = attachment;
            VulkanTexture* texture = textures.AccessResource(attachment.handle);
            imageViews[i] = texture->imageViews[0];
        }

        vkFramebuffer->attachmentCount = attachmentCount;

        if (desc->hasDepthStencilAttachment)
        {
            TextureHandle depthStencilAttachment = desc->depthStencilAttachment;
            vkFramebuffer->depthAttachment = depthStencilAttachment;

            VkImageView depthStencilImageView = textures.AccessResource(depthStencilAttachment.handle)->imageViews[0];
            imageViews.push_back(depthStencilImageView);
        }

        vkFramebuffer->width = desc->width;
        vkFramebuffer->height = desc->height;
        vkFramebuffer->layers = desc->layers;
        vkFramebuffer->framebuffer = createFramebufferInternal(device_, rp->renderPass, imageViews.data(), (uint32_t)imageViews.size(), desc->width, desc->height, desc->layers);
        return fbHandle;
    }
    void VulkanGraphicsDevice::CreateQueryPool(QueryPool* out, uint32_t count, QueryType type)
    {
        auto vkQueryPool = std::make_shared<VulkanQueryPool>();
        VkQueryPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
        if (type == QueryType::TimeStamp)
            createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        else
            assert(!"Undefined QueryType");

        createInfo.queryCount = count;
		VK_CHECK(vkCreateQueryPool(device_, &createInfo, 0, &vkQueryPool->queryPool));
       
        queryPools_.push_back(vkQueryPool->queryPool);
        out->internalState = vkQueryPool;
    }

    void VulkanGraphicsDevice::ResetQueryPool(CommandList* commandList, QueryPool* pool, uint32_t first, uint32_t count)
    {
        auto queryPool = std::static_pointer_cast<VulkanQueryPool>(pool->internalState)->queryPool;
        auto cmd = GetCommandList(commandList);
        vkCmdResetQueryPool(cmd->commandBuffer, queryPool, first, count);
    }

    void VulkanGraphicsDevice::Query(CommandList* commandList, QueryPool* pool, uint32_t index)
    {
        auto cmdList = GetCommandList(commandList);
        auto queryPool = std::static_pointer_cast<VulkanQueryPool>(pool->internalState)->queryPool;
        if (pool->queryType == QueryType::TimeStamp)
            vkCmdWriteTimestamp(cmdList->commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, queryPool, index);
        else
            assert(!"Undefined Query Type");
    }

    void VulkanGraphicsDevice::ResolveQuery(QueryPool* pool, uint32_t index, uint32_t count, uint64_t* result)
    {
        auto queryPool = std::static_pointer_cast<VulkanQueryPool>(pool->internalState)->queryPool;

		VK_CHECK(vkGetQueryPoolResults(device_, queryPool, index, count, sizeof(uint64_t) * count, result, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
    }

    PipelineHandle VulkanGraphicsDevice::CreateGraphicsPipeline(const PipelineDesc* desc)
    {
        PipelineHandle pipelineHandle = { pipelines.ObtainResource() };
        VulkanPipeline* vkPipeline = pipelines.AccessResource(pipelineHandle.handle);
        std::memset(vkPipeline, 0, sizeof(VulkanPipeline));
		
		vkPipeline->bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        std::vector<VkPipelineShaderStageCreateInfo> stages(desc->shaderCount);
        ShaderReflection shaderReflection;
        for (uint32_t i = 0; i < desc->shaderCount; ++i)
        {
            ShaderDescription& shader = desc->shaderDesc[i];
            VkShaderStageFlagBits shaderStage = {};
            ParseShaderReflection(shader.code, shader.sizeInByte, shaderStage, &shaderReflection);
            VkShaderModule shaderModule = createShader(device_, shader.code, shader.sizeInByte, shaderStage);

            stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[i].stage = shaderStage;
            stages[i].module = shaderModule;
            stages[i].pName = "main";

            vkPipeline->shaderModules.push_back(shaderModule);
        }

        createInfo.stageCount = desc->shaderCount;
        createInfo.pStages = stages.data();

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
        rasterizationState.frontFace = _ConvertFrontFace(rs.frontFace);
        rasterizationState.polygonMode = _ConvertPolygonMode(rs.polygonMode);
        createInfo.pRasterizationState = &rasterizationState;
        rasterizationState.depthClampEnable = rs.enableDepthClamp;

        VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        createInfo.pMultisampleState = &multisampleState;

        VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        if (rs.enableDepthTest)
        {
            depthStencilState.depthTestEnable = rs.enableDepthTest;
            depthStencilState.depthWriteEnable = rs.enableDepthWrite;
            depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            depthStencilState.maxDepthBounds = 1.0f;
            depthStencilState.minDepthBounds = 0.0f;
        }
        createInfo.pDepthStencilState = &depthStencilState;

        std::vector< VkPipelineColorBlendAttachmentState> colorAttachmentState(desc->blendStateCount);
        for (uint32_t i = 0; i < desc->blendStateCount; ++i)
        {
            const BlendState* bs = (desc->blendStates + i);
            if (bs->enable)
            {
                colorAttachmentState[i].blendEnable = true;
                colorAttachmentState[i].srcColorBlendFactor = _ConvertBlendFactor(bs->srcColor);
                colorAttachmentState[i].dstColorBlendFactor = _ConvertBlendFactor(bs->dstColor);
                colorAttachmentState[i].srcAlphaBlendFactor = _ConvertBlendFactor(bs->srcAlpha);
                colorAttachmentState[i].dstAlphaBlendFactor = _ConvertBlendFactor(bs->dstAlpha);
            }
            colorAttachmentState[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        }

        VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlendState.attachmentCount = desc->blendStateCount;
        colorBlendState.pAttachments = colorAttachmentState.data();
        createInfo.pColorBlendState = &colorBlendState;

        VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
        dynamicState.pDynamicStates = dynamicStates;
        createInfo.pDynamicState = &dynamicState;

        bool hasBindless = shaderReflection.descriptorSetLayoutBinding[kBindlessTextureBinding].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts(1);
        if (hasBindless)
        {
            // Swap the last binding with the bindless and decrease count
            uint32_t& descriptorCount = shaderReflection.descriptorSetLayoutCount;
            descriptorCount -= 1;
            if(descriptorCount != kBindlessTextureBinding)
				shaderReflection.descriptorSetLayoutBinding[kBindlessTextureBinding] = shaderReflection.descriptorSetLayoutBinding[descriptorCount];
            vkPipeline->hasBindless = true;
            descriptorSetLayouts.push_back(bindlessDescriptorLayout_);
        }

        vkPipeline->setLayout = CreateDescriptorSetLayout(device_, shaderReflection.descriptorSetLayoutBinding, shaderReflection.descriptorSetLayoutCount);
        descriptorSetLayouts[0] = vkPipeline->setLayout;

		VkPipelineLayout pipelineLayout = createPipelineLayout(descriptorSetLayouts.data(), (uint32_t)descriptorSetLayouts.size(), shaderReflection.pushConstantRanges);
        createInfo.layout = pipelineLayout;

		createInfo.renderPass = renderPasses.AccessResource(desc->renderPass.handle)->renderPass;

        VkPipeline pipeline = 0;
        VK_CHECK(vkCreateGraphicsPipelines(device_, 0, 1, &createInfo, 0, &pipeline));

        vkPipeline->pipelineLayout = pipelineLayout;
        vkPipeline->pipeline = pipeline;
        vkPipeline->updateTemplate = CreateUpdateTemplate(device_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, vkPipeline->setLayout, shaderReflection);
        return pipelineHandle;
    }

    PipelineHandle VulkanGraphicsDevice::CreateComputePipeline(const PipelineDesc* desc)
    {
        PipelineHandle pipelineHandle = { pipelines.ObtainResource() };
        VulkanPipeline* vkPipeline = pipelines.AccessResource(pipelineHandle.handle);
        std::memset(vkPipeline, 0, sizeof(VulkanPipeline));

        vkPipeline->bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

        assert(desc->shaderCount > 0);
        ShaderDescription* shaderDesc = desc->shaderDesc;
        assert(shaderDesc != nullptr);

        VkShaderStageFlagBits shaderStageFlag = {};
        ShaderReflection shaderRefl = {};
        ParseShaderReflection(shaderDesc->code, shaderDesc->sizeInByte, shaderStageFlag, &shaderRefl);

        VkShaderModule shaderModule = createShader(device_, shaderDesc->code, shaderDesc->sizeInByte, VK_SHADER_STAGE_COMPUTE_BIT);
        VkPipelineShaderStageCreateInfo shaderStage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        shaderStage.stage = shaderStageFlag;
        shaderStage.module = shaderModule;
        shaderStage.pName = "main";
        vkPipeline->shaderModules.push_back(shaderModule);
        vkPipeline->setLayout = CreateDescriptorSetLayout(device_, shaderRefl.descriptorSetLayoutBinding, shaderRefl.descriptorSetLayoutCount);
        vkPipeline->pipelineLayout = createPipelineLayout(&vkPipeline->setLayout, 1, shaderRefl.pushConstantRanges);

        VkComputePipelineCreateInfo createInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        createInfo.stage = shaderStage;
        createInfo.layout = vkPipeline->pipelineLayout;
		VK_CHECK(vkCreateComputePipelines(device_, 0, 1, &createInfo, nullptr, &vkPipeline->pipeline));

        vkPipeline->updateTemplate = CreateUpdateTemplate(device_, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipeline->pipelineLayout, vkPipeline->setLayout, { shaderRefl });
        return pipelineHandle;
    }

    BufferHandle VulkanGraphicsDevice::CreateBuffer(const GPUBufferDesc* desc)
    {
        BufferHandle bufferHandle = { buffers.ObtainResource() };
        VulkanBuffer* buffer = buffers.AccessResource(bufferHandle.handle);
        std::memset(buffer, 0, sizeof(VulkanBuffer));

        buffer->desc = *desc;

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
        if (HasFlag(desc->bindFlag, BindFlag::IndirectBuffer))
            usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
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

        VK_CHECK(vmaCreateBuffer(vmaAllocator_, &createInfo, &allocCreateInfo, &buffer->buffer, &buffer->allocation, nullptr));
        if (desc->usage == Usage::ReadBack || desc->usage == Usage::Upload)
        {
            buffer->mappedDataPtr = buffer->allocation->GetMappedData();
        }
        return bufferHandle;
    }

    TextureHandle VulkanGraphicsDevice::CreateTexture(const GPUTextureDesc* desc)
    {
        TextureHandle textureHandle = { textures.ObtainResource() };
        VulkanTexture* texture = textures.AccessResource(textureHandle.handle);
        std::memset(texture, 0, sizeof(VulkanTexture));

        texture->width = desc->width;
        texture->height = desc->height;
        texture->depth = desc->depth;
        texture->mipLevels = desc->mipLevels;
        texture->arrayLayers = desc->arrayLayers;
        texture->sizePerPixelByte = _FindSizeInByte(desc->format);

        VkFormat format = _ConvertFormat(desc->format);
        texture->format = format;

        VkImageAspectFlagBits imageAspect = _ConvertImageAspect(desc->imageAspect);
        texture->imageAspect = imageAspect;

        VkImageUsageFlags usage = 0;
        bool depthAttachment = false;
        if (HasFlag(desc->bindFlag, BindFlag::DepthStencil))
        {
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            depthAttachment = true;
        }

        if (HasFlag(desc->bindFlag, BindFlag::RenderTarget))
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if (desc->bCreateSampler && HasFlag(desc->bindFlag, BindFlag::ShaderResource))
            usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

        if (HasFlag(desc->bindFlag, BindFlag::StorageImage))
            usage |=  VK_IMAGE_USAGE_STORAGE_BIT;

        if(!depthAttachment)
			usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        VkImageCreateFlags imageCreateFlags = 0;
        if (desc->imageViewType == ImageViewType::IVCubemap)
            imageCreateFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        createInfo.flags = imageCreateFlags;
        createInfo.usage = usage;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        createInfo.format = format;
        createInfo.arrayLayers = desc->arrayLayers;
        createInfo.mipLevels = desc->mipLevels;
        createInfo.extent.width = desc->width;
        createInfo.extent.height = desc->height;
        createInfo.extent.depth = desc->depth;
        createInfo.imageType = _ConvertImageType(desc->imageType);
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;


        VmaAllocation allocation = {};
        VkImage image = createImageInternal(vmaAllocator_, &createInfo, &allocation);

        texture->imageViews.resize(desc->mipLevels);
        for (uint32_t i = 0; i < desc->mipLevels; ++i)
            texture->imageViews[i] = CreateImageView(device_, image, _ConvertImageViewType(desc->imageViewType), format, imageAspect, createInfo.arrayLayers, desc->mipLevels - i, i);

        if (desc->bCreateSampler)
        {
            VkSampler sampler = CreateSampler(device_, &desc->samplerInfo, properties2_.properties.limits.maxSamplerAnisotropy);
            texture->sampler = sampler;
        }

        if (desc->bAddToBindless)
            textureToUpdateBindless_.push_back(textureHandle);

        texture->image = image;
        texture->allocation = allocation;
        return textureHandle;
    }
/*
    SemaphoreHandle VulkanGraphicsDevice::CreateSemaphore(const SemaphoreDesc* desc)
    {
        SemaphoreHandle semaphoreHandle = { semaphores.ObtainResource() };
        VulkanSemaphore* vkSemaphore = semaphores.AccessResource(semaphoreHandle.handle);

        VkSemaphoreTypeCreateInfo typeCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
        if (desc->type == SemaphoreType::Timeline)
        {
            assert(supportTimelineSemaphore == true);
            typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
            typeCreateInfo.initialValue = desc->initialValue;
        }
        else {
            typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_BINARY;
        }

        VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        createInfo.pNext = &typeCreateInfo;

        VK_CHECK(vkCreateSemaphore(device_, &createInfo, nullptr, &vkSemaphore->semaphore));
        return semaphoreHandle;
    }
    */
    CommandList VulkanGraphicsDevice::BeginCommandList()
    {
        // Initialize Descriptor Pools
        if (descriptorPools_.size() == 0)
        {
            descriptorPools_.resize(swapchain_->imageCount + 1);
            for(uint32_t i = 0; i < swapchain_->imageCount + 1; ++i)
				descriptorPools_[i] = CreateDescriptorPool(device_);
        }

        // Reset Next Descriptor Pool
        uint32_t imageCount = swapchain_->imageCount;
        vkResetDescriptorPool(device_, descriptorPools_[currentFrame], 0);

        commandList_->commandBuffer = commandBuffer_[currentFrame];
        commandList_->commandPool = commandPool_[currentFrame];
        CommandList commandList = {};
        commandList.internalState = commandList_.get();

        // Update bindless descriptors
        uint32_t descriptorCount = static_cast<uint32_t>(textureToUpdateBindless_.size());
        if (supportBindless && descriptorCount > 0)
        {
            std::vector<VkWriteDescriptorSet> descriptorWrites(descriptorCount);
            std::vector<VkDescriptorImageInfo> descriptorImageInfo(descriptorCount);
            for (uint32_t i = 0; i < descriptorWrites.size(); ++i)
            {
                VulkanTexture* texture = textures.AccessResource(textureToUpdateBindless_[i].handle);
                VkWriteDescriptorSet& descriptorWrite = descriptorWrites[i];
                descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
                descriptorWrite.dstSet = bindlessDescriptorSet_;
                descriptorWrite.dstBinding = kBindlessTextureBinding;
                descriptorWrite.dstArrayElement = textureToUpdateBindless_[i].handle;
                descriptorWrite.descriptorCount = 1;
                descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

                VkDescriptorImageInfo& imageInfo = descriptorImageInfo[i];
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = texture->imageViews[0];
                imageInfo.sampler = texture->sampler;
                descriptorWrite.pImageInfo = &imageInfo;
            }

            vkUpdateDescriptorSets(device_, descriptorCount, descriptorWrites.data(), 0, nullptr);
            textureToUpdateBindless_.clear();
        }

        VulkanCommandList* cmdList = GetCommandList(&commandList);
		VK_CHECK(vkResetCommandPool(device_, cmdList->commandPool, 0));

        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandList_->commandBuffer, &beginInfo));
        return commandList;
    }

    void VulkanGraphicsDevice::BeginFrame()
    {
        VkFence currentFence[2] = { mInFlightFences[currentFrame], mComputeFence_ };
        uint32_t fenceCount = 2;

        if (vkGetFenceStatus(device_, mComputeFence_) == VK_SUCCESS)
            fenceCount = 1;

        vkWaitForFences(device_, fenceCount, currentFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device_, fenceCount, currentFence);

        // Add a signal semaphore to notify the queue that the image has been 
        // acquired for rendering
        VK_CHECK(vkAcquireNextImageKHR(device_, swapchain_->swapchain, ~0ull, mImageAcquireSemaphore, VK_NULL_HANDLE, &swapchain_->currentImageIndex));
    }

    void VulkanGraphicsDevice::PrepareSwapchain(CommandList* commandList)
    {
        auto vkCmdList = GetCommandList(commandList);

        uint32_t imageIndex = swapchain_->currentImageIndex;
        std::vector<VkImageMemoryBarrier> renderBeginBarrier = {
            CreateImageBarrier(swapchain_->images[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        };
        vkCmdPipelineBarrier(vkCmdList->commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, static_cast<uint32_t>(renderBeginBarrier.size()), renderBeginBarrier.data());

        if (swapchain_->depthImages.size() > 0)
        {
            VkImageMemoryBarrier depthBarrier = CreateImageBarrier(swapchain_->depthImages[imageIndex].first, VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            vkCmdPipelineBarrier(vkCmdList->commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &depthBarrier);
        }
    }

    void VulkanGraphicsDevice::BeginRenderPass(CommandList* commandList, RenderPassHandle renderPass, FramebufferHandle framebuffer)
    {
        VulkanRenderPass* vkRenderpass = renderPasses.AccessResource(renderPass.handle);

        VkRenderPass rp = vkRenderpass->renderPass;
        auto vkCmdList = GetCommandList(commandList);

        uint32_t imageIndex = swapchain_->currentImageIndex;
        VkFramebuffer vkFramebuffer = VK_NULL_HANDLE;

        uint32_t width = swapchain_->width;
        uint32_t height = swapchain_->height;

        uint32_t attachmentCount = vkRenderpass->numColorAttachments;
        if (framebuffer.handle != K_INVALID_RESOURCE_HANDLE)
        {
            VulkanFramebuffer* internalState = framebuffers.AccessResource(framebuffer.handle);
            vkFramebuffer = internalState->framebuffer;
            width = internalState->width;
            height = internalState->height;
        }
        else
        {
            vkFramebuffer = swapchain_->framebuffers[imageIndex];
        }

        std::vector<VkClearValue> clearValues(attachmentCount);
        for (uint32_t i = 0; i < attachmentCount; ++i)
			clearValues[i].color = { 0.5f, 0.5f, 0.5f, 1 };

        if (vkRenderpass->hasDepthAttachment)
        {
            VkClearValue depthStencilClearValue{};
            depthStencilClearValue.depthStencil = { 1.0f, 0 };
            clearValues.push_back(depthStencilClearValue);
            attachmentCount++;
        }

        VkRenderPassBeginInfo passBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        passBeginInfo.clearValueCount = attachmentCount;
        passBeginInfo.pClearValues = clearValues.data();
        passBeginInfo.framebuffer = vkFramebuffer;
        passBeginInfo.renderPass = rp;
        passBeginInfo.renderArea.extent.width = width;
        passBeginInfo.renderArea.extent.height = height;
        vkCmdBeginRenderPass(vkCmdList->commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = {0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f};
        VkRect2D scissor = { {0, 0}, {width, height} };

        vkCmdSetViewport(vkCmdList->commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(vkCmdList->commandBuffer, 0, 1, &scissor);
    }

    void VulkanGraphicsDevice::BindPipeline(CommandList* commandList, PipelineHandle pipeline)
    {
        auto cmdList = GetCommandList(commandList);
        auto vkPipeline = pipelines.AccessResource(pipeline.handle);
        vkCmdBindPipeline(cmdList->commandBuffer, vkPipeline->bindPoint, vkPipeline->pipeline);
		vkCmdBindDescriptorSets(cmdList->commandBuffer, vkPipeline->bindPoint, vkPipeline->pipelineLayout, 0, 1, &vkPipeline->descriptorSet, 0, 0);
        if (vkPipeline->hasBindless)
        {
            vkCmdBindDescriptorSets(cmdList->commandBuffer, vkPipeline->bindPoint, vkPipeline->pipelineLayout, kBindlessSet, 1, &bindlessDescriptorSet_, 0, 0);
        }
    }

    void VulkanGraphicsDevice::Draw(CommandList* commandList, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount)
    {
        auto cmd = GetCommandList(commandList);
        vkCmdDraw(cmd->commandBuffer, vertexCount, instanceCount, firstVertex, 0);
    }

    void VulkanGraphicsDevice::DrawIndexed(CommandList* commandList, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex)
    {
        auto cmd = GetCommandList(commandList);
        vkCmdDrawIndexed(cmd->commandBuffer, indexCount, instanceCount, firstIndex, 0, 0);
    }

    void VulkanGraphicsDevice::DrawIndexedIndirect(CommandList* commandList, BufferHandle indirectBuffer, uint32_t offset, uint32_t drawCount, uint32_t stride)
    {
        auto cmd = GetCommandList(commandList);
        auto dcb = buffers.AccessResource(indirectBuffer.handle);
        vkCmdDrawIndexedIndirect(cmd->commandBuffer, dcb->buffer, offset, drawCount, stride);
    }

    void VulkanGraphicsDevice::DispatchCompute(CommandList* commandList, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        auto cmd = GetCommandList(commandList);
        vkCmdDispatch(cmd->commandBuffer, groupCountX, groupCountY, groupCountZ);
    }

    bool VulkanGraphicsDevice::IsSwapchainReady()
    {
        if (isSwapchainResized())
        {
            vkDeviceWaitIdle(device_);
            return createSwapchainInternal();
        }
        return true;
    }

    void VulkanGraphicsDevice::BindIndexBuffer(CommandList* commandList, BufferHandle buffer)
    {
        auto cmd = GetCommandList(commandList);
        auto gpuBuffer = buffers.AccessResource(buffer.handle);
        vkCmdBindIndexBuffer(cmd->commandBuffer, gpuBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void VulkanGraphicsDevice::EndRenderPass(CommandList* commandList)
    {
        auto cmdList = GetCommandList(commandList);
        vkCmdEndRenderPass(cmdList->commandBuffer);
    }

    void VulkanGraphicsDevice::CopyToSwapchain(CommandList* commandList, TextureHandle texture, ImageLayout finalSwapchainImageLayout, uint32_t arrayLevel, uint32_t mipLevel)
    {
        auto cmd = GetCommandList(commandList);
        uint32_t imageIndex = swapchain_->currentImageIndex;

        auto src = textures.AccessResource(texture.handle);
        VkImageAspectFlagBits imageAspect = VK_IMAGE_ASPECT_COLOR_BIT;
        VkFilter filter = VK_FILTER_LINEAR;
        VkImage dstImage = swapchain_->images[imageIndex];
        VkPipelineStageFlagBits pipelineStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        if ((src->imageAspect & VK_IMAGE_ASPECT_DEPTH_BIT) == VK_IMAGE_ASPECT_DEPTH_BIT)
        {
            imageAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
            filter = VK_FILTER_NEAREST;
            dstImage = swapchain_->depthImages[imageIndex].first;
            pipelineStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }

        VkImageMemoryBarrier transferBarrier = CreateImageBarrier(dstImage,
            imageAspect,
            0, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vkCmdPipelineBarrier(cmd->commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &transferBarrier);

        int levelWidth  = std::max(1u, src->width >> mipLevel);
        int levelHeight = std::max(1u, src->height >> mipLevel);

        VkImageBlit blitRegion = {};
        blitRegion.srcSubresource.aspectMask = imageAspect;
        blitRegion.srcSubresource.mipLevel = mipLevel;
        blitRegion.srcSubresource.layerCount = 1;

        blitRegion.dstSubresource.aspectMask = imageAspect;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.srcOffsets[0] = { 0, levelHeight, 0 };
        blitRegion.srcOffsets[1] = { levelWidth, 0, 1 };
        blitRegion.dstOffsets[0] = { 0, 0, 0 };
        blitRegion.dstOffsets[1] = { (int)swapchain_->width, (int)swapchain_->height, 1 };

        vkCmdBlitImage(cmd->commandBuffer, src->image, src->layout, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, filter);

        VkImageLayout swapchainImageLayout = _ConvertLayout(finalSwapchainImageLayout);

        VkImageMemoryBarrier renderEndBarrier = CreateImageBarrier(dstImage,
            imageAspect,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            0,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            swapchainImageLayout);
        vkCmdPipelineBarrier(cmd->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, pipelineStage, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderEndBarrier);
    }


    void VulkanGraphicsDevice::SubmitComputeLoad(CommandList* commandList)
    {
        while (vkGetFenceStatus(device_, mComputeFence_) != VK_SUCCESS);
        vkResetFences(device_, 1, &mComputeFence_);

        VulkanCommandList* cmdList = GetCommandList(commandList);
        VK_CHECK(vkEndCommandBuffer(cmdList->commandBuffer));

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.pWaitSemaphores = nullptr;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdList->commandBuffer;
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores = nullptr;
        vkQueueSubmit(queue_, 1, &submitInfo, mComputeFence_);
    }

    void VulkanGraphicsDevice::Present(CommandList* commandList)
    {
        VulkanCommandList* cmdList = GetCommandList(commandList);
        VkSemaphore signalSemaphore = mRenderCompleteSemaphore[currentFrame];
        VK_CHECK(vkEndCommandBuffer(cmdList->commandBuffer));

        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.waitSemaphoreCount = 1;

        VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        // Add a acquire semaphore brefore submitting the workload
        submitInfo.pWaitSemaphores = &mImageAcquireSemaphore; 
        submitInfo.pWaitDstStageMask = &waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdList->commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        // Also add a signal semaphore to signal the presentation queue
        // that the queue has been processed and ready to submit.
        submitInfo.pSignalSemaphores = &signalSemaphore; 

        // @TODO look into it
        // Might have caused the issue on the intel gpu
        // We wait for signal through the fence of current frame but wait 
        // for the fence of next frame before start drawing
        VkFence renderEndFence = mInFlightFences[currentFrame];
        vkQueueSubmit(queue_, 1, &submitInfo, renderEndFence);

        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.pImageIndices = &swapchain_->currentImageIndex;
        presentInfo.waitSemaphoreCount = 1;

        // Wait for the semaphore to signal before presenting
        presentInfo.pWaitSemaphores = &signalSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain_->swapchain;

        VK_CHECK(vkQueuePresentKHR(queue_, &presentInfo));

        destroyReleasedResources();

		currentFrame = (currentFrame + 1) % kMaxFrame;
    }

    void VulkanGraphicsDevice::CopyToBuffer(BufferHandle handle, void* data, uint32_t offset, uint32_t size)
    {
        VulkanBuffer* buffer = buffers.AccessResource(handle.handle);
        if (data == nullptr)
        {
            Logger::Error("Null data to copy to buffer");
            return;
        }

        if (offset > buffer->desc.size)
        {
            Logger::Error("Invalid offset and size (offset > size)");
            return;
        }

        if (size > buffer->desc.size)
        {
            Logger::Error("Copysize exceed the size of buffer");
            return;
        }

        auto device = GetDevice();

        void* ptr = buffer->mappedDataPtr;
        if (buffer->mappedDataPtr)
        {
            uint8_t* dstLoc = reinterpret_cast<uint8_t*>(ptr) + offset;
            std::memcpy(dstLoc, data, size);
        }
        else
        {
            GPUBufferDesc bufferDesc = {};
            bufferDesc.bindFlag = gfx::BindFlag::None;
            bufferDesc.usage = gfx::Usage::Upload;
            bufferDesc.size = size;
            BufferHandle stagingBuffer = CreateBuffer(&bufferDesc);

            VulkanBuffer* vkBuffer = buffers.AccessResource(stagingBuffer.handle);
            std::memcpy(vkBuffer->mappedDataPtr, data, size);
            CopyBuffer(handle, stagingBuffer, offset);
            Destroy(stagingBuffer);
        }
    }

    void VulkanGraphicsDevice::CopyBuffer(BufferHandle dst, BufferHandle src, uint32_t dstOffset)
    {
        VulkanBuffer* srcBuffer = buffers.AccessResource(src.handle);
        VulkanBuffer* dstBuffer = buffers.AccessResource(dst.handle);

        uint32_t copySize = static_cast<uint32_t>(std::min(dstBuffer->allocation->GetSize(), srcBuffer->allocation->GetSize()));
        if (dstBuffer->mappedDataPtr && srcBuffer->mappedDataPtr)
            std::memcpy(dstBuffer->mappedDataPtr, srcBuffer->mappedDataPtr, copySize);
        else 
        {

			VK_CHECK(vkResetCommandPool(device_, stagingCmdPool_, 0));
            VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			VK_CHECK(vkBeginCommandBuffer(stagingCmdBuffer_, &beginInfo));

            VkBufferCopy region = {0, dstOffset, VkDeviceSize(copySize)};
			vkCmdCopyBuffer(stagingCmdBuffer_, srcBuffer->buffer, dstBuffer->buffer, 1, &region);
            
			VK_CHECK(vkEndCommandBuffer(stagingCmdBuffer_));
            VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &stagingCmdBuffer_;
            VK_CHECK(vkQueueSubmit(queue_, 1, &submitInfo, VK_NULL_HANDLE));
            vkQueueWaitIdle(queue_);
        }
    }

    void VulkanGraphicsDevice::CopyTexture(TextureHandle dst, BufferHandle src, PipelineBarrierInfo* barriers, uint32_t arrayLevel, uint32_t mipLevel)
    {
        VulkanBuffer* from = buffers.AccessResource(src.handle);
        VulkanTexture* to = textures.AccessResource(dst.handle);

        assert(from != nullptr);
        assert(to != nullptr);

        VK_CHECK(vkResetCommandPool(device_, stagingCmdPool_, 0));
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(stagingCmdBuffer_, &beginInfo));

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = to->imageAspect;
        region.imageSubresource.baseArrayLayer = arrayLevel;
        region.imageSubresource.mipLevel = mipLevel;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = {to->width, to->height, to->depth};

        // Check Image Layout

        if (barriers)
        {
            assert(barriers->barrierInfoCount == 1);
            VkImageMemoryBarrier barrier = CreateImageBarrier(to->image,
				to->imageAspect,
                _ConvertAccessFlags(barriers->barrierInfo->srcAccessMask),
                _ConvertAccessFlags(barriers->barrierInfo->dstAccessMask),
                to->layout,
                _ConvertLayout(barriers->barrierInfo->newLayout),
                barriers->barrierInfo->baseMipLevel,
                barriers->barrierInfo->baseArrayLevel,
                barriers->barrierInfo->mipCount,
                barriers->barrierInfo->layerCount);

            vkCmdPipelineBarrier(stagingCmdBuffer_,
                _ConvertPipelineStageFlags(barriers->srcStage),
                _ConvertPipelineStageFlags(barriers->dstStage),
                0, 0, 0, 0, 0, 1, &barrier);
            to->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        }

        vkCmdCopyBufferToImage(stagingCmdBuffer_, from->buffer, to->image, to->layout, 1, &region);
        VK_CHECK(vkEndCommandBuffer(stagingCmdBuffer_));
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &stagingCmdBuffer_;
        VK_CHECK(vkQueueSubmit(queue_, 1, &submitInfo, VK_NULL_HANDLE));
        vkDeviceWaitIdle(device_);
    }

    void VulkanGraphicsDevice::CopyTexture(TextureHandle dst, void* src, uint32_t sizeInByte, uint32_t arrayLevel, uint32_t mipLevel, bool generateMipMap)
    {
        VulkanTexture* dstTexture = textures.AccessResource(dst.handle);

        // Create Staging buffer to transfer image data
        const uint32_t imageDataSize = dstTexture->width * dstTexture->height * dstTexture->sizePerPixelByte;

        gfx::GPUBufferDesc bufferDesc;
        bufferDesc.bindFlag = gfx::BindFlag::None;
        bufferDesc.usage = gfx::Usage::Upload;
        bufferDesc.size = imageDataSize;

        BufferHandle stagingBuffer = CreateBuffer(&bufferDesc);
        VulkanBuffer* buffer = buffers.AccessResource(stagingBuffer.handle);
        std::memcpy(buffer->mappedDataPtr, src, sizeInByte);

        // Copy from staging buffer to GPUTexture
        gfx::ImageBarrierInfo transferBarrier{ gfx::AccessFlag::None, gfx::AccessFlag::TransferWriteBit,gfx::ImageLayout::TransferDstOptimal };
        gfx::PipelineBarrierInfo transferBarrierInfo = {
            &transferBarrier, 1,
            gfx::PipelineStage::TransferBit,
            gfx::PipelineStage::TransferBit
        };
        CopyTexture(dst, stagingBuffer, &transferBarrierInfo, 0, 0);
        // If miplevels is greater than  1 the mip are generated
        // else the imagelayout is transitioned to shader attachment optimal
        if(generateMipMap)
			GenerateMipmap(dst, dstTexture->mipLevels);
        Destroy(stagingBuffer);
    }

    void VulkanGraphicsDevice::GenerateMipmap(TextureHandle src, uint32_t mipCount)
    {
        VK_CHECK(vkResetCommandPool(device_, stagingCmdPool_, 0));
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(stagingCmdBuffer_, &beginInfo));

        VulkanTexture* vkImage = textures.AccessResource(src.handle);
        if (mipCount > 1)
        {
            int width = vkImage->width;
            int height = vkImage->height;
            for (uint32_t i = 0; i < mipCount - 1; ++i)
            {
                VkImageMemoryBarrier barrierInfo = CreateImageBarrier(vkImage->image,
                    vkImage->imageAspect,
                    VK_ACCESS_NONE,
                    VK_ACCESS_TRANSFER_READ_BIT,
                    vkImage->layout,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, i, 0, 1);

                vkCmdPipelineBarrier(stagingCmdBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &barrierInfo);

                VkImageBlit blitRegion = {};
                blitRegion.srcSubresource.aspectMask = vkImage->imageAspect;
                blitRegion.srcSubresource.mipLevel = i;
                blitRegion.srcSubresource.layerCount = 1;

                blitRegion.dstSubresource.aspectMask = vkImage->imageAspect;
                blitRegion.dstSubresource.mipLevel = i + 1;
                blitRegion.dstSubresource.layerCount = 1;
                blitRegion.srcOffsets[0] = { 0, 0, 0 };
                blitRegion.srcOffsets[1] = { width, height, 1 };
                blitRegion.dstOffsets[0] = { 0, 0, 0 };
                blitRegion.dstOffsets[1] = { width >> 1, height >> 1, 1 };
                vkCmdBlitImage(stagingCmdBuffer_, vkImage->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vkImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, VK_FILTER_LINEAR);
                width = width >> 1;
                height = height >> 1;
            }

            VkImageMemoryBarrier barrierInfo[2] = {
                CreateImageBarrier(vkImage->image,
                vkImage->imageAspect,
                VK_ACCESS_NONE,
                VK_ACCESS_TRANSFER_READ_BIT,
                vkImage->layout,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mipCount - 1, 0, 1),

                CreateImageBarrier(vkImage->image,
                vkImage->imageAspect,
                VK_ACCESS_TRANSFER_READ_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			};

            vkCmdPipelineBarrier(stagingCmdBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &barrierInfo[0]);

            vkCmdPipelineBarrier(stagingCmdBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &barrierInfo[1]);

        }
        else
        {
            VkImageMemoryBarrier barrierInfo = CreateImageBarrier(vkImage->image,
                vkImage->imageAspect,
                VK_ACCESS_TRANSFER_READ_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                vkImage->layout,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            vkCmdPipelineBarrier(stagingCmdBuffer_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &barrierInfo);
        }

        vkImage->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VK_CHECK(vkEndCommandBuffer(stagingCmdBuffer_));
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &stagingCmdBuffer_;
        VK_CHECK(vkQueueSubmit(queue_, 1, &submitInfo, VK_NULL_HANDLE));
        vkDeviceWaitIdle(device_);
    }


    void VulkanGraphicsDevice::PipelineBarrier(CommandList* commandList, PipelineBarrierInfo* barriers)
    {
        std::vector<VkImageMemoryBarrier> imageBarriers;
        for (uint32_t i = 0; i < barriers->barrierInfoCount; ++i)
        {
            ImageBarrierInfo& barrierInfo = barriers->barrierInfo[i];

            VkImageLayout newLayout = _ConvertLayout(barrierInfo.newLayout);
            VulkanTexture* texture = textures.AccessResource(barrierInfo.resource.handle);

            // @NOTE: for now we don't care about access mask
			//if (texture->layout == newLayout)
            //    continue;

            imageBarriers.push_back(CreateImageBarrier(texture->image,
                texture->imageAspect,
                _ConvertAccessFlags(barrierInfo.srcAccessMask), 
                _ConvertAccessFlags(barrierInfo.dstAccessMask),
                texture->layout,
                newLayout,
                barrierInfo.baseMipLevel,
                barrierInfo.baseArrayLevel,
                barrierInfo.mipCount,
                barrierInfo.layerCount
               ));
            texture->layout = newLayout;
        }
        
        uint32_t barrierCount = (uint32_t)imageBarriers.size();
        if (barrierCount == 0)
            return;
        
		auto cmdList = GetCommandList(commandList);
        vkCmdPipelineBarrier(cmdList->commandBuffer,
            _ConvertPipelineStageFlags(barriers->srcStage),
            _ConvertPipelineStageFlags(barriers->dstStage),
            VK_DEPENDENCY_BY_REGION_BIT,
            0,0, 0, 0,
			barrierCount, imageBarriers.data());
    }

    void* VulkanGraphicsDevice::GetMappedDataPtr(BufferHandle buffer)
    {
        VulkanBuffer* vkBuffer = buffers.AccessResource(buffer.handle);
        if (vkBuffer)
            return vkBuffer->mappedDataPtr;
        return nullptr;
    }

    uint32_t VulkanGraphicsDevice::GetBufferSize(BufferHandle handle)
    {
        return (uint32_t)buffers.AccessResource(handle.handle)->desc.size;
    }

    void VulkanGraphicsDevice::UpdateDescriptor(PipelineHandle pipeline, DescriptorInfo* descriptorInfo, uint32_t descriptorInfoCount)
    {
        auto vkPipeline = pipelines.AccessResource(pipeline.handle);

        // lazy initialization of descriptor set
        std::vector<VulkanDescriptorInfo> descriptorInfos;
        for (uint32_t i = 0; i < descriptorInfoCount; ++i)
        {
            DescriptorInfo info = *(descriptorInfo + i);
            if (info.type == DescriptorType::StorageBuffer || info.type == DescriptorType::UniformBuffer)
            {
                auto vkBuffer = buffers.AccessResource(info.buffer.handle);
                VulkanDescriptorInfo descriptorInfo = {};
                descriptorInfo.bufferInfo.buffer = vkBuffer->buffer;
                descriptorInfo.bufferInfo.offset = info.offset;
                descriptorInfo.bufferInfo.range = info.size;
                descriptorInfos.push_back(descriptorInfo);
            }
            else
            {
                uint32_t totalSize = 1;
                uint32_t mipLevel = 0;
                if (info.type == DescriptorType::Image)
                    mipLevel = info.mipLevel;
                else if (info.type == DescriptorType::ImageArray)
                    totalSize = info.size;

                TextureHandle* inputTextures = (TextureHandle*)(info.texture);
                for (uint32_t imageIndex = 0; imageIndex < totalSize; ++imageIndex)
                {
                    TextureHandle texture = inputTextures[imageIndex];
                    VulkanDescriptorInfo descriptorInfo = {};
                    auto vkTexture = textures.AccessResource(texture.handle);
					descriptorInfo.imageInfo.imageLayout = vkTexture->layout;
					
					assert(vkTexture->imageViews.size() > mipLevel);
					descriptorInfo.imageInfo.imageView = vkTexture->imageViews[mipLevel];
					assert(vkTexture->sampler != VK_NULL_HANDLE);
					descriptorInfo.imageInfo.sampler = vkTexture->sampler;
                    descriptorInfos.push_back(descriptorInfo);
                }
            }
        }

        VkDescriptorPool descriptorPool = descriptorPools_[currentFrame];
		VkDescriptorSetAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocateInfo.descriptorPool = descriptorPool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts = &vkPipeline->setLayout;

		VkDescriptorSet set = 0;
		VK_CHECK(vkAllocateDescriptorSets(device_, &allocateInfo, &set));

        // This is just a reference to created descriptor set and used in 
        // subsequent rendering process
		vkPipeline->descriptorSet = set;
        vkUpdateDescriptorSetWithTemplate(device_, vkPipeline->descriptorSet, vkPipeline->updateTemplate,  descriptorInfos.data());
    }

    void VulkanGraphicsDevice::PushConstants(CommandList* commandList, PipelineHandle pipeline, ShaderStage shaderStages, void* value, uint32_t size, uint32_t offset)
    {
        auto cmd = GetCommandList(commandList);
        VulkanPipeline* vkPipeline = pipelines.AccessResource(pipeline.handle);

        VkShaderStageFlags shaderStageFlag = 0;
        if (HasFlag<>(shaderStages, ShaderStage::Vertex))
            shaderStageFlag |= VK_SHADER_STAGE_VERTEX_BIT;
        if (HasFlag<>(shaderStages, ShaderStage::Fragment))
            shaderStageFlag |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if (HasFlag<>(shaderStages, ShaderStage::Compute))
            shaderStageFlag |= VK_SHADER_STAGE_COMPUTE_BIT;
        if (HasFlag<>(shaderStages, ShaderStage::Geometry))
            shaderStageFlag |= VK_SHADER_STAGE_GEOMETRY_BIT;

        vkCmdPushConstants(cmd->commandBuffer, vkPipeline->pipelineLayout, shaderStageFlag, offset, size, value);
    }

    void VulkanGraphicsDevice::WaitForGPU()
    {
        VK_CHECK(vkDeviceWaitIdle(device_));
    }

    void VulkanGraphicsDevice::PrepareSwapchainForPresent(CommandList* commandList)
    {
        VkImageMemoryBarrier barrierInfo = CreateImageBarrier(swapchain_->images[swapchain_->currentImageIndex],
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_NONE,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        auto cmd = GetCommandList(commandList);
        vkCmdPipelineBarrier(cmd->commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &barrierInfo);
    }


    VkRenderPass VulkanGraphicsDevice::GetSwapchainRenderPass()
    {
        return swapchain_->renderPass->renderPass;
    }

    VkCommandBuffer VulkanGraphicsDevice::Get(CommandList* commandList)
    {
        auto cmdList = GetCommandList(commandList);
        return cmdList->commandBuffer;
    }

    void VulkanGraphicsDevice::Destroy(RenderPassHandle renderPass)
    {
        VulkanRenderPass* vkRp = renderPasses.AccessResource(renderPass.handle);
        if (vkRp == nullptr) return;

        gAllocationHandler.destroyedRenderPass_.push_back(vkRp->renderPass);
        renderPasses.ReleaseResource(renderPass.handle);
    }

    void VulkanGraphicsDevice::Destroy(PipelineHandle pipeline)
    {
        VulkanPipeline* vkPipeline = pipelines.AccessResource(pipeline.handle);
        if (vkPipeline == nullptr) return;

        gAllocationHandler.destroyedPipeline_.push_back(vkPipeline->pipeline);
        gAllocationHandler.destroyedPipelineLayout_.push_back(vkPipeline->pipelineLayout);
        gAllocationHandler.destroyedShaders_.insert(gAllocationHandler.destroyedShaders_.end(), vkPipeline->shaderModules.begin(), vkPipeline->shaderModules.end());
        gAllocationHandler.destroyedSetLayout_.push_back(vkPipeline->setLayout);
        gAllocationHandler.destroyedDescriptorUpdateTemplate_.push_back(vkPipeline->updateTemplate);
        vkPipeline->shaderModules.clear();
        pipelines.ReleaseResource(pipeline.handle);
    }

    void VulkanGraphicsDevice::Destroy(BufferHandle buffer)
    {
        VulkanBuffer* vkBuffer = buffers.AccessResource(buffer.handle);
        if (vkBuffer == nullptr) return;

		gAllocationHandler.destroyedBuffers_.push_back(std::make_pair(vkBuffer->buffer, vkBuffer->allocation));
		vkBuffer->desc = {};
		buffers.ReleaseResource(buffer.handle);
    }

    void VulkanGraphicsDevice::Destroy(TextureHandle texture)
    {
        VulkanTexture* vkTexture = textures.AccessResource(texture.handle);
        if (vkTexture == nullptr) return;

		gAllocationHandler.destroyedImages_.push_back(std::make_pair(vkTexture->image, vkTexture->allocation));
		gAllocationHandler.destroyedImageViews_.insert(gAllocationHandler.destroyedImageViews_.end(), vkTexture->imageViews.begin(), vkTexture->imageViews.end());
		gAllocationHandler.destroyedSamplers_.push_back(vkTexture->sampler);

		vkTexture->imageViews.clear();
		textures.ReleaseResource(texture.handle);
    }

    void VulkanGraphicsDevice::Destroy(FramebufferHandle framebuffer)
    {
        VulkanFramebuffer* vkFramebuffer = framebuffers.AccessResource(framebuffer.handle);
        if (vkFramebuffer == nullptr) return;

        gAllocationHandler.destroyedFramebuffers_.push_back(vkFramebuffer->framebuffer);
        framebuffers.ReleaseResource(framebuffer.handle);
    }
/*
    void VulkanGraphicsDevice::Destroy(SemaphoreHandle semaphore)
    {
        VulkanSemaphore* vkSemaphore = semaphores.AccessResource(semaphore.handle);
        if (vkSemaphore == nullptr) return;
        gAllocationHandler.destroyedSemaphore_.push_back(vkSemaphore->semaphore);
        semaphores.ReleaseResource(semaphore.handle);
    }
    */
    TextureHandle VulkanGraphicsDevice::GetFramebufferAttachment(FramebufferHandle handle, uint32_t index)
    {
        VulkanFramebuffer* framebuffer = framebuffers.AccessResource(handle.handle);
        if (index >= framebuffer->attachmentCount)
            return INVALID_TEXTURE;
        return framebuffer->attachments[index];
    }

    void VulkanGraphicsDevice::Shutdown()
    {
        WaitForGPU();
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

        gAllocationHandler.destroyedSemaphore_.push_back(mImageAcquireSemaphore);
        if (!supportTimelineSemaphore)
        {
            gAllocationHandler.destroyedFences_.push_back(mComputeFence_);
            for (uint32_t i = 0; i < kMaxFrame; ++i)
            {
                gAllocationHandler.destroyedSemaphore_.push_back(mRenderCompleteSemaphore[i]);
                gAllocationHandler.destroyedFences_.push_back(mInFlightFences[i]);
            }
        }
        else {
            gAllocationHandler.destroyedSemaphore_.push_back(mRenderSemaphore);
            gAllocationHandler.destroyedSemaphore_.push_back(mComputeSemaphore);
        }

        gAllocationHandler.destroyedFramebuffers_.insert(gAllocationHandler.destroyedFramebuffers_.end(), swapchain_->framebuffers.begin(), swapchain_->framebuffers.end());
        swapchain_->framebuffers.clear();

        gAllocationHandler.destroyedRenderPass_.push_back(swapchain_->renderPass->renderPass);
        renderPasses.ReleaseResource(mSwapchainRP.handle);

        // Release resources
        framebuffers.Shutdown();
        renderPasses.Shutdown();
        buffers.Shutdown();
        textures.Shutdown();
        pipelines.Shutdown();
		//semaphores.Shutdown();

        destroyReleasedResources();

        vmaDestroyAllocator(vmaAllocator_);

        for (auto& descriptorPool : descriptorPools_)
            vkDestroyDescriptorPool(device_, descriptorPool, nullptr);

        vkDestroyDescriptorSetLayout(device_, bindlessDescriptorLayout_, nullptr);
        vkDestroyDescriptorPool(device_, bindlessDescriptorPool_, nullptr);

        for (auto& queryPool : queryPools_)
            vkDestroyQueryPool(device_, queryPool, nullptr);

        for (uint32_t i = 0; i < swapchain_->imageCount; ++i)
        {
            vkFreeCommandBuffers(device_, commandPool_[i], 1, &commandBuffer_[i]);
            vkDestroyCommandPool(device_, commandPool_[i], nullptr);
        }
        delete[] commandPool_;
        delete[] commandBuffer_;

        vkFreeCommandBuffers(device_, stagingCmdPool_, 1, &stagingCmdBuffer_);
        vkDestroyCommandPool(device_, stagingCmdPool_, nullptr);
        vkDestroySwapchainKHR(device_, swapchain_->swapchain, nullptr);
        vkDestroySurfaceKHR(instance_, swapchain_->surface, nullptr);
        vkDestroyDevice(device_, nullptr);
        if (mValidationMode == ValidationMode::Enabled)
            vkDestroyDebugUtilsMessengerEXT(instance_, messenger_, nullptr);

        vkDestroyInstance(instance_, nullptr);

    }

    void VulkanGraphicsDevice::BeginDebugLabel(CommandList* commandList, const char* name, float r, float g, float b, float a)
    {
        if (mValidationMode == ValidationMode::Disabled || !debugMarkerEnabled_)
            return;

        VkDebugUtilsLabelEXT labelInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
        labelInfo.color[0] = r;
        labelInfo.color[1] = g;
        labelInfo.color[2] = b;
        labelInfo.color[3] = a;
        labelInfo.pLabelName = name;

        VulkanCommandList* cmdList = GetCommandList(commandList);
        vkCmdBeginDebugUtilsLabelEXT(cmdList->commandBuffer, &labelInfo);
    }

    void VulkanGraphicsDevice::EndDebugLabel(CommandList* commandList)
    {
        if (mValidationMode == ValidationMode::Disabled || !debugMarkerEnabled_)
            return;
        auto cmd = GetCommandList(commandList);
        vkCmdEndDebugUtilsLabelEXT(cmd->commandBuffer);
    }

    void VulkanGraphicsDevice::destroyReleasedResources()
    {
        gAllocationHandler.destroyReleasedResource(device_, vmaAllocator_);
    }
};