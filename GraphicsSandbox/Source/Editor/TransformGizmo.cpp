#include "TransformGizmo.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace TransformGizmo
{
	struct Context {
		bool bUsing = false;
		bool bEnable = false;

		float translationLineThickness = 3.0f;
		ImDrawList* mDrawList;

		glm::mat4 V;
		glm::mat4 P;
		glm::mat4 VP;

		uint32_t width, height;

	};

	const ImVec4 gAxisColor[] = {
		ImVec4(0.666f, 0.000f, 0.000f, 1.0f),
		ImVec4(0.000f, 0.666f, 0.000f, 1.0f),
		ImVec4(0.000f, 0.000f, 0.666f, 1.0f)
	};

	const glm::vec3 gUnitDirection[] = {
		glm::vec3(1.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, 1.0f),
	};

	Context gContext;
	static ImU32 GetColorU32(ImVec4 color)
	{
		return ImGui::ColorConvertFloat4ToU32(color);
	}

	static float GetLengthClipSpace(glm::vec3 start, glm::vec3 end)
	{
		glm::vec4 ndcCoord = gContext.VP * glm::vec4(start, 1.0f);
		glm::vec3 clipStart = glm::vec3(ndcCoord);
		if (fabs(ndcCoord.w) > FLT_EPSILON)
			clipStart /= ndcCoord.w;

		ndcCoord = gContext.VP * glm::vec4(end, 1.0f);
		glm::vec3 clipEnd = glm::vec3(ndcCoord);
		if (fabs(ndcCoord.w) > FLT_EPSILON)
			clipEnd /= ndcCoord.w;

		return glm::length(clipEnd - clipStart);
	}

	static void ComputeTripodAxis(const int axisIndex, glm::vec3& dirAxis)
	{
		dirAxis = gUnitDirection[axisIndex];
	}

	glm::vec2 ComputeScreenSpacePos(const glm::vec3& p)
	{
		// Calculate NDC Coordinate
		glm::vec4 ndc = gContext.VP * glm::vec4(p, 1.0f);
		ndc /= ndc.w;

		// Convert in range 0-1
		ndc = ndc * 0.5f + 0.5f;
		// Invert Y-Axis 
		ndc.y = 1.0f - ndc.y;
		return glm::vec2(ndc.x * gContext.width, ndc.y * gContext.height);
	}

	void AddLine(glm::vec2 start, glm::vec2 end)
	{
		gContext.mDrawList->AddLine(ImVec2(start.x, start.y), ImVec2(end.x, end.y), GetColorU32(gAxisColor[0]), 3.0f);
	}

		
	static void DrawTranslationGizmo(Operation op)
	{
		ImDrawList* drawList = gContext.mDrawList;

		if (drawList == nullptr)
			return;

		if (op != Operation::Translate)
			return;

		const float screenFactor = 1.0f;

		// Calculate axis position
		for (int i = 0; i < 3; ++i)
		{
			glm::vec3 axis;
			ComputeTripodAxis(i, axis);

			glm::vec2 baseScreenSpace = ComputeScreenSpacePos(axis * 0.1f * screenFactor);
			glm::vec2 dirScreenSpace = ComputeScreenSpacePos(axis * screenFactor);
			drawList->AddLine(ImVec2(baseScreenSpace.x, baseScreenSpace.y), ImVec2(dirScreenSpace.x, dirScreenSpace.y), GetColorU32(gAxisColor[i]), gContext.translationLineThickness);

			// Calculate the Arrow Head
			glm::vec2 dir = dirScreenSpace - baseScreenSpace;
			dir = glm::normalize(dir) * 5.0f;

			glm::vec2 orthoDir(dir.y, -dir.x);
			glm::vec2 p0 = dirScreenSpace + dir * 2.0f;
			glm::vec2 p1 = dirScreenSpace + orthoDir;
			glm::vec2 p2 = dirScreenSpace - orthoDir;
			drawList->AddTriangleFilled(ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), GetColorU32(gAxisColor[i]));
		}

		glm::vec2 screenCenter = ComputeScreenSpacePos(glm::vec3(0.0f));
		drawList->AddCircleFilled(ImVec2(screenCenter.x, screenCenter.y), 6.0f, IM_COL32_WHITE);
	}

	void BeginFrame()
	{
		const ImU32 flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGuiIO& io = ImGui::GetIO();
		gContext.width = (uint32_t)io.DisplaySize.x;
		gContext.height = (uint32_t)io.DisplaySize.y;

		ImGui::SetWindowSize(io.DisplaySize);
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
		ImGui::PushStyleColor(ImGuiCol_Border, 0);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

		ImGui::Begin("Gizmo", NULL, flags);
		gContext.mDrawList = ImGui::GetWindowDrawList();
		ImGui::End();

		ImGui::PopStyleVar();
		ImGui::PopStyleColor(2);
	}

	void Manipulate(const glm::mat4& view, const glm::mat4& projection, Operation operation, glm::mat4& matrix)
	{
		gContext.V = view;
		gContext.P = projection;
		gContext.VP = projection * view;
		DrawTranslationGizmo(operation);
	}
}
