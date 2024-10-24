#pragma once
#include <ARMenuBar.h>
#include <ARToolBar.h>
#include <ARProjectExplorer.h>
#include <ARCodeEditor.h>
#include <ARStudioParams.hpp>
#include <Data/ARData.h>

class ARWindowsManager {
public:
	ARWindowsManager(bool* p_open);
	~ARWindowsManager();
	void Render();

private:
	void init();
	bool* p_open = nullptr;
	ARStudioWindowConfig window_config;
	ARStudioWindowFlags window_flags = 0;
	ARStudioWindowData win_data;
	ARStudioEventSignal* signal = nullptr;
	ARStudioJsonRootData* data = nullptr;
	ARMenuBar* menu_bar = nullptr;
	ARToolBar* tool_bar = nullptr;
	ARStudioProjectExplorer* project_explorer = nullptr;
	ARStudioEditorManager* editor_manager = nullptr;
};