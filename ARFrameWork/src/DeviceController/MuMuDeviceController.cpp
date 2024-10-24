#include <DeviceController/MuMuDeviceController.h>
#include <Cmd/CmdTools.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <Log/MiniLog.h>
#include <format>

constexpr std::string ar::MuMuDeviceController::getMuMuManagerPath(std::string path) {
    return path + "\\" + "shell" + "\\" + "MuMuManager.exe";
}

ar::ARDeviceError ar::MuMuDeviceController::checkIsCommandValid(std::string cmd_res) {
    if (cmd_res.find("{") == std::string::npos) return ar::ARDeviceError::AR_INVALID_COMMAND;
    else return ar::ARDeviceError::AR_NO_ERROR;
}

ar::MuMuDeviceController::MuMuDeviceController(const std::string mumu_path) : mumu_path(mumu_path){}

ar::ARDeviceError ar::MuMuDeviceController::getDeviceInfo(const int& index, ar::deviceInfo& info){
    ar::ARDeviceError err = ar::ARDeviceError::AR_NO_ERROR;
    std::string cmd_res;
    std::string cmd = std::format("{} {} {}", ar::MuMuDeviceController::getMuMuManagerPath(mumu_path), "info -v", index);
    ar::exec_cmd(cmd, cmd_res);
    err = ar::MuMuDeviceController::checkIsCommandValid(cmd_res);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Cmd invalid !");
        return ar::ARDeviceError::AR_INVALID_COMMAND;
    }
    rapidjson::Document d;
    d.Parse(cmd_res.c_str());
    info.error_code = d["error_code"].GetInt();
    if(info.error_code == -200) {
        ar::error("Invalid MuMu index: {}", index);
        return ar::ARDeviceError::AR_INVALID_INDEX;
    }
    info.is_empty = false;
    //get offline info
    info.is_process_started = false;
    info.index = index;
    //get online info
    if(d.HasMember("adb_host_ip")){
        info.is_process_started = true;
        info.adb_host_ip = d["adb_host_ip"].GetString();
        info.adb_port = d["adb_port"].GetUint();
    }
    return ar::ARDeviceError::AR_NO_ERROR;
}

ar::ARDeviceError ar::MuMuDeviceController::checkIsDeviceIndexValid(const int& index){
    ar::ARDeviceError err = ar::ARDeviceError::AR_NO_ERROR;
    bool state = false;
    std::string cmd_res;
    std::string cmd = std::format("{} {} {}", ar::MuMuDeviceController::getMuMuManagerPath(mumu_path), "info -v", index);
    ar::exec_cmd(cmd, cmd_res);
    err = ar::MuMuDeviceController::checkIsCommandValid(cmd_res);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Cmd invalid !");
        return ar::ARDeviceError::AR_INVALID_COMMAND;
    }
    rapidjson::Document d;
    d.Parse(cmd_res.c_str());
    if (d.HasMember("error_code") && d["error_code"] == 0) return ar::ARDeviceError::AR_NO_ERROR;
    return ar::ARDeviceError::AR_INVALID_INDEX;
}

ar::ARDeviceError ar::MuMuDeviceController::checkDeviceState(const int& index){
    ar::ARDeviceError err = ar::ARDeviceError::AR_NO_ERROR;
    bool state = false;
    std::string cmd_res;
    std::string cmd = std::format("{} {} {}", ar::MuMuDeviceController::getMuMuManagerPath(mumu_path), "info -v", index);
    err = checkIsDeviceIndexValid(index);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Index invalid !");
        return ar::ARDeviceError::AR_INVALID_COMMAND;
    }
    
    ar::exec_cmd(cmd, cmd_res);
    err = ar::MuMuDeviceController::checkIsCommandValid(cmd_res);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Cmd invalid !");
        return ar::ARDeviceError::AR_INVALID_COMMAND;
    }
    rapidjson::Document d;
    d.Parse(cmd_res.c_str());
    if(d["error_code"].GetInt() == -200) {
        ar::error("Invalid MuMu index: {}", index);
        std::exit(0);
    }
    state = d["is_process_started"].GetBool();
    if (state == true) return ar::ARDeviceError::AR_NO_ERROR;
    return ar::ARDeviceError::AR_DEVICE_OFFLINE;
}

ar::ARDeviceError ar::MuMuDeviceController::lauchDevice(const int& index){
    ar::ARDeviceError err = ar::ARDeviceError::AR_NO_ERROR;
    std::string cmd_res;
    std::string cmd = std::format("{} {} {} {}",
    ar::MuMuDeviceController::getMuMuManagerPath(mumu_path), "control -v", index, "launch");
    err = ar::MuMuDeviceController::checkIsDeviceIndexValid(index);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Invalid MuMu index : {}", index);
        return ar::ARDeviceError::AR_INVALID_INDEX;
    }
    ar::exec_cmd(cmd, cmd_res);
    err = ar::MuMuDeviceController::checkIsCommandValid(cmd_res);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Cmd invalid !");
        return ar::ARDeviceError::AR_INVALID_COMMAND;
    }

    rapidjson::Document d; d.Parse(cmd_res.c_str());
    if (d["errcode"].GetInt() != 0) {
        ar::error("Unknown error in MuMu {} !", index);
        return ar::ARDeviceError::AR_UNKNOWN_ERROR;
    }
    ar::info("Launch MuMu {}", index);
    return ar::ARDeviceError::AR_NO_ERROR;
}

