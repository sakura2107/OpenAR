#include <ARCodeEditor.h>
#include <ARPromptWindows.h>
#include <imgui/imgui.h>
#include <ARWindowsTools.hpp>

ARStudioEditorManager::ARStudioEditorManager(ARStudioEventSignal* signal) : signal(signal) {}

ARStudioEditorManager::~ARStudioEditorManager() {
	if (!code_editor) delete code_editor;
}

void ARStudioEditorManager::Render() {
	last_data = data;
	data = signal->TaskData;
	if (!data) return;
	if (last_data != data) SwapCodeEditor(); //updata code_editor
	code_editor->Render();
}

void ARStudioEditorManager::SwapCodeEditor() {
	if (!code_editor) delete code_editor;
	code_editor = new ARStudioCodeEditor(signal, data);
}

ARStudioCodeEditor::ARStudioCodeEditor(ARStudioEventSignal* signal, ARTask* data) : signal(signal), data(data) {
	if (data->Size()) Load();
}

ARStudioCodeEditor::~ARStudioCodeEditor() {
	for (int i = 0; i < data->Size(); i++) {
		delete block_map[i];
		delete add_and_remove_map[i];
	}
	block_map.clear();
	add_and_remove_map.clear();
}

void ARStudioCodeEditor::Render() {
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;
	window_flags |= ImGuiWindowFlags_MenuBar;
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
	ImVec2 mainWindowSize = ImGui::GetWindowSize();
	const int posX = 20;
	const int posY = 100;
	ImGui::SetNextWindowPos(ImVec2(posX, posY));
	ImGui::BeginChild("Code Editor", ImVec2(mainWindowSize.x - 2 * mainWindowSize.x / 7 - 40, mainWindowSize.y - posY - 20), ImGuiChildFlags_Borders, window_flags);
	if (ImGui::Button("Add", ImVec2(70, 0))) signal->AddBlockCombinationID = 0;

	float CodeEditorWindowSizeX = mainWindowSize.x - 2 * mainWindowSize.x / 7 - 40;

	int cur_id_for_block = 0;
	int cur_id_for_add_and_delete = 0;
	//Receive signal
	if (signal->AddBlockCombinationID != -1) ARStudioCodeEditor::AddBlockCombinationtEvent();
	if (signal->RemoveBlockCombinationID != -1) ARStudioCodeEditor::RemoveBlockCombinationEvent();
	//Render component
	for (int i = 0; i < data->Size(); i++) {
		block_map[i]->block_id = 2 * i;
		add_and_remove_map[i]->add_and_delete_id = 2 * i + 1;
		block_map[i]->Render();
		ImGui::SameLine();
		add_and_remove_map[i]->Render();
		if ((i + 1) % ((int)(CodeEditorWindowSizeX / (400 + 10 * 2)) > 1 ?
			(int)(CodeEditorWindowSizeX / (400 + 10 * 2)) : 1))
			ImGui::SameLine(); //Change line per 3 components
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();
}


void ARStudioCodeEditor::Load() {
	for (int i = 0; i < data->Size(); i++) {
		block_map.push_back(new ARStudioBlock(signal, data->getPtr(i)));
		add_and_remove_map.push_back(new ARStudioAddAndRemove(signal));
	}
}

void ARStudioCodeEditor::AddBlockCombinationtEvent() {
	if (block_map.size() == signal->AddBlockCombinationID) {
		data->push_back();
		block_map.push_back(new ARStudioBlock(signal, data->getPtr(data->Size() - 1)));
		add_and_remove_map.push_back(new ARStudioAddAndRemove(signal));
	}
	else {
		data->insert(signal->AddBlockCombinationID);
		block_map.insert(block_map.begin() + signal->AddBlockCombinationID, new ARStudioBlock(signal, data->getPtr(signal->AddBlockCombinationID)));
		add_and_remove_map.insert(add_and_remove_map.begin() + signal->AddBlockCombinationID, new ARStudioAddAndRemove(signal));
	}
	signal->AddBlockCombinationID = -1;
}

void ARStudioCodeEditor::RemoveBlockCombinationEvent() {
	delete block_map[signal->RemoveBlockCombinationID];
	delete add_and_remove_map[signal->RemoveBlockCombinationID];
	block_map.erase(block_map.begin() + signal->RemoveBlockCombinationID);
	add_and_remove_map.erase(add_and_remove_map.begin() + signal->RemoveBlockCombinationID);
	data->erase(signal->RemoveBlockCombinationID);
	signal->RemoveBlockCombinationID = -1;
}
//add and delete

ARStudioAddAndRemove::ARStudioAddAndRemove(ARStudioEventSignal* signal) : signal(signal) {}

ARStudioAddAndRemove::~ARStudioAddAndRemove() {}

void ARStudioAddAndRemove::Render() {
	ImGui::PushID(add_and_delete_id);
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
	ImGui::BeginChild("", ImVec2(90, 100), ImGuiChildFlags_Borders, window_flags);

	int loop_block_clicked_times = 0;
	if (ImGui::Button("Add", ImVec2(70, 0))) loop_block_clicked_times++;
	if (loop_block_clicked_times == 1) signal->AddBlockCombinationID = (add_and_delete_id - 1) / 2 + 1; //add location should + 1

	int remove_clicked_times = 0;
	if (ImGui::Button("Remove", ImVec2(70, 0))) remove_clicked_times++;
	if (remove_clicked_times == 1) signal->RemoveBlockCombinationID = (add_and_delete_id - 1) / 2;

	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopID();
}

//Block

ARStudioBlock::ARStudioBlock(ARStudioEventSignal* signal, ARBlock* data) : signal(signal), data(data) {
	if (data->Size()) Load();
}

ARStudioBlock::~ARStudioBlock() {
	for (int i = 0; i < logic_pair_map.size(); i++) delete logic_pair_map[i];
	logic_pair_map.clear();
}

void ARStudioBlock::Render() {
	ImGui::PushID(block_id);
	data->block_id = block_id;
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
	ImVec2 CodeEditerWindowSize = ImGui::GetWindowSize();
	ImGui::BeginChild("", ImVec2(300, 260), ImGuiChildFlags_Borders, window_flags);
	ImVec2 CodeBlockWindowSize = ImGui::GetWindowSize();

	ImGui::TextColored(ImVec4(61.0f / 255, 195.0f / 255, 110.0f / 255, 200.0f / 255), "Looping Block");
	ImGui::SetNextItemWidth((int)(CodeBlockWindowSize.x - 60 * 2 - 10 * 4 > 0) ?
		(int)(CodeBlockWindowSize.x - 60 * 2 - 10 * 4) : 0);
	ImGui::InputText("##LoopingBlock :", data->block_name.data(), 32);

	ImGui::SameLine();
	int add_clicked_times = 0;
	if (ImGui::Button("Add", ImVec2(60, 0))) add_clicked_times++;
	if (add_clicked_times == 1) signal->IsAddLogicPair = true;

	ImGui::SameLine();
	int remove_clicked_times = 0;
	if (ImGui::Button("Remove", ImVec2(60, 0))) remove_clicked_times++;
	if (remove_clicked_times == 1) signal->IsRemoveLogicPair = true;

	if (signal->IsAddLogicPair) AddLogicPairEvent();
	if (signal->IsRemoveLogicPair) RemoveLogicPairEvent();

	for (int i = 0; i < data->Size(); i++) {
		logic_pair_map[i]->pair_id = i;
		logic_pair_map[i]->Render();
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopID();
}

void ARStudioBlock::Load() {
	for (int i = 0; i < data->Size(); i++)
		logic_pair_map.push_back(new ARStudioCode(signal, data->getPtr(i)));
}

void ARStudioBlock::AddLogicPairEvent() {
	data->push_back();
	logic_pair_map.push_back(new ARStudioCode(signal, data->getPtr(data->Size() - 1)));
	signal->IsAddLogicPair = false;
}

void ARStudioBlock::RemoveLogicPairEvent() {
	if (!logic_pair_map.size()) {
		signal->IsRemoveLogicPair = false;
		return;
	}
	delete logic_pair_map[logic_pair_map.size() - 1];
	data->pop_back();
	logic_pair_map.pop_back();
	signal->IsRemoveLogicPair = false;
}

ARStudioCode::ARStudioCode(ARStudioEventSignal* signal, ARCode* data) : signal(signal), data(data) {}

ARStudioCode::~ARStudioCode() {}

void ARStudioCode::Render() {
	ImGui::PushID(pair_id);
	data->code_id = pair_id;
	ImVec2 CodeBlockWindowSize = ImGui::GetWindowSize();
	ImGui::SetNextItemWidth((CodeBlockWindowSize.x - 60 - 10 * 4) / 2 > 0.0f ?
		(CodeBlockWindowSize.x - 60 - 10 * 4) / 2 : 0.0f);
	ImGui::Combo("##LeftCombo", &data->first_value, left_combo_items, (int)sizeof(left_combo_items) / sizeof(*left_combo_items));
	ImGui::SameLine();
	ImGui::SetNextItemWidth((CodeBlockWindowSize.x - 60 - 10 * 4) / 2 > 0.0f ?
		(CodeBlockWindowSize.x - 60 - 10 * 4) / 2 : 0.0f);
	ImGui::Combo("##RightCombo", &data->second_value, right_combo_items, (int)sizeof(right_combo_items) / sizeof(*right_combo_items));
	ImGui::SameLine();
	if (ImGui::Button("Setting", ImVec2(60, 0))) {
		p_open = true;
	}
	if (p_open) ShowSettingWindows(&p_open);
	ImGui::PopID();
}

void ARStudioCode::ShowSettingWindows(bool* p_open) {
	ImGui::SetNextWindowSize(ImVec2(600, 360));
	ImGui::OpenPopup("Settings");
	if (ImGui::BeginPopupModal("Settings", NULL, ImGuiWindowFlags_None)) {
		ShowSettingLeftWindow();
		ImGui::SameLine();
		ShowSettingRightWindow();
		if (ImGui::Button("Close", ImVec2(70, 30))) {
			ImGui::CloseCurrentPopup();
			*p_open = false;
		}

		ImGui::EndPopup();
	}
}

void ARStudioCode::ShowSettingLeftWindow() {
	ImGui::PushID(pair_id * 2);
	ImGui::BeginChild("LeftWindow", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 270), ImGuiChildFlags_Borders);
	if (data->first_value == 1) {
		ImGui::SetNextItemWidth(105);
		ImGui::InputText("##ImagePath", data->image_path.data(), 64);
		ImGui::SameLine();
		if (ImGui::Button("Browse", ImVec2(60, 0))) {
			data->image_path = ShowImageFileSelectionDialog();
		}
	}
	// Text Unsupport Now
	if (data->first_value == 3) {
		ImGui::Text("TimeOut :");
		ImGui::SameLine();
		ImGui::InputInt("##TimeOut", &data->time_out);
	}
	ImGui::EndChild();
	ImGui::PopID();
}

void ARStudioCode::ShowSettingRightWindow() {
	ImGui::PushID(pair_id * 2 + 1);
	ImGui::BeginChild("RightWindow", ImVec2(0, 270), ImGuiChildFlags_Borders);
	if (data->second_value == 1) {
		ImGui::Text("Click x :");
		ImGui::SameLine();
		ImGui::InputInt("##ClickX", &data->click_x);
		ImGui::Text("Click y :");
		ImGui::SameLine();
		ImGui::InputInt("##ClickY", &data->click_y);
	}
	if (data->second_value == 2) {
		ImGui::Text("Swipe x_1 :");
		ImGui::SameLine();
		ImGui::InputInt("##SwipeX_1", &data->swipe_x_1);
		ImGui::Text("Swipe y_1 :");
		ImGui::SameLine();
		ImGui::InputInt("##SwipeY_1", &data->swipe_y_1);
		ImGui::Text("Swipe x_2 :");
		ImGui::SameLine();
		ImGui::InputInt("##SwipeX_2", &data->swipe_x_2);
		ImGui::Text("Swipe y_2 :");
		ImGui::SameLine();
		ImGui::InputInt("##SwipeY_2", &data->swipe_y_2);
	}
	if (data->second_value == 3) {
		ImGui::Text("LongClick x :");
		ImGui::SameLine();
		ImGui::InputInt("##LongClickX", &data->long_click_x);
		ImGui::Text("LongClick y :");
		ImGui::SameLine();
		ImGui::InputInt("##LongClickY", &data->long_click_y);
		ImGui::Text("Time :");
		ImGui::SameLine();
		ImGui::InputInt("##LongClickTime", &data->long_click_time);
	}
	if (data->second_value == 4) {
		ImGui::Text("InputKey :");
		ImGui::SameLine();
		ImGui::InputInt("##InputKey", &data->key_code);
	}
	if (data->second_value == 5) {
		ImGui::Text("Sleep :");
		ImGui::SameLine();
		ImGui::InputInt("##Sleep", &data->sleep_time);
	}
	ImGui::EndChild();
	ImGui::PopID();
}