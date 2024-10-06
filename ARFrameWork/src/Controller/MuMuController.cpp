#include <Controller/MuMuController.h>
#include <Log/MiniLog.h>
#include <Cmd/CmdTools.hpp>
#include <format>

#define LOAD_LIBRARY(name) LoadLibraryW(name)

#define GET_PROC_ADDRESS(dllHandle, name) GetProcAddress(dllHandle, name)

#define UNLOAD_LIBRARY(dllHandle) FreeLibrary(dllHandle)

#define LOAD_FUNCTION(dllHandle, funcName, funcType) \
    funcType funcName = (funcType)GET_PROC_ADDRESS(dllHandle, #funcName);

#define CALL_FUNC(funcName, ...) \
    funcName(__VA_ARGS__)

#define ADB_EXE_PATH ".\\adb\\adb.exe"

#define MUMUCONTROLLER_DLL_PATH L"./external_renderer_ipc.dll"

ar::MuMuController::MuMuController(const std::string adb_path, const int adb_port, const wchar_t* mumu_path, const int index) :
    adb_path(adb_path),
    adb_port(adb_port),
    mumu_path(mumu_path) ,                                                                    
    index(index){}

ar::MuMuController::~MuMuController(){
    disconnect();
}

bool ar::MuMuController::initialize(){
    bool err = false;
    std::string cmd_res;
    std::string cmd = std::format("{} {} {}:{}", ADB_EXE_PATH, "connect", adb_path, adb_port);
    ar::debug("{}", cmd);
    exec_cmd(cmd, cmd_res);
    if (cmd_res.find("(10016)") != std::string::npos || cmd_res.find("error") != std::string::npos) {
        ar::error("Cannot connect {}:{} .Please check this host !", adb_path, adb_port);
        return false;
    }
    hDLL = LOAD_LIBRARY(MUMUCONTROLLER_DLL_PATH);
    if (hDLL == NULL) {
        ar::error("Cannot find external_renderer_ipc.dll");
        return false;
    }
    err = connect();
    if (!err) {
        ar::error("Initialize fail !");
        return false;
    }
    LOAD_FUNCTION(hDLL, nemu_capture_display, NemuCaptureFunc);
    if(CALL_FUNC(nemu_capture_display, handle, 0, 0, &window_width, &window_height, nullptr) != 0){
        ar::error("Get screencap pixels failed!");
        return false;
    }
    is_initialize = true;
    return true;
}

bool ar::MuMuController::connect(){
    LOAD_FUNCTION(hDLL, nemu_connect, NemuConnectFunc);
    handle = CALL_FUNC(nemu_connect, mumu_path, index);
    if(handle == 0){
        ar::error("MuMu {} connect fail !", index);
        return false;
    }
    return true;
}

bool ar::MuMuController::disconnect(){
    bool err = false;
    if (!is_initialize) return false;
    LOAD_FUNCTION(hDLL, nemu_disconnect, NemuDisconnectFunc);
    CALL_FUNC(nemu_disconnect, handle);
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
    UNLOAD_LIBRARY(hDLL);
    return true;
}

bool ar::MuMuController::click(const int x, const int y){
    bool err = false;
    if (!is_initialize) {
        err = initialize();
        if (!err) return false;
    }
    LOAD_FUNCTION(hDLL, nemu_input_event_touch_down, NemuTouchDownFunc);
    LOAD_FUNCTION(hDLL, nemu_input_event_touch_up, NemuTouchUpFunc);
    int x_ = window_height - y, y_ = x;
    if(CALL_FUNC(nemu_input_event_touch_down, handle, 0, x_, y_) ||
    CALL_FUNC(nemu_input_event_touch_up, handle, 0)){
    ar::error("Mouse click at {},{} false!", x, y);
    return false;
    }
    ar::debug("Mouse click at {},{}", x, y);
    return true;
}

bool ar::MuMuController::longClick(const int x, const int y, const int duration){
    bool err = false;
    if (!is_initialize) {
        err = initialize();
        if (!err) return false;
    }
    LOAD_FUNCTION(hDLL, nemu_input_event_touch_down, NemuTouchDownFunc);
    LOAD_FUNCTION(hDLL, nemu_input_event_touch_up, NemuTouchUpFunc);
    int x_ = window_height - y, y_ = x;
    if(CALL_FUNC(nemu_input_event_touch_down, handle, 0, x_, y_)){
        ar::error("Mouse long click at {},{} {}ms false!", x, y, duration);
        return false;
    }
    Sleep(duration);
    if(CALL_FUNC(nemu_input_event_touch_up, handle, 0)){
        ar::error("Mouse long click at {},{} {}ms false!", x_, y_, duration);
        return false;
    }
    return true;
}

bool ar::MuMuController::inputKey(const int key_code){
    bool err = false;
    if (!is_initialize) {
        err = initialize();
        if (!err) return false;
    }
    LOAD_FUNCTION(hDLL, nemu_input_event_key_down, NemuKeyDownFunc);
    LOAD_FUNCTION(hDLL, nemu_input_event_key_up, NemuKeyUpFunc);
    if(CALL_FUNC(nemu_input_event_key_down, handle, 0, key_code)||
    CALL_FUNC(nemu_input_event_key_up, handle, 0, key_code)  ){
        ar::error("Input key {} false!", key_code);
        return false;
    }
    return true;
}

bool ar::MuMuController::screencap(cv::Mat& image_){
    bool err = false;
    if (!is_initialize) {
        err = initialize();
        if (!err) return false;
    }
    LOAD_FUNCTION(hDLL, nemu_capture_display, NemuCaptureFunc);
    std::vector<uchar> img_data;
    img_data.resize(window_width * window_height * 4);
    if(CALL_FUNC(nemu_capture_display, handle, 0, static_cast<int>(img_data.size()), &window_width, &window_height, img_data.data()) != 0){
        ar::error("Get screencap pixels failed!");
        return false;
    }
    cv::Mat dst_img(window_height, window_width, CV_8UC4, img_data.data());
    cv::cvtColor(dst_img, dst_img, cv::COLOR_RGBA2BGR);
    cv::flip(dst_img, dst_img, 0);
    image_ = dst_img.clone();
    return true;
}

bool ar::MuMuController::swipe(const int x_1, const int y_1, const int x_2, const int y_2, const int ms_duration){
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