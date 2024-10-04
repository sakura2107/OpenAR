#include <Controller/AdbController.h>
#include <format>
#include <Log/MiniLog.h>
#include <Cmd/CmdTools.hpp>
#include <opencv2/opencv.hpp>

#define ADB_EXE_PATH ".\\adb\\adb.exe"

ar::AdbController::AdbController(std::string adb_path, int adb_port) {
	this->adb_path = adb_path;
	this->adb_port = adb_port;
}

ar::AdbController::~AdbController() {
	disconnect();
}

bool ar::AdbController::initialize() {
	bool err = false;
	err = connect();
	if (!err) return false;
	is_initialize = true;
	return true;
}

bool ar::AdbController::connect() {
	//commmand: .\adb\adb.exe connect 127.0.0.1:xxxxx 
	bool err = false;
	std::string cmd_res;
	std::string cmd = std::format("{} {} {}:{}", ADB_EXE_PATH, "connect", adb_path, adb_port);
	ar::debug("{}", cmd);
	exec_cmd(cmd, cmd_res);
	if (cmd_res.find("(10016)") != std::string::npos || cmd_res.find("error") != std::string::npos) {
		ar::error("Cannot connect {}:{} .Please check this host !", adb_path, adb_port);
		return false;
	}
	return true;
}

bool ar::AdbController::disconnect() {
	//command .\adb\adb.exe disconnect 127.0.0.1:xxxxx
	//command .\adb\adb.exe kill-server
	bool err = false;
	if (!is_initialize) return false;
	std::string cmd_res;
	std::string cmd = std::format("{} {} {}:{}", ADB_EXE_PATH, "disconnect", adb_path, adb_port);
	ar::debug("{}", cmd);
	exec_cmd(cmd, cmd_res);
	if (cmd_res.find("error") != std::string::npos) {
		ar::error("Cannot disconnect {}:{} .Please check this host !", adb_path, adb_port);
		return false;
	}
	cmd = std::format("{} {}", ADB_EXE_PATH, "kill-server");
	ar::debug("{}", cmd);
	exec_cmd(cmd, cmd_res);
	cmd = std::format("{} {}", "del", ".\\screen.png");
	ar::debug("{}", cmd);
	exec_cmd(cmd, cmd_res);
	return true;
}

bool ar::AdbController::click(const int x, const int y) {
	//command .\adb\adb.exe -s 127.0.0.1:xxxxx shell input tap x y
	bool err = false;
	if (!is_initialize) {
		err = initialize();
		if (!err) return false;
	}
	std::string cmd_res;
	std::string cmd = std::format("{} {} {}:{} {} {} {} {} {}", ADB_EXE_PATH, "-s", adb_path, adb_port, "shell", "input", "tap", x, y);
	ar::debug("{}", cmd);
	exec_cmd(cmd, cmd_res);
	if (cmd_res.find("not found") != std::string::npos) {
		ar::error("Click x:{} y:{} fail !", x, y);
		return false;
	}
	return true;
}

bool ar::AdbController::longClick(const int x, const int y, const int ms_duration) {
	//command .\adb\adb.exe -s 127.0.0.1:xxxxx shell input swipe x y x y ms_duration
	bool err = false;
	if (!is_initialize) {
		err = initialize();
		if (!err) return false;
	}
	std::string cmd_res;
	std::string cmd = std::format("{} {} {}:{} {} {} {} {} {} {} {} {}",
									ADB_EXE_PATH,
									"-s",
									adb_path,adb_port,
									"shell",
									"input",
									"swipe",
									x, y, x, y, ms_duration);
	ar::debug("{}", cmd);
	exec_cmd(cmd, cmd_res);
	if (cmd_res.find("not found") != std::string::npos) {
		ar::error("Long Click x:{} y:{} fail !", x, y);
		return false;
	}
	return true;
}

bool ar::AdbController::inputKey(const int key_code) {
	//command .\adb\adb.exe -s 127.0.0.1:xxxxx shell input keyevent key_code
	bool err = false;
	if (!is_initialize) {
		err = initialize();
		if (!err) return false;
	}
	std::string cmd_res;
	std::string cmd = std::format("{} {} {}:{} {} {} {} {}", ADB_EXE_PATH, "-s", adb_path, adb_port, "shell", "input", "keyevent", key_code);
	ar::debug("{}", cmd);
	exec_cmd(cmd, cmd_res);
	if (!err) return false;
	if (cmd_res.find("not found") != std::string::npos) {
		ar::error("Input Key {} fail !", key_code);
		return false;
	}
	return true;
}

bool ar::AdbController::swipe(const int x_1, const int y_1, const int x_2, const int y_2, const int ms_duration) {
	//command .\adb\adb.exe -s 127.0.0.1:xxxxx shell input swipe x_1 y_1 x_2 y_2 ms_duration
	bool err = false;
	if (!is_initialize) {
		err = initialize();
		if (!err) return false;
	}
	std::string cmd_res;
	std::string cmd = std::format("{} {} {}:{} {} {} {} {} {} {} {} {}",
		ADB_EXE_PATH,
		"-s",
		adb_path, adb_port,
		"shell",
		"input",
		"swipe",
		x_1, y_1, x_2, y_2, ms_duration);
	ar::debug("{}", cmd);
	exec_cmd(cmd, cmd_res);
	if (cmd_res.find("not found") != std::string::npos) {
		ar::error("Swipe from x_1:{} y_1:{} to x_2:{} y_2:{} fail !", x_1, y_1, x_2, y_2);
		return false;
	}
	return true;
}

bool ar::AdbController::screencap(cv::Mat& image_) {
	//command one .\adb\adb.exe -s 127.0.0.1:xxxxx shell screencap /data/screen.png
	//command two .\adb\adb.exe -s 127.0.0.1:xxxxx pull /data/screen.png .
	bool err = false;
	if (!is_initialize) {
		err = initialize();
		if (!err) return false;
	}
	std::string cmd_res;
	std::string cmd = std::format("{} {} {}:{} {} {} {}",
		ADB_EXE_PATH,
		"-s",
		adb_path, adb_port,
		"shell",
		"screencap",
		"/sdcard/screen.png");
	ar::debug("{}", cmd);
	exec_cmd(cmd, cmd_res);
	if (cmd_res.find("not found") != std::string::npos) {
		ar::error("Screencap fail !");
		return false;
	}
	cmd = std::format("{} {} {}:{} {} {} {}",
		ADB_EXE_PATH,
		"-s",
		adb_path, adb_port,
		"pull",
		"/sdcard/screen.png",
		".\\");
	ar::debug("{}", cmd);
	exec_cmd(cmd, cmd_res);
	if (cmd_res.find("not found") != std::string::npos) {
		ar::error("Screencap fail !");
		return false;
	}
	image_ = cv::imread("./screen.png");
	if (image_.empty()) {
		ar::error("Image empty in {} !", "./screen.png");
		return false;
	}
	return true;
}