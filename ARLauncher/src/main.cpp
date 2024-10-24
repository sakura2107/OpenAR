#include <ARLTaskPipline.h>
#include <ARLGlobalParams.h>
#include <Data/ARJson.h>
#include <filesystem>

int main() {
	//��ʼ��project��Դ
	ARProject* m_project_data = nullptr;
	//��ʼ��json�ļ�
	std::filesystem::path dir = "./"; //Ŀǰֻ��������ǰĿ¼�µ�json�ļ�
	std::string json_dir;
	for (const auto& entry : std::filesystem::directory_iterator(dir)) {
		if (entry.is_regular_file() && entry.path().extension() == ".json") {
			json_dir = entry.path().string();
		}
	}
	//��json�ļ�����project��Դ
	ar::loadJsonFile(json_dir, &m_project_data);
	//��ʼ��ȫ�ֲ���
	ARLGlobalParams global_params(m_project_data);
	//��ʼ���������
	ARLTaskPipline m_task_pipline(&global_params);
	//���ν������������
	for (int i = 0; i < m_project_data->Size(); i++) {
		m_task_pipline.push_back_task(m_project_data->getPtr(i));
	}
	//��˳��ִ�й�������
	m_task_pipline.play();
	//�˳�
	std::system("pause");
	return 0;
}