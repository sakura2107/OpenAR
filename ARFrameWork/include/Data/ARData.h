#pragma once
#include <cstdint>
#include <string>
#include <vector>

class ARProject;
class ARTask;
class ARBlock;
class ARCode;

class ARProject {
public:
	ARProject() {};
	~ARProject();
public:
	int project_id = 0;
	std::string project_name = "New Project";
	std::string adb_path = "127.0.0.1";
	int adb_port = 5555;
	int device_type = 0;
	int controller_type = 0;
	int image_recognition_type = 0;
	std::string device_path = "";
	int device_index = 0;
	int duration_time = 200;
private:
	std::vector<ARTask*> task_array;
	int size = 0;
public:
	void push_back();
	void pop_back();
	void insert(const int& loc);
	void erase(const int& loc);
	void clear();
	bool empty() const;
	int Size() const;
	ARTask* getPtr(const int& loc);
	void swap(const int& first_loc, const int& second_loc);
};

class ARTask {
public:
	ARTask() {};
	~ARTask();
public:
	int task_id = 0;
	std::string task_name = "";
private:
	std::vector<ARBlock*> block_array;
	int size = 0;
public:
	void push_back();
	void pop_back();
	void insert(const int& loc);
	void erase(const int& loc);
	void clear();
	bool empty() const;
	int Size() const;
	ARBlock* getPtr(const int& loc);
	void swap(const int& first_loc, const int& second_loc);
};

class ARBlock {
public:
	ARBlock() {};
	~ARBlock();
public:
	int block_id = 0;
	std::string block_name = "Add Title Here";
private:
	std::vector<ARCode*> code_array;
	int size = 0;
public:
	void push_back();
	void pop_back();
	void insert(const int& loc);
	void erase(const int& loc);
	void clear();
	bool empty() const;
	int Size() const;
	ARCode* getPtr(const int& loc);
	void swap(const int& first_loc, const int& second_loc);
};

class ARCode {
public:
	int code_id = 0;
	int first_value = 0;

	std::string image_path = ".\\Image Path";
	float threshold = 0.95f;

	std::string text = "";

	int second_value = 0;

	int time_out = 0;

	int click_x = 0;
	int click_y = 0;

	int swipe_x_1 = 0;
	int swipe_y_1 = 0;
	int swipe_x_2 = 0;
	int swipe_y_2 = 0;
	int swipe_time = 0;

	int long_click_x = 0;
	int long_click_y = 0;
	int long_click_time = 0;

	int key_code = 0;

	int sleep_time = 0;
};