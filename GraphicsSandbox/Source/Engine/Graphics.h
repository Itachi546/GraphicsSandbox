#pragma once

#include <memory>
#include <string>
#include <vector>

#include "CommonInclude.h"
#include "Resource.h"

namespace gfx
{
	struct GPUResource;

	constexpr const uint32_t INVALID_TEXTURE_ID = 0xFFFFFFFF;

	enum class ValidationMode
	{
		Enabled,
		Disabled,
	};

	enum class QueueType
	{
		Graphics,
		Compute,
		Transfer
	};

	enum class Format
	{
		R10G10B10A2_UNORM = 0,
		B8G8R8A8_UNORM,
		R8G8B8A8_UNORM,
		R16_SFLOAT,
		R16G16_SFLOAT,
		R16B16G16_SFLOAT,
		R16B16G16A16_SFLOAT,
		R32B32G32A32_SFLOAT,
		R32G32_SFLOAT,
		D16_UNORM,
		D32_SFLOAT,
		D32_SFLOAT_S8_UINT,
		D24_UNORM_S8_UINT,
		FormatCount
	};

	enum class ImageType
	{
		I2D,
		I3D
	};

	enum class ImageViewType
	{
		IV2D,
		IV2DArray,
		IV3D,
		IVCubemap
	};

	enum class ImageAspect
	{
		Color, 
		Depth
	};

	enum class ImageLayout
	{
		General,
		ColorAttachmentOptimal,
		DepthStencilAttachmentOptimal,
		DepthAttachmentOptimal,
		TransferSrcOptimal,
		TransferDstOptimal,
		ShaderReadOptimal,
		PresentSrc
	};

	enum class ImageUsage
	{
		DepthStencilAttachment,
		ColorAttachment,
		ShaderResource
	};

	enum class TextureFilter
	{
		Linear,
		Nearest,
	};

	enum class TextureWrapMode
	{
		Repeat,
		ClampToEdge,
		ClampToBorder
	};

	enum class Topology
	{
		TriangleList,
		TriangleStrip,
		Line
	};

	enum class CullMode
	{
		Back,
		Front,
		None
	};

	enum class FrontFace
	{
		Clockwise,
		Anticlockwise
	};

	enum class PolygonMode
	{
		Fill,
		Line
	};

	enum class CompareOp
	{
		Less,
		Greater,
		Equal,
		LessOrEqual,
		GreaterOrEqual,
		Always
	};

	enum class ShaderStage
	{
		Vertex = 1,
		Fragment = 1 << 1,
		Compute  = 1 << 2,
		Geometry = 1 << 3
	};

	enum class BlendFactor
	{
		SrcAlpha,
		OneMinusSrcAlpha,
		SrcColor,
		OneMinusSrcColor,
		SrcColorOne,
		SrcAlphaOne
	};

	enum DescriptorType {
		StorageBuffer,
		UniformBuffer,
		Image,
		ImageArray
	};

	struct Viewport
	{
		float x, y, w, h, minDepth, maxDepth;
	};

	
	struct GraphicsDeviceResource {
		std::shared_ptr<void> internalState;
		bool IsValid() { return internalState != nullptr; }
	};

	struct BufferView
	{
		BufferHandle buffer;

		// Offset in byte to the parent buffer
		uint32_t byteOffset;

		// Count of underlying data
		uint32_t byteLength;
	};

/*
	struct SwapchainDesc
	{
		union
		{
			float val[4] = { 0.188f, 0.039f, 0.141f, 1.0f };
			struct {
				float r, g, b, a;
			};
		} clearColor;
		int width;
		int height;
		Format colorFormat = Format::R8G8B8A8_UNORM;
		Format depthFormat = Format::D32_SFLOAT_S8_UINT;
		bool fullscreen = false;
		bool vsync = false;
		bool enableDepth = false;
		RenderPassHandle renderPass = { K_INVALID_RESOURCE_HANDLE };
	};
	*/
	enum class AccessFlag
	{
		None,
		ShaderRead,
		ShaderWrite,
		ShaderReadWrite,
		DepthStencilRead,
		DepthStencilWrite,
		ColorAttachmentRead,
		ColorAttachmentWrite,
		TransferWriteBit,
		TransferReadBit,
		DrawCommandRead
	};

	enum class PipelineStage
	{
		TopOfPipe,
		FragmentShader,
		VertexShader,
		ComputeShader,
		EarlyFramentTest,
		LateFragmentTest,
		BottomOfPipe,
		Transfer,
		ColorAttachmentOutput,
		DrawIndirect
	};

