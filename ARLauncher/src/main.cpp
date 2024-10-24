#include <ARLTaskPipline.h>
#include <ARLGlobalParams.h>
#include <Data/ARJson.h>
#include <filesystem>

int main() {
	//初始化project资源
	ARProject* m_project_data = nullptr;
	//初始化json文件
	std::filesystem::path dir = "./"; //目前只会搜索当前目录下的json文件
	std::string json_dir;
	for (const auto& entry : std::filesystem::directory_iterator(dir)) {
		if (entry.is_regular_file() && entry.path().extension() == ".json") {
			json_dir = entry.path().string();
		}
	}
	//从json文件读入project资源
	ar::loadJsonFile(json_dir, &m_project_data);
	//初始化全局参数
	ARLGlobalParams global_params(m_project_data);
	//初始化任务管线
	ARLTaskPipline m_task_pipline(&global_params);
	//依次将任务载入管线
	for (int i = 0; i < m_project_data->Size(); i++) {
		m_task_pipline.push_back_task(m_project_data->getPtr(i));
	}
	//按顺序执行管线数据
	m_task_pipline.play();
	//退出
	std::system("pause");
	return 0;
}