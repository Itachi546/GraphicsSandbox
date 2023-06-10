#pragma once

#include <string>
#include "../Engine/Platform.h"
class FileDialog
{
public:
	static std::wstring Open(const wchar_t* basePath, Platform::WindowType window, const wchar_t* filter)
	{
#ifdef PLATFORM_WINDOW
		TCHAR szFile[256] = { 0 };
		OPENFILENAME ofn;
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = Platform::GetWindowHandle(window);
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = filter;
		ofn.lpstrInitialDir = basePath;
		ofn.nFilterIndex = 1;
		ofn.nMaxFileTitle = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
		if (GetOpenFileName(&ofn) == TRUE)
			return std::wstring(szFile);
#else 
    #error "Unsupported Platform"
#endif
		return std::wstring();
	}
};