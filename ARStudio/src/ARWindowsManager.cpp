#include <ARWindowsManager.h>
#include <imgui/imgui.h>

ARWindowsManager::ARWindowsManager(bool* p_open) : p_open(p_open) {
	init();
}

ARWindowsManager::~ARWindowsManager() {
	if (signal != nullptr) {
		delete signal;
		signal = nullptr;
	}
	if (data != nullptr) {
		delete data;
		data = nullptr;
	}
	if (menu_bar != nullptr) {
		delete menu_bar;
		menu_bar = nullptr;
	}
	if (tool_bar != nullptr) {
		delete tool_bar;
		tool_bar = nullptr;
	}
	if (project_explorer != nullptr) {
		delete project_explorer;
		project_explorer = nullptr;
	}
	if (editor_manager != nullptr) {
		delete editor_manager;
		editor_manager = nullptr;
	}
}

void ARWindowsManager::Render() 
{
	bool use_work_area = true;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(use_work_area ? viewport->WorkPos : viewport->Pos);
	ImGui::SetNextWindowSize(use_work_area ? viewport->WorkSize : viewport->Size);

	window_flags = 0;
	if (window_config.no_titlebar)        window_flags |= ImGuiWindowFlags_NoTitleBar;
	if (window_config.no_scrollbar)       window_flags |= ImGuiWindowFlags_NoScrollbar;
	if (!window_config.no_menu)           window_flags |= ImGuiWindowFlags_MenuBar;
	if (window_config.no_move)            window_flags |= ImGuiWindowFlags_NoMove;
	if (window_config.no_resize)          window_flags |= ImGuiWindowFlags_NoResize;
	if (window_config.no_collapse)        window_flags |= ImGuiWindowFlags_NoCollapse;
	if (window_config.no_nav)             window_flags |= ImGuiWindowFlags_NoNav;
	if (window_config.no_background)      window_flags |= ImGuiWindowFlags_NoBackground;
	if (window_config.no_bring_to_front)  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	if (window_config.unsaved_document)   window_flags |= ImGuiWindowFlags_UnsavedDocument;

	if (!ImGui::Begin("ARStudio", p_open, window_flags))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

	menu_bar->Render();
	tool_bar->Render();
	project_explorer->Render();
	editor_manager->Render();

	ImGui::End();
}

void ARWindowsManager::init()
{
	signal = new ARStudioEventSignal;
	data = new ARStudioJsonRootData;
	menu_bar = new ARMenuBar(&win_data);
	tool_bar = new ARToolBar(signal);
	project_explorer = new ARStudioProjectExplorer(signal, data);
	editor_manager = new ARStudioEditorManager(signal);
}