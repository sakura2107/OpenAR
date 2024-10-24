#include <ARProjectExplorer.h>
#include <imgui/imgui.h>
#include <Data/ARJson.h>
#include <ARWindowsTools.hpp>

ARStudioProjectExplorer::ARStudioProjectExplorer(ARStudioEventSignal* signal, ARStudioJsonRootData* data) : signal(signal), data(data) {
	signal->root_data = data;
}

ARStudioProjectExplorer::~ARStudioProjectExplorer() {
	for (int i = 0; i < data->project_data_array.size(); i++) {
		delete project_map[i];
		delete data->project_data_array[i];
	}
	project_map.clear();
	data->project_data_array.clear();
}

void ARStudioProjectExplorer::Render() {
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;
	window_flags |= ImGuiWindowFlags_MenuBar;
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
	ImVec2 mainWindowSize = ImGui::GetWindowSize();
	const float posX = 5 * mainWindowSize.x / 7;
	const float posY = 100.0f;
	ImGui::SetNextWindowPos(ImVec2(posX, posY));
	ImGui::BeginChild("Project Explorer", ImVec2(2 * mainWindowSize.x / 7 - 20, mainWindowSize.y - posY - 20), ImGuiChildFlags_Borders, window_flags);
	ShowProjectExplorerMenu();
	ShowProjectExplorerTreeNode();
	ImGui::EndChild();
	ImGui::PopStyleVar();
}

void ARStudioProjectExplorer::ShowProjectExplorerMenu() {
	if (ImGui::BeginMenuBar()) {
		ImGui::Text("Project Explorer");
		ImGui::EndMenuBar();
	}
}

void ARStudioProjectExplorer::ShowProjectExplorerTreeNode() {
	int add_button_times = 0;

	if (ImGui::Button("Add", ImVec2(60, 0))) add_button_times++;
	if (add_button_times == 1) {
		signal->IsAddProject = true;
		if (signal->IsAddProject) AddProjectEvent();
	}
	ImGui::SameLine();
	int delete_botton_times = 0;
	if (ImGui::Button("Clear", ImVec2(60, 0))) delete_botton_times++;
	if (delete_botton_times == 1) {
		signal->IsRemoveAllProject = true;
		if (signal->IsRemoveAllProject) RemoveAllProjectEvent();
	}
	ImGui::SameLine();
	if (ImGui::Button("Load", ImVec2(60, 0))) LoadProjectEvent();
	if (signal->RemoveProjectID != -1) RemoveProjectEvent();

	if (ImGui::CollapsingHeader("./Root")) {
		for (int i = 0; i < data->project_data_array.size(); i++) {
			project_map[i]->project_id = i;
			project_map[i]->Render();
		}
	}
}

void ARStudioProjectExplorer::AddProjectEvent() {
	ARProject* ptr = new ARProject;
	data->project_data_array.push_back(ptr);
	project_map.push_back(new ARStudioProject(signal, ptr));
	signal->IsAddProject = false;
}

void ARStudioProjectExplorer::RemoveAllProjectEvent() {
	for (int i = 0; i < project_map.size(); i++) {
		delete project_map[i];
		delete data->project_data_array[i];
	}
	project_map.clear();
	data->project_data_array.clear();
	signal->IsRemoveAllProject = false;
}

void ARStudioProjectExplorer::RemoveProjectEvent() {
	delete project_map[signal->RemoveProjectID];
	delete data->project_data_array[signal->RemoveProjectID];
	project_map.erase(project_map.begin() + signal->RemoveProjectID);
	data->project_data_array.erase(data->project_data_array.begin() + signal->RemoveProjectID);
	signal->RemoveProjectID = -1;
}

void ARStudioProjectExplorer::LoadProjectEvent() {
	std::string path = ShowJsonFileSelectionDialog();
	ARProject* project_data = nullptr;
	if (!ar::loadJsonFile(path, &project_data))return;
	data->project_data_array.push_back(project_data);
	project_map.push_back(new ARStudioProject(signal, project_data));
}

ARStudioProject::ARStudioProject(ARStudioEventSignal* signal, ARProject* data) : signal(signal), data(data) {
	if (data->Size()) Load();
}

ARStudioProject::~ARStudioProject() {
	for (int i = 0; i < data->Size(); i++) delete task_map[i];
	task_map.clear();
	if (signal->ProjectData == data) signal->ProjectData = nullptr;
}

void ARStudioProject::Render() {
	ImGui::PushID(project_id);
	data->project_id = project_id;
	ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanTextWidth | ImGuiTreeNodeFlags_Framed;
	if (signal->RemoveProjectID == project_id) {
		node_flags |= ImGuiTreeNodeFlags_Selected;
	}

	bool IsTreeNode = ImGui::TreeNodeEx("", node_flags);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(150);
	ImGui::InputText("##buffer", data->project_name.data(), 32);
	ImGui::SameLine();

	if (signal->RemoveTaskID.first == project_id) RemoveTaskEvent();

	int add_button_times = 0;
	if (ImGui::Button("Add", ImVec2(60, 0))) add_button_times++;
	if (add_button_times == 1) AddTaskEvent();

	ImGui::SameLine();
	int delete_button_times = 0;
	if (ImGui::Button("Clear", ImVec2(60, 0))) delete_button_times++;
	if (delete_button_times == 1) {
		signal->RemoveProjectID = project_id;
	}

	if (IsTreeNode) {
		for (int i = 0; i < task_map.size(); i++) {
			task_map[i]->task_id = i;
			task_map[i]->project_id = project_id;
			task_map[i]->Render();
		}
		ImGui::TreePop();
	}

	ImGui::PopID();
}

void ARStudioProject::Load() {
	for (int i = 0; i < data->Size(); i++) {
		ARTask* ptr = data->getPtr(i);
		task_map.push_back(new ARStudioTask(signal, ptr, data, project_id));
	}
}

void ARStudioProject::AddTaskEvent() {
	data->push_back();
	task_map.push_back(new ARStudioTask(signal, data->getPtr(data->Size() - 1), data, project_id));
}

void ARStudioProject::RemoveTaskEvent() {
	delete task_map[signal->RemoveTaskID.second];
	task_map.erase(task_map.begin() + signal->RemoveTaskID.second);
	data->erase(signal->RemoveTaskID.second);
	signal->RemoveTaskID = { -1,-1 };
}

ARStudioTask::ARStudioTask(ARStudioEventSignal* signal, ARTask* data, ARProject* project_data, int project_id) :
	signal(signal), data(data), project_data(project_data), project_id(project_id) {}

ARStudioTask::~ARStudioTask() {
	if (signal->TaskData == data) signal->TaskData = nullptr;
}

void ARStudioTask::Render() {
	ImGui::PushID(task_id);
	data->task_id = task_id;
	ImGui::SetNextItemWidth(50);
	ImGui::Checkbox("", &is_on);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(150);
	ImGui::SameLine();

	int edit_button_times = 0;
	if (ImGui::Button("Edit", ImVec2(60, 0))) edit_button_times++;
	if (edit_button_times == 1) {
		signal->ShowCodeEditorID = task_id;
		signal->ProjectData = project_data;
		signal->TaskData = data;
	}

	ImGui::SameLine();
	int remove_button_times = 0;
	if (ImGui::Button("Remove", ImVec2(60, 0))) remove_button_times++;
	if (remove_button_times == 1) {
		signal->RemoveTaskID = { project_id , task_id };
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(130);
	ImGui::InputText("##buffer", data->task_name.data(), 32);
	ImGui::PopID();
}

