#include <ARLTaskNode.h>
#include <Log/MiniLog.h>
#include <chrono>
#include <Windows.h>

ARLTaskNode::ARLTaskNode(ARLGlobalParams* global_params, ARTask* task_data) :
	global_params(global_params), task_data(task_data)
{
	loadImageToPool();
}

void ARLTaskNode::loadImageToPool()
{
	for (int i = 0; i < task_data->Size(); i++) {
		ARBlock* block_data = task_data->getPtr(i);
		for (int j = 0; j < block_data->Size(); j++) {
			ARCode* code_data = block_data->getPtr(j);
			if (code_data->first_value != 1) continue;
			if (image_pool.find(code_data->image_path) != image_pool.end()) continue;
			cv::Mat image = cv::imread(code_data->image_path);
			if (image.empty()) {
				ar::error("Failed to load image from path: {}", code_data->image_path);
				std::exit(0);
			}
			image_pool[code_data->image_path] = image;
		}
	}
}

void ARLTaskNode::play()
{
	ar::info("Task Name : {} Task ID : {}", task_data->task_name, task_data->task_id);

	for (int i = 0; i < task_data->Size(); i++) {
		if (is_return) return;
		ARBlock* block_ptr = task_data->getPtr(i);
		playPerLoopingBlock(block_ptr);
	}
}

void ARLTaskNode::playPerLoopingBlock(ARBlock* block_data)
{
	//Signal For Second Value
	//给第二个值的信号
	bool stop_condition = false;
	int cur_time_out = 0;

	//Event looping
	//事件循环
	do {
		auto start_time = std::chrono::steady_clock::now();
		bool is_time_out = false;

		cv::Mat frame;
		global_params->controller->screencap(frame);

		std::function<bool(callbackParams&)> first_callback_func;
		std::function<void(const callbackParams&)> second_callback_func;

		for (int i = 0; i < block_data->Size(); i++) {
			ar::point m_point;
			ARCode* code_data = block_data->getPtr(i);
			switch (code_data->first_value) {
			case 0:
				first_callback_func = [](callbackParams&)->bool {return true; };
				break;
			case 1:
				first_callback_func = std::bind(&ARLTaskNode::imageRecognitionCallback, this, std::placeholders::_1);
				break;
			case 2:
				first_callback_func = std::bind(&ARLTaskNode::textRecgnitionCallback, this, std::placeholders::_1);
				break;
			case 3:
				first_callback_func = std::bind(&ARLTaskNode::timeOutCallback, this, std::placeholders::_1);
				break;
			}
			switch (code_data->second_value) {
			case 0:
				second_callback_func = [](const callbackParams&) {return; };
			case 1:
				second_callback_func = std::bind(&ARLTaskNode::clickCallback, this, std::placeholders::_1);
				break;
			case 2:
				second_callback_func = std::bind(&ARLTaskNode::swipeCallback, this, std::placeholders::_1);
				break;
			case 3:
				second_callback_func = std::bind(&ARLTaskNode::longClickCallback, this, std::placeholders::_1);
				break;
			case 4:
				second_callback_func = std::bind(&ARLTaskNode::inputKeyCallback, this, std::placeholders::_1);
				break;
			case 5:
				second_callback_func = std::bind(&ARLTaskNode::sleepCallback, this, std::placeholders::_1);
				break;
			case 6:
				second_callback_func = std::bind(&ARLTaskNode::stopLoopCallback, this, std::placeholders::_1);
				break;
			case 7:
				second_callback_func = std::bind(&ARLTaskNode::returnCallback, this, std::placeholders::_1);
				break;
			case 8:
				second_callback_func = std::bind(&ARLTaskNode::exitCallback, this, std::placeholders::_1);
				break;
			}

			callbackParams m_callback_params = { code_data , m_point, frame, cur_time_out, stop_condition};
			codePerform(first_callback_func, second_callback_func, m_callback_params);
		}

		auto end_time = std::chrono::steady_clock::now();
		auto time_out_duration = end_time - start_time;
		cur_time_out += time_out_duration.count();
	} while (!stop_condition);
}

void ARLTaskNode::codePerform(const std::function<bool(callbackParams&)>& first_value_callback,
	const std::function<void(const callbackParams&)>& second_value_callback,
	callbackParams& m_callback_params)
{
	if (first_value_callback(m_callback_params)) second_value_callback(m_callback_params);
}

bool ARLTaskNode::imageRecognitionCallback(callbackParams& params)
{
	const std::string& m_image_path = params.code_data->image_path;
	const float& m_threshold = params.code_data->threshold;
	params.point =  global_params->image_recognition->compareImageReturnCentrePoint(params.frame, image_pool[m_image_path], m_threshold);
	if (!params.point.is_empty) return true;
	return false;
}

bool ARLTaskNode::textRecgnitionCallback(callbackParams& params)
{
	ar::error("Text Recognition Unsurpport Now !");
	std::exit(0);
	return false;
}

bool ARLTaskNode::timeOutCallback(callbackParams& params)
{
	if (params.cur_time_out >= params.code_data->time_out) return true;
	return false;
}

void ARLTaskNode::clickCallback(const callbackParams& params)
{
	int m_x = params.point.x + params.code_data->click_x;
	int m_y = params.point.y + params.code_data->click_y;
	global_params->controller->click(m_x, m_y);
}

void ARLTaskNode::swipeCallback(const callbackParams& params)
{
	global_params->controller->swipe(params.code_data->swipe_x_1, params.code_data->swipe_y_1, params.code_data->swipe_x_2, params.code_data->swipe_y_2, params.code_data->swipe_time);
}

void ARLTaskNode::longClickCallback(const callbackParams& params)
{
	global_params->controller->longClick(params.code_data->long_click_x, params.code_data->long_click_y, params.code_data->long_click_time);
}

void ARLTaskNode::inputKeyCallback(const callbackParams& params)
{
	global_params->controller->inputKey(params.code_data->key_code);
}

void ARLTaskNode::sleepCallback(const callbackParams& params)
{
	Sleep(params.code_data->sleep_time);
}

void ARLTaskNode::stopLoopCallback(const callbackParams& params)
{
	params.stop_condition = true;
}

void ARLTaskNode::returnCallback(const callbackParams& params)
{
	is_return = true;
}

void ARLTaskNode::exitCallback(const callbackParams& params)
{
	std::exit(0);
}

