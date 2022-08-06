#pragma once

#include <memory>
#include <string>
#include "CommonInclude.h"

namespace gfx
{
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
		D32_SFLOAT,
		D32_SFLOAT_S8_UINT,
		D24_UNORM_S8_UINT,
		FormatCount
	};

	enum class ImageType
	{
		I2D,
		I3D,
		I2DArray
	};

	enum class ImageLayout
	{
		ColorAttachmentOptimal,
		DepthStencilAttachmentOptimal,
		DepthAttachmentOptimal
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
		Vertex,
		Fragment,
		Compute,
		Geometry
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
		Image
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

	struct Attachment
	{
		uint32_t index;
		Format format;
		ImageLayout layout;
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
		BlendState blendState = {};
		RenderPass* renderPass = nullptr;
	};

	struct GPUResource : public GraphicsDeviceResource
	{
		enum class Type
		{
			Buffer,
			Texture,
			Unknown
		} type = Type::Unknown;

		constexpr bool IsTexture() const { return type == Type::Texture; }
		constexpr bool IsBuffer() const { return type == Type::Buffer; }

		void* mappedDataPtr;
		std::size_t mappedDataSize;
	};

	struct DescriptorInfo
	{
		GPUResource* resource = nullptr;
		uint32_t offset = 0;
		uint32_t size = 0;
		DescriptorType type = DescriptorType::StorageBuffer;
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
		IndirectBuffer = 1 << 6
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

	struct DrawIndirectCommand
	{
		uint32_t    indexCount;
		uint32_t    instanceCount;
		uint32_t    firstIndex;
		int32_t     vertexOffset;
		uint32_t    firstInstance;
	};


	template<>
	struct enable_bitwise_operation<BindFlag>
	{
		static constexpr bool enabled = true;
	};
}