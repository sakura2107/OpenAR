#pragma once
#include <WindowsParams.hpp>

class MenuBar {
public:
	MenuBar(ARStudioWindowData* win_data);
	~MenuBar();
	void Render();
private:
	void ShowMenuBar();

	void ShowMenuFile();
	void ShowToolsFile();
	void ShowWindowFile();
	void ShowHelpFile();

	ARStudioWindowData* win_data = nullptr;
};