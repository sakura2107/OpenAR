#include <ARMenuBar.h>
#include <imgui/imgui.h>

ARMenuBar::ARMenuBar(ARStudioWindowData* p_win_data) : p_win_data(p_win_data) {}

ARMenuBar::~ARMenuBar() {}

void ARMenuBar::Render() {
	if (p_win_data != nullptr) ARMenuBar::ShowMenuBar();
}

void ARMenuBar::ShowMenuBar() {
	if (bool res = ImGui::BeginMenuBar())
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

void ARMenuBar::ShowMenuFile() {
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

	if (ImGui::MenuItem("Exit", "Alt+F4")) {
		//Add event function here ...
	}
}

void ARMenuBar::ShowToolsFile() {
	ImGui::MenuItem("Log trace tool", NULL, &p_win_data->IsShowLogTraceTool);
}

void ARMenuBar::ShowWindowFile() {
	ImGui::MenuItem("Save window layout", NULL, &p_win_data->IsSaveWindowLayout);
	ImGui::MenuItem("Theme", NULL, &p_win_data->IsSelectedTheme);
	ImGui::MenuItem("Edit theme", NULL, &p_win_data->IsEditTheme);
}

void ARMenuBar::ShowHelpFile() {
	ImGui::MenuItem("View tutorial", NULL, &p_win_data->IsViewTutorial);
}