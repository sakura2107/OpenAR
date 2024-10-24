#include <ARLTaskPipline.h>

ARLTaskPipline::ARLTaskPipline(ARLGlobalParams* global_params) : global_params(global_params)
{}

void ARLTaskPipline::play()
{
	while (!task_queue.empty()) {
		ARLTaskNode* m_task_node = task_queue.front();
		m_task_node->play();
		task_queue.pop();
	}
}

void ARLTaskPipline::push_back_task(ARTask* task_data)
{
	ARLTaskNode* m_task_node = new ARLTaskNode(global_params, task_data);
	task_queue.push(m_task_node);
}