#include <ARToolBar.h>
#include <imgui/imgui.h>
#include <ARPromptWindows.h>
#include <ARWindowsTools.hpp>

ARToolBar::ARToolBar(ARStudioEventSignal* signal) : signal(signal) {}

ARToolBar::~ARToolBar() {}

void ARToolBar::Render() {
	data = signal->ProjectData;
	if (data == nullptr) return;
	ImGui::Text("ADB Path :");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100);
	ImGui::InputText("##ADB path :", data->adb_path.data(), 32);

	ImGui::SameLine();
	ImGui::Text("ADB Port :");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100);
	ImGui::InputInt("##adb_port_input_text", &data->adb_port, 32);

	ImGui::SameLine();
	ImGui::Text("Device :");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100);
	ImGui::Combo("##device_combo", &data->device_type, device_items, (int)sizeof(device_items) / sizeof(*device_items));

	ImGui::SameLine();
	ImGui::Text("Controller :");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100);
	ImGui::Combo("##controller_combo", &data->controller_type, controller_items, (int)sizeof(controller_items) / sizeof(*controller_items));

	ImGui::SameLine();
	ImGui::Text("Image Recognition :");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100);
	ImGui::Combo("##image_recognition_combo", &data->image_recognition_type, image_recognition_items, (int)sizeof(image_recognition_items) / sizeof(*image_recognition_items));

	ImGui::SetCursorPosY(60);
	ImGui::Text("Device Path :");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200);
	ImGui::InputText("##device_path_input_text", data->device_path.data(), 128);

	ImGui::SameLine();
	ImGui::Dummy(ImVec2(5, 0));
	ImGui::SameLine();
	if (ImGui::Button("Browse ..", ImVec2(80, 0))) {
		data->device_path = ShowFolderSelectionDialog();
	}

	ImGui::SameLine();
	ImGui::Dummy(ImVec2(30, 0));
	ImGui::SameLine();
	int run_button_clicked = 0;
	if (ImGui::Button("Run", ImVec2(80, 0)))
		run_button_clicked++;
	if (run_button_clicked == 1)
	{
		//add event here !
	}

	ImGui::SameLine();
	ImGui::Dummy(ImVec2(30, 0));
	ImGui::SameLine();
	int stop_button_clicked = 0;
	if (ImGui::Button("Stop", ImVec2(80, 0)))
		stop_button_clicked++;
	if (stop_button_clicked == 1)
	{
		//add event here !
	}

	ImGui::SameLine();
	ImGui::Dummy(ImVec2(30, 0));
	ImGui::SameLine();
	if (ImGui::Button("Save", ImVec2(80, 0))) p_open = true;
	if (p_open) ShowSaveWindow(data, save_path, &p_open);

	ImGui::SameLine();
	ImGui::Dummy(ImVec2(30, 0));
	ImGui::SameLine();
	ImGui::Text("Device Index :");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	ImGui::InputInt("##device_index", &data->device_index);
}