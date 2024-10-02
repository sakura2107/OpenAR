#pragma once
#include <windows.h>
#include <Log/MiniLog.h>
#include <opencv2/opencv.hpp>

#define FUNCTION_LOOPS(f) \
stop_condition = false; \
times = 0; \
do{ \
	Sleep(params.operate_duration); \
	times++; \
	if (times > params.run_max_times) { ar::error("Runtime too long !"); std::exit(0); } \
	\
	cv::Mat frame; \
	params.controller->screencap(frame); \
	if (frame.empty()) { \
	ar::error("Frame empty !"); \
	params.controller->disconnect(); \
	std::exit(0); \
	} \
	\
	f();\
}while (!stop_condition);

#define LOAD_IMAGE(name, path) \
cv::Mat name = cv::imread(path); \
if(name.empty()) { ar::error("{} is empty!", #name); std::exit(0);}