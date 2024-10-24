#pragma once
#include <string>

namespace ar {

	enum class controllerType {
		ADB,
		MUMU,
	};

	struct controllerParams {
		//adb
		std::string adb_path = "";
		int adb_port = -1;
		//mumu
		const wchar_t* mumu_path = L"";
		int mumu_index = -1;
	};

}