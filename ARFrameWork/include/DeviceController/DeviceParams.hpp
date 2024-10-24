#pragma once
#include <string>

namespace ar {

	enum struct ARDeviceError : int {
		AR_NO_ERROR,
		AR_INVALID_COMMAND,
		AR_INVALID_INDEX,
		AR_DEVICE_OFFLINE,
		AR_UNKNOWN_ERROR,
	};

	enum struct deviceControllerType {
		MUMU,
	};

	struct deviceInfo {
		std::string adb_host_ip = "0.0.0.0";
		unsigned int adb_port = 0;
		unsigned int index = 0;
		bool is_process_started = false;
		int error_code = -1;
		bool is_empty = true;
	};

	struct appInfo {
		bool is_running = false;
		bool is_install = false;
	};

}