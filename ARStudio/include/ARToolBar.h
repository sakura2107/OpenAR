#pragma once
#include <ARStudioParams.hpp>
#include <string>
#include <Data/ARData.h>

class ARToolBar {
public:
	ARToolBar(ARStudioEventSignal* signal);
	~ARToolBar();
	void Render();
private:
	ARStudioEventSignal* signal = nullptr;
	ARProject* data = nullptr;
	const char* device_items[2] = { "None", "MuMu" };
	const char* controller_items[2] = { "ADB", "MuMu" };
	const char* image_recognition_items[3] = { "SPR", "MPR", "MPR_CUDA" };
	int tool_bar_id = -1;
	std::string save_path = ".\\Select\\Path";
	bool p_open = false;
};