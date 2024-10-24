#include <ARPromptWindows.h>
#include <imgui/imgui.h>
#include <string>
#include <ARWindowsTools.hpp>
#include <Data/ARJson.h>

void ShowSaveWindow(ARProject* data, std::string& path, bool* p_open)
{
	ImGui::SetNextWindowSize(ImVec2(600, 120));
	ImGui::OpenPopup("Save Item");
	if (ImGui::BeginPopupModal("Save Item", NULL, ImGuiWindowFlags_None)) {
		ImGui::SetNextItemWidth(300);
		ImGui::InputText("Saving path", path.data(),64);
		ImGui::SameLine();
		if (ImGui::Button("Browse ..",ImVec2(70,0))) {
			path = ShowFolderSelectionDialog();
			path += "\\test.json";
		}
		ImGui::SameLine();
		if (ImGui::Button("Save", ImVec2(70, 0))) {
			if (ar::saveJsonFile(path, data)) {
				ImGui::CloseCurrentPopup();
				*p_open = false;
			}
		}
		if (ImGui::Button("Close")) {
			ImGui::CloseCurrentPopup();
			*p_open = false;
		}
		ImGui::EndPopup();
	}
}