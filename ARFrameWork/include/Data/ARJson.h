#pragma once
#include <string>
#include <Data/ARData.h>

namespace ar {

	bool loadJsonFile(const std::string& path, ARProject** p_project_data);

	bool saveJsonFile(const std::string& path, ARProject* p_project_data);

}

