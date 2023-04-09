#pragma once

#include "enkiTS/TaskScheduler.h"
#include "GraphicsDevice.h"
#include "Logger.h"

struct FileLoadRequest {
	std::string path;
	gfx::TextureHandle texture = gfx::INVALID_TEXTURE;
	gfx::BufferHandle buffer = gfx::INVALID_BUFFER;
};

struct UploadRequest {
	void* data = nullptr;
	uint32_t sizeInBytes;
	gfx::TextureHandle texture = gfx::INVALID_TEXTURE;
	gfx::BufferHandle buffer = gfx::INVALID_BUFFER;
};

class AsynchronousLoader
{
public:
	void Initialize(gfx::GraphicsDevice* device, enki::TaskScheduler* taskScheduler);

	void Update();

	void RequestTextureData(const std::string& filename, gfx::TextureHandle textureHandle);

	void Shutdown();
private:
	gfx::GraphicsDevice* mDevice;
	enki::TaskScheduler* mScheduler;

	gfx::FenceHandle mTransferFence;

	std::vector<FileLoadRequest> mFileLoadRequest;
	std::vector<UploadRequest> mUploadRequest;
	
	gfx::BufferHandle mStagingBuffer;
	gfx::TextureHandle mTextureReady = gfx::INVALID_TEXTURE;

	// 64 MB 
	const uint32_t kSize = 64 * 1024 * 1024;

	std::atomic_size_t mStagingBufferOffset;
};


struct AsynchronousLoadTask : enki::IPinnedTask
{
	void Execute() override {
		while (execute)
		{
			asyncLoader->Update();
		}
	}

	AsynchronousLoader* asyncLoader;
	enki::TaskScheduler* scheduler;
	bool execute = true;
};

