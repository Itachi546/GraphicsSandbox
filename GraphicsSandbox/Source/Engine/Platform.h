#pragma once

#ifdef _WIN32
#define PLATFORM_WINDOW
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <string>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#else
#endif

#if defined(GLFW_WINDOW)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#endif

struct WindowProperties
{
	int width = 0;
	int height = 0;
};

namespace Platform
{
#ifdef GLFW_WINDOW
	using WindowType = GLFWwindow *;
#else
	using WindowType = void *;
#endif

	inline void GetWindowProperties(WindowType window, WindowProperties *properties)
	{
#ifdef GLFW_WINDOW
		glfwGetFramebufferSize(window, &properties->width, &properties->height);
#endif
	}

	inline void SetWindowTitle(WindowType window, const char *title)
	{
#ifdef GLFW_WINDOW
		glfwSetWindowTitle(window, title);
#endif
	}

#ifdef PLATFORM_WINDOW
	inline HWND GetWindowHandle(WindowType window)
	{
		return glfwGetWin32Window(window);
	}

	inline std::string WStringToString(const std::wstring &wstr)
	{
		std::string str;
		size_t size;
		str.resize(wstr.length());
		wcstombs_s(&size, &str[0], str.size() + 1, wstr.c_str(), wstr.size());
		return str;
	}

#endif
};

#define EXIT_ERROR -1