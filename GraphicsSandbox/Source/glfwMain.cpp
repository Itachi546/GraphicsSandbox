#include "Engine/GraphicsSandbox.h"
#include <GLFW/glfw3.h>

int main(int argc, char** argv)
{
	Application application;

	if (!glfwInit())
		glfwInit();
	/*
	* Setting up this window hint helps to prevent error
	* that return VK_OUT_OF_DEVICE_MEMORY_KHR when creating
	* swapchain in some case.
	*/
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(1360, 769, "Hello World", 0, 0);
	glfwSwapInterval(1);
	application.SetWindow(window, false);
	application.Initialize();

	while (!glfwWindowShouldClose(window) && application.IsRunning())
	{
		glm::vec2 delta = Input::GetMouseState().delta;

		application.Run();

		glfwSwapBuffers(window);
	}

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}