	enum ResourceBarrierType {
		Texture,
		Buffer
	};

	struct ResourceBarrierInfo
	{
		AccessFlag srcAccessMask;
		AccessFlag dstAccessMask;
		ResourceBarrierType barrierType;
		union {
			struct {
				ImageLayout newLayout;
				TextureHandle texture;
				uint32_t baseMipLevel;
				uint32_t baseArrayLevel;
				uint32_t mipCount;
				uint32_t layerCount;
			} texture;
			struct {
				BufferHandle       buffer;
				uint32_t           offset;
				uint32_t           size;
			} buffer;
		} resourceInfo;

		static ResourceBarrierInfo CreateBufferBarrier(AccessFlag srcAccessMask, AccessFlag dstAccessMask, BufferHandle buffer, uint32_t offset, uint32_t size) {
			ResourceBarrierInfo info = {};
			info.srcAccessMask = srcAccessMask;
			info.dstAccessMask = dstAccessMask;
			info.resourceInfo.buffer.buffer = buffer;
			info.resourceInfo.buffer.size = size;
			info.resourceInfo.buffer.offset = offset;
			info.barrierType = ResourceBarrierType::Buffer;
			return info;
		}

		static ResourceBarrierInfo CreateImageBarrier(AccessFlag srcAccessMask, AccessFlag dstAccessMask, ImageLayout newLayout, TextureHandle texture, uint32_t baseMipLevel = 0, uint32_t baseArrayLevel = 0, uint32_t mipCount = ~0u, uint32_t layerCount = ~0u) {
			ResourceBarrierInfo info = {};
			info.srcAccessMask = srcAccessMask;
			info.dstAccessMask = dstAccessMask;
			info.resourceInfo.texture.newLayout = newLayout;
			info.resourceInfo.texture.texture = texture;
			info.resourceInfo.texture.baseArrayLevel = baseArrayLevel;
			info.resourceInfo.texture.baseMipLevel = baseMipLevel;
			info.resourceInfo.texture.mipCount = mipCount;
			info.resourceInfo.texture.layerCount = layerCount;
			info.barrierType = ResourceBarrierType::Texture;
			return info;
		}
	};

	struct BufferBarrierInfo {
		AccessFlag         srcAccessMask;
		AccessFlag         dstAccessMask;
		uint32_t           srcQueueFamilyIndex;
		uint32_t           dstQueueFamilyIndex;
	};

	struct PipelineBarrierInfo {
		ResourceBarrierInfo* barrierInfo;
		uint32_t barrierInfoCount;
		PipelineStage srcStage;
		PipelineStage dstStage;
	};

	struct ShaderDescription
	{
		const char* code;
		uint32_t sizeInByte;
	};

	struct RasterizationState
	{
		CullMode cullMode = CullMode::Back;
		FrontFace frontFace = FrontFace::Clockwise;
		PolygonMode polygonMode = PolygonMode::Fill;

		bool enableDepthTest = false;
		bool enableDepthWrite = false;
		bool enableDepthClamp = false;
		float lineWidth = 1.0f;
		float pointSize = 1.0f;
	};

	struct BlendState
	{
		bool enable = false;
		BlendFactor srcColor = BlendFactor::SrcAlpha;
		BlendFactor dstColor = BlendFactor::OneMinusSrcAlpha;
		BlendFactor srcAlpha = BlendFactor::SrcAlpha;
		BlendFactor dstAlpha = BlendFactor::OneMinusSrcAlpha;
	};
/*
	enum class SemaphoreType {
		Binary,
		Timeline
	};

	struct SemaphoreDesc
	{
		SemaphoreType type;
		uint64_t initialValue;
	};
	*/
	struct PipelineDesc
	{
		uint32_t pushConstantSize = 0;
		uint32_t shaderCount = 0;
		ShaderDescription* shaderDesc = nullptr;
		Topology topology = Topology::TriangleList;
		RasterizationState rasterizationState = {};
		BlendState* blendStates = nullptr;
		uint32_t blendStateCount = 0;
		RenderPassHandle renderPass = { K_INVALID_RESOURCE_HANDLE };
	};

	enum class Usage
	{
		Default, //CPU No access, GPU read/write
		Upload, // CPU Write, GPU Read
		ReadBack
	};
	
	enum class RenderPassOperation {
		DontCare,
		Load,
		Clear,
		Count
	};

