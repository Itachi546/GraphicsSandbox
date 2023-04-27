#include "AsynchronousLoader.h"
#include "Timer.h"
#include "Logger.h"
#include "ImageLoader.h"

void AsynchronousLoader::Initialize(gfx::GraphicsDevice* device, enki::TaskScheduler* taskSchedular)
{
	mDevice = device;
	mScheduler = taskSchedular;
	mFileLoadRequest.reserve(16);
	mUploadRequest.reserve(16);

	gfx::GPUBufferDesc bufferDesc;
	bufferDesc.bindFlag = gfx::BindFlag::None;
	bufferDesc.usage = gfx::Usage::Upload;
	bufferDesc.size = kSize;

	mStagingBuffer = mDevice->CreateBuffer(&bufferDesc);

	mTransferFence = mDevice->CreateFence();
	mStagingBufferOffset = 0;
}

void AsynchronousLoader::Update()
{
	// Process upload request
	if (mUploadRequest.size() > 0)
	{
		// Wait for upload to finish
		if (!mDevice->IsFenceSignalled(mTransferFence))
			return;

		// Check if last texture was uploaded to gpu
		if (mTextureReady.handle != gfx::K_INVALID_RESOURCE_HANDLE)
			mDevice->AddTextureToUpdate(mTextureReady);
		mTextureReady = gfx::INVALID_TEXTURE;

		// Reset the fences
		mDevice->ResetFence(mTransferFence);

		UploadRequest uploadRequest = mUploadRequest.back();
		mUploadRequest.pop_back();

		if (uploadRequest.texture.handle != gfx::K_INVALID_RESOURCE_HANDLE)
		{
			// Process texture upload
			uint32_t dataSize = uploadRequest.sizeInBytes;
			uint32_t bufferOffset = static_cast<uint32_t>(std::atomic_fetch_add(&mStagingBufferOffset, dataSize));

			mDevice->CopyToCPUBuffer(mStagingBuffer, uploadRequest.data, bufferOffset, dataSize);
			mDevice->CopyTexture(uploadRequest.texture, mStagingBuffer, bufferOffset, mTransferFence);
			mTextureReady = uploadRequest.texture;
		}
	}

	// Process File Load Request
	if (mFileLoadRequest.size() > 0)
	{
		FileLoadRequest loadRequest = mFileLoadRequest.back();
		mFileLoadRequest.pop_back();

		// Process request
		Timer timer;
		timer.record();
		int width, height, nChannel;
		std::string& filename = loadRequest.path;
		uint8_t* pixels = Utils::ImageLoader::Load(filename.c_str(), width, height, nChannel);
		if (pixels == nullptr)
		{
			Logger::Warn("Failed to load texture: " + filename);
		}
		else {
			Logger::Info("Processing Load request " + filename);
			UploadRequest uploadRequest;
			uploadRequest.data = pixels;
			uploadRequest.sizeInBytes = width * height * nChannel * sizeof(uint8_t);
			uploadRequest.texture = loadRequest.texture;
			uploadRequest.cpuBuffer = gfx::INVALID_BUFFER;
			mUploadRequest.push_back(uploadRequest);
		}
		Logger::Info("Reading file: " + filename + " took " + std::to_string(timer.elapsedSeconds()) + 's');
	}

	mStagingBufferOffset = 0;
}

void AsynchronousLoader::RequestTextureCopy(void* data, uint32_t sizeInBytes, gfx::TextureHandle texture)
{
	UploadRequest uploadRequest;
	uploadRequest.sizeInBytes = sizeInBytes;
	uploadRequest.data = data;
	uploadRequest.cpuBuffer = gfx::INVALID_BUFFER;
	uploadRequest.texture = texture;
	uploadRequest.gpuBuffer = gfx::INVALID_BUFFER;
}

void AsynchronousLoader::RequestBufferUpload(void* data, uint32_t sizeInByte, gfx::BufferHandle buffer)
{
	UploadRequest uploadRequest;
	uploadRequest.sizeInBytes = sizeInByte;
	uploadRequest.data = data;
	uploadRequest.cpuBuffer = buffer;
	uploadRequest.texture = gfx::INVALID_TEXTURE;
	uploadRequest.gpuBuffer = gfx::INVALID_BUFFER;
}


void AsynchronousLoader::RequestTextureData(const std::string& filename, gfx::TextureHandle textureHandle)
{
	FileLoadRequest request;
	request.texture = textureHandle;
	request.path = filename;
	request.buffer = gfx::INVALID_BUFFER;
	mFileLoadRequest.push_back(request);
}


void AsynchronousLoader::RequestBufferCopy(gfx::BufferHandle src, gfx::BufferHandle dst)
{
	UploadRequest uploadRequest;
	uploadRequest.data = nullptr;
	uploadRequest.cpuBuffer = src;
	uploadRequest.texture = gfx::INVALID_TEXTURE;
	uploadRequest.gpuBuffer = dst;
}

void AsynchronousLoader::Shutdown()
{
	mDevice->Destroy(mStagingBuffer);
	mDevice->Destroy(mTransferFence);
}
