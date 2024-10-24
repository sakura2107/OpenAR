#pragma once
#include <ARStudioParams.hpp>
#include <vector>
#include <Data/ARData.h>


// ARStudio Code Editor Manager
class ARStudioEditorManager;
// ARStudio Code Editor Window
class ARStudioCodeEditor;
// ARStudio Code Add Loop Button And Remove Button
class ARStudioAddAndRemove;
// ARStudio Code Block Window
class ARStudioBlock;
// ARStudio Code Logic Pair (Which Is In Block Windows)
class ARStudioCode;

class ARStudioEditorManager {
public:
	ARStudioEditorManager(ARStudioEventSignal* signal);
	~ARStudioEditorManager();
	void Render();

private:
	void SwapCodeEditor();
	ARStudioEventSignal* signal = nullptr;
	ARTask* data = nullptr;
	ARTask* last_data = nullptr;
	ARStudioCodeEditor* code_editor = nullptr;
};

class ARStudioCodeEditor {
public:
	ARStudioCodeEditor(ARStudioEventSignal* signal, ARTask* data);
	~ARStudioCodeEditor();
	void Render();
private:
	void Load();
	void AddBlockCombinationtEvent();
	void RemoveBlockCombinationEvent();
	ARStudioEventSignal* signal = nullptr;
	ARTask* data = nullptr;
	std::vector<ARStudioBlock*> block_map;
	std::vector<ARStudioAddAndRemove*> add_and_remove_map;
};

class ARStudioAddAndRemove {
public:
	friend class ARStudioCodeEditor;
	ARStudioAddAndRemove(ARStudioEventSignal* signal);
	~ARStudioAddAndRemove();
	void Render();
protected:
	int add_and_delete_id = -1;
private:
	ARStudioEventSignal* signal = nullptr;
};

class ARStudioBlock {
public:
	friend class ARStudioCodeEditor;
	ARStudioBlock(ARStudioEventSignal* signal, ARBlock* data);
	~ARStudioBlock();
	void Render();
protected:
	int block_id;
private:
	void Load();
	void AddLogicPairEvent();
	void RemoveLogicPairEvent();
	ARStudioEventSignal* signal = nullptr;
	ARBlock* data = nullptr;
	std::vector<ARStudioCode*> logic_pair_map;
};

class ARStudioCode {
public:
	friend class ARStudioBlock;
	ARStudioCode(ARStudioEventSignal* signal, ARCode* data);
	~ARStudioCode();
	void Render();
protected:
	int pair_id = -1;
private:
	void ShowSettingWindows(bool* p_open);
	void ShowSettingLeftWindow();
	void ShowSettingRightWindow();
	bool p_open = false;
	ARStudioEventSignal* signal = nullptr;
	ARCode* data = nullptr;
	const char* left_combo_items[4] = { "Null", "Image", "Text(Unsupport)", "TimeOut" };
	const char* right_combo_items[9] = { "Null", "Click", "Swipe", "LongClick", "InputKey", "Sleep", "StopLoop" , "Return", "Exit"};
};
