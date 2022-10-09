#pragma once

#include <memory>
#include <string>
#include <vector>

#include "CommonInclude.h"

namespace gfx
{
	struct GPUResource;

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

	struct RenderPass;
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
		RenderPass* renderPass = nullptr;
	};



	struct Semaphore : public GraphicsDeviceResource
	{

	};

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
		TransferReadBit
	};

	enum class PipelineStage
	{
		TopOfPipe,
		FragmentShader,
		VertexShader,
		ComputeShader,
		EarlyFramentTest,
		LateFramentTest,
		BottomOfPipe,
		TransferBit,
		ColorAttachmentOutput
	};

	struct ImageBarrierInfo
	{
		AccessFlag srcAccessMask;
		AccessFlag dstAccessMask;
		ImageLayout newLayout;
		GPUResource* resource;
		uint32_t baseMipLevel = 0;
		uint32_t baseArrayLevel = 0;
		uint32_t mipCount = ~0u;
		uint32_t layerCount = ~0u;
	};

	struct PipelineBarrierInfo {
		ImageBarrierInfo* barrierInfo;
		uint32_t barrierInfoCount;
		PipelineStage srcStage;
		PipelineStage dstStage;
	};

	struct Pipeline : public GraphicsDeviceResource
	{
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
		BlendFactor srcColor = BlendFactor::SrcColor;
		BlendFactor dstColor = BlendFactor::OneMinusSrcColor;
		BlendFactor srcAlpha = BlendFactor::SrcAlpha;
		BlendFactor dstAlpha = BlendFactor::OneMinusSrcAlpha;

	};


	struct PipelineDesc
	{
		uint32_t pushConstantSize = 0;
		uint32_t shaderCount = 0;
		ShaderDescription* shaderDesc = nullptr;
		Topology topology = Topology::TriangleList;
		RasterizationState rasterizationState = {};
		BlendState* blendState = nullptr;
		uint32_t blendStateCount = 0;
		RenderPass* renderPass = nullptr;
	};

	struct GPUResource : public GraphicsDeviceResource
	{
		enum class Type
		{
			Buffer,
			Texture,
			Unknown
		} resourceType = Type::Unknown;

		constexpr bool IsTexture() const { return resourceType == Type::Texture; }
		constexpr bool IsBuffer() const { return resourceType == Type::Buffer; }

		void* mappedDataPtr;
		std::size_t mappedDataSize;
	};

	enum class Usage
	{
		Default, //CPU No access, GPU read/write
		Upload, // CPU Write, GPU Read
		ReadBack
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

	struct GPUBuffer : public GPUResource
	{
		GPUBufferDesc desc;

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
	};

	struct Attachment
	{
		uint32_t index;
		GPUTextureDesc desc;
	};


	struct RenderPassDesc
	{
		uint32_t attachmentCount;
		Attachment* attachments;

		uint32_t width;
		uint32_t height;
		bool hasDepthAttachment = false;
	};

	struct RenderPass : public GraphicsDeviceResource
	{
		RenderPassDesc desc;
	};

	struct GPUTexture : public GPUResource
	{
		GPUTextureDesc desc;
	};

	struct DescriptorInfo
	{
		GPUResource* resource;
		uint32_t offset = 0;
		union {
			uint32_t size = 0;
			uint32_t mipLevel;
		};
		DescriptorType type = DescriptorType::StorageBuffer;
	};

	struct Framebuffer : public GraphicsDeviceResource
	{
		std::vector<GPUTexture> attachments;
		uint32_t depthAttachmentIndex = ~0u;
	};

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