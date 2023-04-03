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
}

void AsynchronousLoader::Update()
{

	// Process upload request
	if (mUploadRequest.size() > 0)
	{
		// Wait for upload to finish
		if (!mDevice->IsFenceSignalled(mTransferFence))
			return;

		// Reset the fences
		mDevice->ResetFence(mTransferFence);

		UploadRequest uploadRequest = mUploadRequest.back();
		mUploadRequest.pop_back();

		if (uploadRequest.texture.handle != gfx::K_INVALID_RESOURCE_HANDLE)
		{
			// Process texture upload
			mDevice->CopyTexture(uploadRequest.texture, uploadRequest.data, uploadRequest.sizeInBytes, mTransferFence);
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
			UploadRequest uploadRequest;
			uploadRequest.data = pixels;
			uploadRequest.sizeInBytes = width * height * nChannel * sizeof(uint8_t);
			uploadRequest.texture = loadRequest.texture;
			uploadRequest.buffer = gfx::INVALID_BUFFER;
			mUploadRequest.push_back(uploadRequest);
		}
		Logger::Info("Reading file: " + filename + " took " + std::to_string(timer.elapsedSeconds()) + 's');
	}
}

void AsynchronousLoader::RequestTextureData(const std::string& filename, gfx::TextureHandle textureHandle)
{
	FileLoadRequest request;
	request.texture = textureHandle;
	request.path = filename;
	request.buffer = gfx::INVALID_BUFFER;
	mFileLoadRequest.push_back(request);
}

void AsynchronousLoader::Shutdown()
{
	mDevice->Destroy(mStagingBuffer);
	mDevice->Destroy(mTransferFence);
}
