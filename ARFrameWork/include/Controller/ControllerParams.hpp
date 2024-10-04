#pragma once
#include <string>

namespace ar {

	enum class controllerType {
		ADB,
		MUMU,
	};

	struct controllerParams {
		std::string adb_path = "";
		int adb_port = -1;
		const wchar_t* mumu_path = L"";
		int mumu_index = -1;
	};

}