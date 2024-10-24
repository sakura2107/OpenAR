#pragma once
#include <ARStudioParams.hpp>

class ARMenuBar {
public:
	ARMenuBar(ARStudioWindowData* p_win_data);
	~ARMenuBar();
	void Render();
private:
	void ShowMenuBar();

	void ShowMenuFile();
	void ShowToolsFile();
	void ShowWindowFile();
	void ShowHelpFile();

	ARStudioWindowData* p_win_data = nullptr;
};