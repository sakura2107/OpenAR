#pragma once
#include <Data/ARData.h>
#include <ARLGlobalParams.h>
#include <unordered_map>
#include <memory_resource>
#include <functional>
#include <opencv2/opencv.hpp>
#include <memory>

class ARLTaskNode;
class ARLImagePool;

class ARLTaskNode {
public:
	ARLTaskNode(ARLGlobalParams* global_params, ARTask* task_data);
	~ARLTaskNode() {};
private:
	ARLGlobalParams* global_params = nullptr;
	ARTask* task_data = nullptr;
	std::unordered_map<std::string, cv::Mat> image_pool;
	struct callbackParams {
		ARCode* code_data;
		ar::point& point;
		cv::Mat& frame;
		int& cur_time_out;
		bool& stop_condition;
	};
	bool is_return = false;
private:
	void playPerLoopingBlock(ARBlock* block);
	void loadImageToPool();
	void codePerform(const std::function<bool(callbackParams&)>& first_value_callback,
		const std::function<void(const callbackParams&)>& second_value_callback,
		callbackParams& m_callback_params);

	bool imageRecognitionCallback(callbackParams& params);
	bool textRecgnitionCallback(callbackParams& params);
	bool timeOutCallback(callbackParams& params);

	void clickCallback(const callbackParams& params);
	void swipeCallback(const callbackParams& params);
	void longClickCallback(const callbackParams& params);
	void inputKeyCallback(const callbackParams& params);
	void sleepCallback(const callbackParams& params);
	void stopLoopCallback(const callbackParams& params);
	void returnCallback(const callbackParams& params);
	void exitCallback(const callbackParams& params);

public:
	void play();
};