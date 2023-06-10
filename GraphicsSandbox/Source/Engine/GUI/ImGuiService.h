#pragma once

#include "ImGui/imgui.h"
#include "../Platform.h"

#include <memory>

namespace gfx {
	class GraphicsDevice;
	class VulkanGraphicsDevice;
	struct CommandList;
}

namespace ui {
	struct ImGuiService {

		static ImGuiService* GetInstance()
		{
			static ImGuiService* instance = new ImGuiService();
			return instance;
		}

		void Init(Platform::WindowType window, std::shared_ptr<gfx::GraphicsDevice> device);

		void NewFrame();

		void Render(gfx::CommandList* commandList);

		bool IsAcceptingEvent();

		void Shutdown();

		std::shared_ptr<gfx::VulkanGraphicsDevice> device;
		bool enabled = true;
	};
};