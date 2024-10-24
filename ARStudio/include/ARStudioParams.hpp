#pragma once
#include <string>
#include <vector>
#include <Data/ARData.h>
typedef int ARStudioWindowFlags;

/*
	Null = 0
	MuMu = 1
*/
typedef int ARDEVICETYPE;
/*
	ADB = 0
	MuMu = 1
*/
typedef int ARCONTROLLERTYPE;
/*
	SPR = 0
	MPR = 1
	MPR_CUDA = 2
*/
typedef int ARIMAGERECOGNITIONTYPE;

struct ARStudioWindowConfig {
	bool no_titlebar = true;
	bool no_scrollbar = false;
	bool no_menu = false;
	bool no_move = true;
	bool no_resize = true;
	bool no_collapse = false;
	bool no_close = false;
	bool no_nav = false;
	bool no_background = false;
	bool no_bring_to_front = true;
	bool unsaved_document = false;
};

struct ARStudioWindowData {
	// ARStudio Menu (accessible from the "Menu" menu)
	bool IsAutoSave = false;
	// ARStudio Tool (accessible from the "Tool" menu)
	bool IsShowLogTraceTool = false;
	// ARStudio Window (accessible from the "Window" menu)
	bool IsSaveWindowLayout = false;
	bool IsSelectedTheme = false;
	bool IsEditTheme = false;
	// ARStudio Help (accessible from the "Help" menu)
	bool IsViewTutorial = false;
};

struct ARStudioJsonRootData {
	// ARStudio Root params
	std::vector<ARProject*> project_data_array;
};

struct ARStudioEventSignal {
	//Global Root Data
	ARStudioJsonRootData* root_data = nullptr;
	// ARStudio Signal for ARToolBar
	ARProject* ProjectData = nullptr;
	// ARStudio Signal for ARProjectExplorer
	bool IsAddProject = false;
	bool IsRemoveAllProject = false;
	int RemoveProjectID = -1;
	bool IsAddTask = false;
	std::pair<int, int>RemoveTaskID = { -1,-1 };
	int ShowCodeEditorID = -1;

	// ARStudio Signal for ARStudioCodeEditor
	ARTask* TaskData = nullptr;
	int AddBlockCombinationID = -1; //add block and add_and_delete window ID
	int RemoveBlockCombinationID = -1;
	bool IsAddLogicPair = false;
	bool IsRemoveLogicPair = false;
	bool IsOpenSetting = false;
};