	enum class BindFlag
	{
		None = 0, 
		VertexBuffer = 1 << 0,
		IndexBuffer = 1 << 1,
		ConstantBuffer = 1 << 2,
		ShaderResource = 1 << 3,
		RenderTarget = 1 << 4,
		DepthStencil = 1 << 5,
		IndirectBuffer = 1 << 6,
		StorageImage = 1 << 7
	};

	struct GPUBufferDesc
	{
		uint32_t size = 0;
		Usage usage = Usage::Default;
		BindFlag bindFlag = BindFlag::None;

	};

	struct SamplerInfo
	{
		TextureFilter textureFilter = TextureFilter::Linear;
		TextureWrapMode wrapMode = TextureWrapMode::ClampToEdge;
		bool enableAnisotropicFiltering = false;
		bool enableBorder = false;
	};

	struct GPUTextureDesc
	{
		SamplerInfo samplerInfo;
		bool bCreateSampler = false;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t depth = 1;
		uint32_t mipLevels = 1;
		uint32_t arrayLayers = 1;

		ImageType imageType = ImageType::I2D;
		BindFlag bindFlag = BindFlag::None;
		ImageAspect imageAspect = ImageAspect::Color;
		ImageViewType imageViewType = ImageViewType::IV2D;
		Format format = Format::R8G8B8A8_UNORM;
		bool bAddToBindless = true;
	};

	struct Attachment
	{
		Format format;
		RenderPassOperation operation;
		ImageAspect imageAspect;
	};


	struct RenderPassDesc
	{
		std::vector<Attachment> colorAttachments;
		Attachment depthAttachment;
		bool hasDepthAttachment = false;
	};


	struct FramebufferDesc {
		RenderPassHandle renderPass;
		std::vector<TextureHandle> outputTextures;

		bool hasDepthStencilAttachment = false;
		TextureHandle depthStencilAttachment;

		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t layers = 1;

		float scalingX = 1.0f;
		float scalingY = 1.0f;
	};

	/*
	struct RenderPass : public GraphicsDeviceResource
	{
		RenderPassDesc desc;
	};*/

	/*
	struct GPUTexture : public GPUResource
	{
		std::string name;
		GPUTextureDesc desc;
	};
	*/

	struct DescriptorInfo
	{
		union {
			BufferHandle buffer;
			// @TODO Refactor this
			TextureHandle* texture;
		};

		uint32_t offset = 0;
		union {
			uint32_t size = 0;
			uint32_t mipLevel;
		};
		DescriptorType type = DescriptorType::StorageBuffer;
		uint32_t totalSize = 64;

		DescriptorInfo() = default;

		DescriptorInfo(BufferHandle handle, uint32_t offset, uint32_t size, DescriptorType type, uint32_t totalSize = 64) : 
			buffer(handle),
			offset(offset),
			size(size),
			type(type),
			totalSize(totalSize)
		{

		}

		DescriptorInfo(TextureHandle* texture, uint32_t offset, uint32_t mipLevel, DescriptorType type, uint32_t totalSize = 64) :
			texture(texture),
			offset(offset),
			mipLevel(mipLevel),
			type(type),
			totalSize(totalSize)
		{

		}

	};

	struct MeshDrawIndirectCommand {
		uint32_t    indexCount;
		uint32_t    instanceCount;
		uint32_t    firstIndex;

		int32_t     vertexOffset;
		uint32_t    firstInstance;
		uint32_t    taskCount;
		uint32_t    firstTask;
		uint32_t    drawId;
	};
	static_assert(sizeof(MeshDrawIndirectCommand) % 16 == 0);

	struct DrawIndirectCommand
	{
		uint32_t    indexCount;
		uint32_t    instanceCount;
		uint32_t    firstIndex;
		int32_t     vertexOffset;
		uint32_t    firstInstance;
	};


	enum class QueryType
	{
		TimeStamp,
	};

	struct QueryPool : public GraphicsDeviceResource
	{
		uint32_t queryPoolCount = 0;
		QueryType queryType;
	};

	struct CommandList
	{
		void* internalState = nullptr;
		constexpr bool IsValid() const { return internalState != nullptr; }
	};

	template<>
	struct enable_bitwise_operation<BindFlag>
	{
		static constexpr bool enabled = true;
	};

	template<>
	struct enable_bitwise_operation<ImageUsage>
	{
		static constexpr bool enabled = true;
	};

	template<>
	struct enable_bitwise_operation<ShaderStage>
	{
		static constexpr bool enabled = true;
	};

}