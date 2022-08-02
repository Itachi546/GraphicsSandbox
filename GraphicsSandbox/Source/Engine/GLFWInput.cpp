#include "GLFWInput.h"
#include "EventDispatcher.h"

Input::KeyboardState gKeyboard;
Input::MouseState gMouse;

static void KeyCallbackEvent(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_RELEASE)
		gKeyboard.keys[(int)key] = false;
	else
		gKeyboard.keys[(int)key] = true;
}

static void MouseButtonCallbackEvent(GLFWwindow* window, int button, int action, int mods)
{
	if (action == GLFW_RELEASE)
		gKeyboard.keys[(int)button] = false;
	else
		gKeyboard.keys[(int)button] = true;
}

static void MouseScrollCallbackEvent(GLFWwindow* window, double xOffset, double yOffset)
{
	gMouse.scrollWheelX = static_cast<float>(xOffset);
	gMouse.scrollWheelY = static_cast<float>(yOffset);
}

static void WindowResizeCallbackEvent(GLFWwindow* window, int width, int height)
{
	WindowResizeEvent evt(width, height);
	EventDispatcher::DispatchEvent(evt);
}

void GLFWInput::Initialize(Platform::WindowType window)
{
	glfwSetKeyCallback(window, KeyCallbackEvent);
	glfwSetMouseButtonCallback(window, MouseButtonCallbackEvent);
	glfwSetScrollCallback(window, MouseScrollCallbackEvent);
	glfwSetWindowSizeCallback(window, WindowResizeCallbackEvent);
}

void GLFWInput::Update(Platform::WindowType window)
{
	// Prepare for new frame
	gMouse.scrollWheelX = 0.0f;
	gMouse.scrollWheelY = 0.0f;

	glfwPollEvents();

	double x = 0.0, y = 0.0;
	glfwGetCursorPos(window, &x, &y);

	glm::vec2 p = glm::vec2{ float(x), float(y) };
	gMouse.delta = gMouse.position - p;
	gMouse.position = p;
}

void GLFWInput::GetKeyboardState(Input::KeyboardState* state)
{
	*state = gKeyboard;
}

void GLFWInput::GetMouseState(Input::MouseState* state)
{
	*state = gMouse;
}
