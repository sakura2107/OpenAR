#pragma once
#include <ARLGlobalParams.h>
#include <Data/ARData.h>
#include <ARLTaskNode.h>
#include <queue>

class ARLTaskPipline {
public:
	ARLTaskPipline(ARLGlobalParams* global_params);
	~ARLTaskPipline() {}
public:
	void play();
	void push_back_task(ARTask* task_data);
private:
	ARLGlobalParams* global_params;
	std::queue<ARLTaskNode*> task_queue;
};