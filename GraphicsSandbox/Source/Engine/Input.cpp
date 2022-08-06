#include "Input.h"
#include "GLFWInput.h"
#include "Logger.h"
#include <unordered_map>

namespace Input
{
	KeyboardState gKeyboard;
	MouseState gMouse;

	std::unordered_map<Key, uint32_t> inputs;

	const KeyboardState& GetKeyboardState() { return gKeyboard; }

	const MouseState& GetMouseState()       { return gMouse; }

	void Initialize(Platform::WindowType window)
	{
#ifdef GLFW_WINDOW
		Logger::Debug("Initializing GLFW Input");
		GLFWInput::Initialize(window);
#endif
	}

	void Update(Platform::WindowType window)
	{
#ifdef GLFW_WINDOW
		GLFWInput::Update(window);
		GLFWInput::GetKeyboardState(&gKeyboard);
		GLFWInput::GetMouseState(&gMouse);
#endif
		for (auto iter = inputs.begin(); iter != inputs.end();)
		{
			if (Down(iter->first)) {
				iter->second++;
				++iter;
			}
			else
			{
				inputs.erase(iter++);
			}
		}
	}

	bool Down(Key key)
	{
		return gKeyboard.keys[(int)key] == true;
	}

	bool Press(Key key)
	{
		if (!Down(key))
			return false;

		auto iter = inputs.find(key);
		if (iter == inputs.end())
		{
			inputs.insert(std::make_pair(key, 0));
			return true;
		}

		if (iter->second == 0)
			return true;

		return false;
	}

	bool Hold(Key key, uint32_t frames)
	{
		if (!Down(key))
			return false;

		auto iter = inputs.find(key);
		if (iter == inputs.end())
		{
			inputs.insert(std::make_pair(key, 0));
			return false;
		}
		else if (iter->second >= frames)
			return true;

		return false;
	}
}