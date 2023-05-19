#include "ImGuiService.h"

#include "ImGui/imgui_impl_glfw.h"
#include "ImGui/imgui_impl_vulkan.h"
#include "ImPlot/implot.h"
#include "ImGui/IconsFontAwesome5.h"

#include "../Profiler.h"
#include "../VulkanGraphicsDevice.h"

#include <iomanip>
#include <sstream>
#include <string>

void ImGuiService::Init(Platform::WindowType window, std::shared_ptr<gfx::GraphicsDevice> device_)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;

	io.Fonts->AddFontDefault();
	static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
	ImFontConfig iconConfig;
	iconConfig.MergeMode = true;
	iconConfig.PixelSnapH = true;
	iconConfig.GlyphMinAdvanceX = 13.0f;
	std::string filename = "Assets/Fonts/" + std::string(FONT_ICON_FILE_NAME_FAS);
	io.Fonts->AddFontFromFileTTF(filename.c_str(), 13.0f, &iconConfig, icons_ranges);
	// use FONT_ICON_FILE_NAME_FAR if you want regular instead of solid

	ImGui_ImplGlfw_InitForVulkan(window, true);

	device = std::static_pointer_cast <gfx::VulkanGraphicsDevice>(device_);

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

bool ImGuiService::IsAcceptingEvent()
{
	return ImGui::IsAnyItemActive() || ImGui::IsAnyItemHovered();
}

void ImGuiService::Shutdown()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();
}
