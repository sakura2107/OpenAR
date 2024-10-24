#include <ARLGlobalParams.h>
#include <Controller/Controller.h>
#include <stdlib.h>
#include <Log/MiniLog.h>

ARLGlobalParams::ARLGlobalParams(ARProject* project_data) :
	project_data(project_data) 
{
	Init();
}

void ARLGlobalParams::Init()
{
	if (project_data == nullptr) {
		ar::error("Init Fail !");
		std::exit(0);
	}

	device_path = project_data->device_path;
	index = project_data->device_index;
	adb_path = project_data->adb_path;
	adb_port = project_data->adb_port;

	if (project_data->controller_type == 0) controller_type = ar::controllerType::ADB;
	if (project_data->controller_type == 1) controller_type = ar::controllerType::MUMU;

	if (project_data->image_recognition_type == 0) image_recognition_type = ar::imageRecognitionType::PSR;
	if (project_data->image_recognition_type == 1) image_recognition_type = ar::imageRecognitionType::MPR;
#if defined(ENABLE_CUDA)
	if (project_data->image_recognition_type == 2) image_recognition_type = ar::imageRecognitionType::PSR;
#endif
	
	if (project_data->device_type == 1) device_controller_type = ar::deviceControllerType::MUMU;

	duration_time = project_data->duration_time;

	//Init Controller
	ar::ControllerFactory controller_factory;
	ar::controllerParams controller_params;
	switch (controller_type) {
	case(ar::controllerType::ADB) :
		controller_params.adb_path = adb_path;
		controller_params.adb_port = adb_port;
		break;
	case(ar::controllerType::MUMU):
		controller_params.adb_path = adb_path;
		controller_params.adb_port = adb_port;
		size_t wchar_size = strlen(device_path.c_str()) + 1;
		wchar_t* w_device_path = new wchar_t[wchar_size];
		size_t convertedChars = 0;
		mbstowcs_s(&convertedChars, w_device_path, wchar_size, device_path.c_str(), _TRUNCATE);
		controller_params.mumu_path = w_device_path;
		controller_params.mumu_index = index;
		break;
	}
	controller = controller_factory.createController(controller_params, controller_type);

	//Init Device Controller
	ar::DeviceControllerFactory device_controller_factory;
	device_controller = device_controller_factory.createDeviceController(device_controller_type, device_path);

	//Init Image Recognition
	ar::ImageRecognitionFactory image_recognition_factory;
	image_recognition = image_recognition_factory.createIamgeRecognition(image_recognition_type);
}
