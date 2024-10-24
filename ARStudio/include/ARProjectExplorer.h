#pragma once
#include <ARStudioParams.hpp>
#include <ARCodeEditor.h>
#include <ARToolBar.h>
#include <vector>
#include <string>
#include <Data/ARData.h>

class ARStudioProjectExplorer;
class ARStudioProject;
class ARStudioTask;

class ARStudioProjectExplorer {
public:
	ARStudioProjectExplorer(ARStudioEventSignal* signal, ARStudioJsonRootData* data);
	~ARStudioProjectExplorer();
	void Render();
private:
	void ShowProjectExplorerMenu();
	void ShowProjectExplorerTreeNode();

	void AddProjectEvent();
	void RemoveAllProjectEvent();
	void RemoveProjectEvent();
	void LoadProjectEvent();
	int add_button_times = 0;
	int delete_botton_times = 0;
	ARStudioEventSignal* signal = nullptr;
	ARStudioJsonRootData* data = nullptr;
	std::vector<ARStudioProject*> project_map;
};

class ARStudioProject {
public:
	friend class ARStudioProjectExplorer;
	ARStudioProject(ARStudioEventSignal* signal, ARProject* data);
	~ARStudioProject();
	void Render();
protected:
	int project_id;
private:
	void AddTaskEvent();
	void RemoveTaskEvent();
	void Load();
	std::string name;
	ARStudioEventSignal* signal = nullptr;
	ARProject* data = nullptr;
	std::vector<ARStudioTask*> task_map;
};

class ARStudioTask {
public:
	friend class ARStudioProject;
	ARStudioTask(ARStudioEventSignal* signal, ARTask* data, ARProject* project_data, int project_id);
	~ARStudioTask();
	void Render();
protected:
	int task_id = -1;
	int project_id = -1;
private:
	ARStudioEventSignal* signal = nullptr;
	ARTask* data = data;
	ARProject* project_data; // This params is use to tell project's ToolBar what should be render
	bool is_on = false;
};