ar::ARDeviceError ar::MuMuDeviceController::shutdownDevice(const int& index){
    ar::ARDeviceError err = ar::ARDeviceError::AR_NO_ERROR;
    std::string cmd_res;
    std::string cmd = std::format("{} {} {} {}",
    ar::MuMuDeviceController::getMuMuManagerPath(mumu_path), "control -v", index, "shutdown");
    err = ar::MuMuDeviceController::checkIsDeviceIndexValid(index);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Invalid MuMu index : {}", index);
        return ar::ARDeviceError::AR_INVALID_INDEX;
    }

    err = ar::MuMuDeviceController::checkDeviceState(index); 
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("MuMu index : {} offline !", index);
        return ar::ARDeviceError::AR_DEVICE_OFFLINE;
    }

    ar::exec_cmd(cmd, cmd_res);
    err = ar::MuMuDeviceController::checkIsCommandValid(cmd_res);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Cmd invalid !");
        return ar::ARDeviceError::AR_INVALID_COMMAND;
    }
    rapidjson::Document d; d.Parse(cmd_res.c_str());
    if (d["errcode"].GetInt() != 0) {
        ar::error("Unknown error in MuMu {} !", index);
        return ar::ARDeviceError::AR_UNKNOWN_ERROR;
    }
    ar::info("Shutdown MuMu {}", index);
    return ar::ARDeviceError::AR_NO_ERROR;
}

ar::ARDeviceError ar::MuMuDeviceController::restartDevice(const int& index){
    ar::ARDeviceError err = ar::ARDeviceError::AR_NO_ERROR;
    std::string cmd_res;
    std::string cmd = std::format("{} {} {} {}",
    ar::MuMuDeviceController::getMuMuManagerPath(mumu_path), "control -v", index, "restart");
    err = ar::MuMuDeviceController::checkIsDeviceIndexValid(index);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Invalid MuMu index : {}", index);
        return ar::ARDeviceError::AR_INVALID_INDEX;
    }
    ar::exec_cmd(cmd, cmd_res);
    err = ar::MuMuDeviceController::checkIsCommandValid(cmd_res);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Cmd invalid !");
        return ar::ARDeviceError::AR_INVALID_COMMAND;
    }
    rapidjson::Document d; d.Parse(cmd_res.c_str());
    if (d["errcode"].GetInt() != 0) {
        ar::error("Unknown error in MuMu {} !", index);
        return ar::ARDeviceError::AR_UNKNOWN_ERROR;
    }
    ar::info("Restart MuMu {}", index);
    return ar::ARDeviceError::AR_NO_ERROR;
}

ar::ARDeviceError ar::MuMuDeviceController::showWindow(const int& index){
    ar::ARDeviceError err = ar::ARDeviceError::AR_NO_ERROR;
    std::string cmd_res;
    std::string cmd = std::format("{} {} {} {}",
    ar::MuMuDeviceController::getMuMuManagerPath(mumu_path), "control -v", index, "showWindow");
    err = ar::MuMuDeviceController::checkIsDeviceIndexValid(index);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Invalid MuMu index : {}", index);
        return ar::ARDeviceError::AR_INVALID_INDEX;
    }
    err = ar::MuMuDeviceController::checkDeviceState(index);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("MuMu index : {} offline !", index);
        return ar::ARDeviceError::AR_DEVICE_OFFLINE;
    }
    ar::exec_cmd(cmd, cmd_res);
    err = ar::MuMuDeviceController::checkIsCommandValid(cmd_res);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Cmd invalid !");
        return ar::ARDeviceError::AR_INVALID_COMMAND;
    }
    rapidjson::Document d; d.Parse(cmd_res.c_str());
    if (d["errcode"].GetInt() != 0) {
        ar::error("Unknown error in MuMu {} !", index);
        return ar::ARDeviceError::AR_UNKNOWN_ERROR;
    }
    ar::info("Show MuMu {}", index);
    return ar::ARDeviceError::AR_NO_ERROR;
}

ar::ARDeviceError ar::MuMuDeviceController::hideWindow(const int& index){
    ar::ARDeviceError err = ar::ARDeviceError::AR_NO_ERROR;
    std::string cmd_res;
    std::string cmd = std::format("{} {} {} {}",
    ar::MuMuDeviceController::getMuMuManagerPath(mumu_path), "control -v", index, "hideWindow");
    err = ar::MuMuDeviceController::checkIsDeviceIndexValid(index);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Invalid MuMu index : {}", index);
        return ar::ARDeviceError::AR_INVALID_INDEX;
    }
    err = ar::MuMuDeviceController::checkDeviceState(index);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("MuMu index : {} offline !", index);
        return ar::ARDeviceError::AR_DEVICE_OFFLINE;
    }
    ar::exec_cmd(cmd, cmd_res);
    err = ar::MuMuDeviceController::checkIsCommandValid(cmd_res);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Cmd invalid !");
        return ar::ARDeviceError::AR_INVALID_COMMAND;
    }
    rapidjson::Document d; d.Parse(cmd_res.c_str());
    if (d["errcode"].GetInt() != 0) {
        ar::error("Unknown error in MuMu {} !", index);
        return ar::ARDeviceError::AR_UNKNOWN_ERROR;
    }
    ar::info("Hide MuMu {}", index);
    return ar::ARDeviceError::AR_NO_ERROR;
}

