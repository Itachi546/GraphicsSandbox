#pragma once

namespace gfx {
	static constexpr uint32_t K_INVALID_RESOURCE_HANDLE = 0xffffffff;
	typedef unsigned int ResourceHandle;

	struct RenderPassHandle {
		ResourceHandle handle;
	};

	struct BufferHandle {
		ResourceHandle handle;
	};

	struct TextureHandle {
		ResourceHandle handle;
	};

	struct PipelineHandle {
		ResourceHandle handle;
	};

	struct FramebufferHandle {
		ResourceHandle handle;
	};

	struct DescriptorSetHandle {
		ResourceHandle handle;
	};

	struct SemaphoreHandle {
		ResourceHandle handle;
	};

	static RenderPassHandle INVALID_RENDERPASS = { K_INVALID_RESOURCE_HANDLE };
	static TextureHandle INVALID_TEXTURE = { K_INVALID_RESOURCE_HANDLE };
	static BufferHandle INVALID_BUFFER = { K_INVALID_RESOURCE_HANDLE };
	static PipelineHandle INVALID_PIPELINE = { K_INVALID_RESOURCE_HANDLE };
	static FramebufferHandle INVALID_FRAMEBUFFER = { K_INVALID_RESOURCE_HANDLE };
	static DescriptorSetHandle INVALID_DESCRIPTOR_SET = { K_INVALID_RESOURCE_HANDLE };
	static SemaphoreHandle INVALID_SEMAPHORE = { K_INVALID_RESOURCE_HANDLE };
}