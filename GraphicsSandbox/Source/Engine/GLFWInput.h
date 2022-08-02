#pragma once

#include "Platform.h"
#include "Input.h"

namespace GLFWInput
{
	void Initialize(Platform::WindowType window);

	void Update(Platform::WindowType window);

	void GetKeyboardState(Input::KeyboardState* state);

	void GetMouseState(Input::MouseState* state);

}