ar::ARDeviceError ar::MuMuDeviceController::lauchApp(const int& index, const std::string& app_name){
    ar::ARDeviceError err = ar::ARDeviceError::AR_NO_ERROR;
    std::string cmd_res;
    std::string cmd = std::format("{} {} {} {} {}",
    ar::MuMuDeviceController::getMuMuManagerPath(mumu_path), "control -v", index, "app launch -pkg", app_name);
    err = ar::MuMuDeviceController::checkIsDeviceIndexValid(index);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Invalid MuMu index : {}", index);
        return ar::ARDeviceError::AR_INVALID_INDEX;
    }
    err = ar::MuMuDeviceController::checkDeviceState(index);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("MuMu index : {} offline !", index);
        return ar::ARDeviceError::AR_DEVICE_OFFLINE;
    }
    ar::exec_cmd(cmd, cmd_res);
    //warning ! checkIsCommandValid will not check app_name !
    err = ar::MuMuDeviceController::checkIsCommandValid(cmd_res);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Cmd invalid !");
        return ar::ARDeviceError::AR_INVALID_COMMAND;
    }
    rapidjson::Document d; d.Parse(cmd_res.c_str());
    if (d["errcode"].GetInt() != 0) {
        ar::error("Unknown error in MuMu {} !", index);
        return ar::ARDeviceError::AR_UNKNOWN_ERROR;
    }
    ar::info("Launch {} in MuMu {}", app_name, index);
    return ar::ARDeviceError::AR_NO_ERROR;
}

ar::ARDeviceError ar::MuMuDeviceController::closeApp(const int& index, const std::string& app_name){
    ar::ARDeviceError err = ar::ARDeviceError::AR_NO_ERROR;
    std::string cmd_res;
    std::string cmd = std::format("{} {} {} {} {}",
    ar::MuMuDeviceController::getMuMuManagerPath(mumu_path), "control -v", index, "app close -pkg", app_name);
    err = ar::MuMuDeviceController::checkIsDeviceIndexValid(index);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Invalid MuMu index : {}", index);
        return ar::ARDeviceError::AR_INVALID_INDEX;
    }
    err = ar::MuMuDeviceController::checkDeviceState(index);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("MuMu index : {} offline !", index);
        return ar::ARDeviceError::AR_DEVICE_OFFLINE;
    }
    ar::exec_cmd(cmd, cmd_res);
    //warning ! checkIsCommandValid will not check app_name !
    err = ar::MuMuDeviceController::checkIsCommandValid(cmd_res);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Cmd invalid !");
        return ar::ARDeviceError::AR_INVALID_COMMAND;
    }
    rapidjson::Document d; d.Parse(cmd_res.c_str());
    if (d["errcode"].GetInt() != 0) {
        ar::error("Unknown error in MuMu {} !", index);
        return ar::ARDeviceError::AR_UNKNOWN_ERROR;
    }
    ar::info("Close {} in MuMu {}", app_name, index);
    return ar::ARDeviceError::AR_NO_ERROR;
}

ar::ARDeviceError ar::MuMuDeviceController::getAppState(const int& index, const std::string& app_name, appInfo& info){
    ar::ARDeviceError err = ar::ARDeviceError::AR_NO_ERROR;
    std::string cmd_res;
    std::string cmd = std::format("{} {} {} {} {}",
    ar::MuMuDeviceController::getMuMuManagerPath(mumu_path), "control -v", index, "app info -pkg", app_name);
    err = ar::MuMuDeviceController::checkIsDeviceIndexValid(index);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Invalid MuMu index : {}", index);
        return ar::ARDeviceError::AR_INVALID_INDEX;
    }
    err = ar::MuMuDeviceController::checkDeviceState(index); 
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("MuMu index : {} offline !", index);
        return ar::ARDeviceError::AR_DEVICE_OFFLINE;
    }
    ar::exec_cmd(cmd, cmd_res);
    err = ar::MuMuDeviceController::checkIsCommandValid(cmd_res);
    if (err != ar::ARDeviceError::AR_NO_ERROR) {
        ar::error("Cmd invalid !");
        return ar::ARDeviceError::AR_INVALID_COMMAND;
    }
    rapidjson::Document d;
    d.Parse(cmd_res.c_str());
    std::string state_res = d["state"].GetString();
    if(state_res == "running") {info.is_install = true; info.is_running = true;}
    if(state_res == "stopped") {info.is_install = true; info.is_running = false;}
    if(state_res == "not_installed") {info.is_install = false; info.is_running = false;}
    return ar::ARDeviceError::AR_NO_ERROR;
}