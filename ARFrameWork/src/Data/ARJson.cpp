#include <Data/ARJson.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>
#include <sstream>
#include <cstdint>

bool ar::loadJsonFile(const std::string& path, ARProject** p_project_data)
{
	std::fstream file(path);
	if (!file.is_open()) {
		return false;
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string jsonBuffer = buffer.str();
	file.close();
	rapidjson::Document project_data;
	project_data.Parse(jsonBuffer.c_str());
	//Load project data
	*p_project_data = new ARProject;
	(*p_project_data)->project_id = project_data["project_id"].GetInt();
	(*p_project_data)->project_name = project_data["project_name"].GetString();
	(*p_project_data)->adb_path = project_data["adb_path"].GetString();
	(*p_project_data)->adb_port = project_data["adb_port"].GetInt();
	(*p_project_data)->device_type = project_data["device_type"].GetInt();
	(*p_project_data)->controller_type = project_data["controller_type"].GetInt();
	(*p_project_data)->image_recognition_type = project_data["image_recognition_type"].GetInt();
	(*p_project_data)->device_path = project_data["device_path"].GetString();
	(*p_project_data)->device_index = project_data["device_index"].GetInt();
	(*p_project_data)->duration_time = project_data["duration_time"].GetInt();
	//Laod task data
	rapidjson::Value& task_data_array = project_data["task_data"];
	for (rapidjson::SizeType i = 0; i < task_data_array.Size(); i++) {
		rapidjson::Value& task_data = task_data_array[i];
		(*p_project_data)->push_back();
		ARTask* p_task_data = (*p_project_data)->getPtr(i);
		p_task_data->task_id = task_data["task_id"].GetInt();
		p_task_data->task_name = task_data["task_name"].GetString();
		//Load code block data
		rapidjson::Value& block_data_array = task_data["code_block_data"];
		for (rapidjson::SizeType j = 0; j < block_data_array.Size(); j++) {
			rapidjson::Value& code_block_data = block_data_array[j];
			p_task_data->push_back();
			ARBlock* p_block_data = p_task_data->getPtr(j);
			p_block_data->block_id = code_block_data["code_block_id"].GetInt();
			p_block_data->block_name = code_block_data["code_block_name"].GetString();
			//Load code data
			rapidjson::Value& code_data_array = code_block_data["code_data_array"];
			for (rapidjson::SizeType k = 0; k < code_data_array.Size(); k++) {
				rapidjson::Value& code_data = code_data_array[k];
				p_block_data->push_back();
				ARCode* p_code_data = p_block_data->getPtr(k);
				p_code_data->code_id = code_data["code_id"].GetInt();
				p_code_data->first_value = code_data["first_value"].GetInt();
				p_code_data->image_path = code_data["image_path"].GetString();
				p_code_data->threshold = code_data["threshold"].GetFloat();
				p_code_data->text = code_data["text"].GetString();
				p_code_data->time_out = code_data["time_out"].GetInt();
				p_code_data->second_value = code_data["second_value"].GetInt();
				p_code_data->click_x = code_data["click_x"].GetInt();
				p_code_data->click_y = code_data["click_y"].GetInt();
				p_code_data->swipe_x_1 = code_data["swipe_x_1"].GetInt();
				p_code_data->swipe_x_2 = code_data["swipe_x_2"].GetInt();
				p_code_data->swipe_y_1 = code_data["swipe_y_1"].GetInt();
				p_code_data->swipe_y_2 = code_data["swipe_y_2"].GetInt();
				p_code_data->swipe_time = code_data["swipe_time"].GetInt();
				p_code_data->long_click_x = code_data["long_click_x"].GetInt();
				p_code_data->long_click_y = code_data["long_click_y"].GetInt();
				p_code_data->long_click_time = code_data["long_click_time"].GetInt();
				p_code_data->key_code = code_data["key_code"].GetInt();
			}
		}
	}
	return true;
}

bool ar::saveJsonFile(const std::string& path, ARProject* p_project_data)
{
	rapidjson::Document project_data;
	project_data.SetObject();

	rapidjson::Document::AllocatorType& allocator = project_data.GetAllocator();

	project_data.AddMember("project_id", p_project_data->project_id, allocator);
	project_data.AddMember("project_name", rapidjson::Value(p_project_data->project_name.c_str(), allocator), allocator);
	project_data.AddMember("adb_path", rapidjson::Value(p_project_data->adb_path.c_str(), allocator), allocator);
	project_data.AddMember("adb_port", p_project_data->adb_port, allocator);
	project_data.AddMember("device_type", p_project_data->device_type, allocator);
	project_data.AddMember("controller_type", p_project_data->controller_type, allocator);
	project_data.AddMember("image_recognition_type", p_project_data->image_recognition_type, allocator);
	project_data.AddMember("device_path", rapidjson::Value(p_project_data->device_path.c_str(), allocator), allocator);
	project_data.AddMember("device_index", p_project_data->device_index, allocator);
	project_data.AddMember("duration_time", p_project_data->duration_time, allocator);

	rapidjson::Value task_data_array(rapidjson::kArrayType);
	for (int i = 0; i < p_project_data->Size(); ++i) {
		ARTask* p_task_data = p_project_data->getPtr(i);

		rapidjson::Value task_data(rapidjson::kObjectType);
		task_data.AddMember("task_id", p_task_data->task_id, allocator);
		task_data.AddMember("task_name", rapidjson::Value(p_task_data->task_name.c_str(), allocator), allocator);

		rapidjson::Value block_data_array(rapidjson::kArrayType);
		for (int j = 0; j < p_task_data->Size(); ++j) {
			ARBlock* p_block_data = p_task_data->getPtr(j);

			rapidjson::Value code_block_data(rapidjson::kObjectType);
			code_block_data.AddMember("code_block_id", p_block_data->block_id, allocator);
			code_block_data.AddMember("code_block_name", rapidjson::Value(p_block_data->block_name.c_str(), allocator), allocator);

			rapidjson::Value code_data_array(rapidjson::kArrayType);
			for (int k = 0; k < p_block_data->Size(); ++k) {
				ARCode* p_code_data = p_block_data->getPtr(k);

				rapidjson::Value code_data(rapidjson::kObjectType);
				code_data.AddMember("code_id", p_code_data->code_id, allocator);
				code_data.AddMember("first_value", p_code_data->first_value, allocator);
				code_data.AddMember("image_path", rapidjson::Value(p_code_data->image_path.c_str(), allocator), allocator);
				code_data.AddMember("threshold", p_code_data->threshold, allocator);
				code_data.AddMember("text", rapidjson::Value(p_code_data->text.c_str(), allocator), allocator);
				code_data.AddMember("time_out", p_code_data->time_out, allocator);
				code_data.AddMember("second_value", p_code_data->second_value, allocator);
				code_data.AddMember("click_x", p_code_data->click_x, allocator);
				code_data.AddMember("click_y", p_code_data->click_y, allocator);
				code_data.AddMember("swipe_x_1", p_code_data->swipe_x_1, allocator);
				code_data.AddMember("swipe_x_2", p_code_data->swipe_x_2, allocator);
				code_data.AddMember("swipe_y_1", p_code_data->swipe_y_1, allocator);
				code_data.AddMember("swipe_y_2", p_code_data->swipe_y_2, allocator);
				code_data.AddMember("swipe_time", p_code_data->swipe_time, allocator);
				code_data.AddMember("long_click_x", p_code_data->long_click_x, allocator);
				code_data.AddMember("long_click_y", p_code_data->long_click_y, allocator);
				code_data.AddMember("long_click_time", p_code_data->long_click_time, allocator);
				code_data.AddMember("key_code", p_code_data->key_code, allocator);

				code_data_array.PushBack(code_data, allocator);
			}

			code_block_data.AddMember("code_data_array", code_data_array, allocator);

			block_data_array.PushBack(code_block_data, allocator);
		}

		task_data.AddMember("code_block_data", block_data_array, allocator);

		task_data_array.PushBack(task_data, allocator);
	}

	project_data.AddMember("task_data", task_data_array, allocator);

	std::ofstream ofs(path);
	if (!ofs.is_open()) {
		return false;
	}

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	project_data.Accept(writer);

	std::ofstream file(path);
	if (!file.is_open()) {
		return false;
	}
	file << buffer.GetString();
	file.close();

	return true;
}