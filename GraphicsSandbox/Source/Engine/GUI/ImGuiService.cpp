#include "ImGuiService.h"

#include <imgui/imgui_impl_glfw.h>

#include <imgui/imgui_impl_vulkan.h>
#include "IconsFontAwesome6.h"

#include "../Profiler.h"
#include "../VulkanGraphicsDevice.h"

#include <iomanip>
#include <sstream>
#include <string>
namespace ui {

	std::unordered_map<uint32_t, VkDescriptorSet> ImTextureIDMap;

	void ImGuiService::Init(Platform::WindowType window, std::shared_ptr<gfx::GraphicsDevice> device_)
	{
		device = std::static_pointer_cast <gfx::VulkanGraphicsDevice>(device_);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO(); (void)io;

		VkInstance instance = device->GetInstance();
		ImGui_ImplVulkan_LoadFunctions([](const char* function_name, void* vulkan_instance) {
			return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance*>(vulkan_instance)), function_name);
			}, &instance);
		ImGui_ImplGlfw_InitForVulkan(window, true);

		ImGui_ImplVulkan_InitInfo initInfo = {};
		initInfo.Instance = device->GetInstance();
		initInfo.PhysicalDevice = device->GetPhysicalDevice();
		initInfo.Device = device->GetDevice();
		initInfo.Queue = device->GetQueue();
		initInfo.MinImageCount = device->GetSwapchainImageCount();
		initInfo.ImageCount = device->GetSwapchainImageCount();
		initInfo.DescriptorPool = device->GetDescriptorPool();
		initInfo.Allocator = 0;

		ImGui_ImplVulkan_Init(&initInfo, device->GetSwapchainRenderPass());

		gfx::CommandList commandList = device->BeginCommandList();
		ImGui_ImplVulkan_CreateFontsTexture(device->Get(&commandList));
		// @TODO placeholder
		device->SubmitComputeLoad(&commandList);
		device->WaitForGPU();
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}

	void ImGuiService::NewFrame()
	{
		if (!enabled) return;
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiService::Render(gfx::CommandList* commandList)
	{
		if (!enabled) return;

		ImGui::SetNextWindowPos(ImVec2(10, 10));
		// Profilter Data
		//ImGui::Begin("Statistics", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Begin("Statistics");
		std::vector<Profiler::RangeData> profilerData;
		Profiler::GetEntries(profilerData);
		if (profilerData.size() > 0)
		{
			std::stringstream ss;
			for (auto& data : profilerData)
			{
				if (data.inUse)
					ss << data.name << " " << std::setprecision(3) << data.time << "ms\n";
			}
			ImGui::Text("%s", ss.str().c_str());
		}
		ImGui::End();

		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), device->Get(commandList));
	}

	void ImGuiService::AddImage(gfx::TextureHandle texture, const ImVec2& size)
	{
		gfx::VulkanGraphicsDevice* vkDevice = (gfx::VulkanGraphicsDevice*)gfx::GetDevice();
		auto found = ImTextureIDMap.find(texture.handle);

		ImTextureID textureId = nullptr;
		if (found == ImTextureIDMap.end()) {
			VulkanTexture* vkTexture = vkDevice->textures.AccessResource(texture.handle);
			VkDescriptorSet descriptorSet = ImGui_ImplVulkan_AddTexture(vkTexture->sampler, vkTexture->imageViews[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			ImTextureIDMap[texture.handle] = descriptorSet;
			textureId = (ImTextureID)descriptorSet;
		}
		else textureId = (ImTextureID)found->second;

		ImGui::ImageButton(textureId, size, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
	}

	bool ImGuiService::IsAcceptingEvent()
	{
		return ImGui::IsAnyItemActive() || ImGui::IsAnyItemHovered();
	}

	void ImGuiService::Shutdown()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
}