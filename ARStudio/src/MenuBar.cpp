#include <MenuBar.h>
#include <imgui.h>

MenuBar::MenuBar(ARStudioWindowData* win_data) : win_data(win_data) {}

MenuBar::~MenuBar() {}

void MenuBar::Render() {
	if (win_data != nullptr) MenuBar::ShowMenuBar();
}

void MenuBar::ShowMenuBar() {
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("Menu"))
		{
			ShowMenuFile();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Tools"))
		{
			ShowToolsFile();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Windows"))
		{
			ShowWindowFile();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help"))
		{
			ShowHelpFile();
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
}

void MenuBar::ShowMenuFile() {
	if (ImGui::MenuItem("New", "Ctrl+N")) {
		//Add event function here ...
	}
	if (ImGui::MenuItem("Open", "Ctrl+O")) {
		//Add event function here ...
	}
	if (ImGui::MenuItem("Save", "Ctrl+S")) {
		//Add event function here ...
	}
	if (ImGui::MenuItem("Save as ...")) {
		//Add event function here ...
	}

	ImGui::MenuItem("Auto save", NULL, &win_data->IsAutoSave);

	if (ImGui::MenuItem("Exit", "Alt+F4")) {
		//Add event function here ...
	}
}

void MenuBar::ShowToolsFile() {
	ImGui::MenuItem("Log trace tool", NULL, &win_data->IsShowLogTraceTool);
}

void MenuBar::ShowWindowFile() {
	ImGui::MenuItem("Save window layout", NULL, &win_data->IsSaveWindowLayout);
	ImGui::MenuItem("Theme", NULL, &win_data->IsSelectedTheme);
	ImGui::MenuItem("Edit theme", NULL, &win_data->IsEditTheme);
}

void MenuBar::ShowHelpFile() {
	ImGui::MenuItem("View tutorial", NULL, &win_data->IsViewTutorial);
}