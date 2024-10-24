#pragma once

#include <Windows.h>
#include <string>
#include <Controller/Controller.h>

namespace ar{

class image{
public:
    std::vector<unsigned char> data;
    int height;
    int width;
};

class MuMuController : public Controller{
private:
    const std::string adb_path;
    const int adb_port;
    const wchar_t* mumu_path;
    const int index;
    bool is_initialize = false;
    int handle = 0;
    int window_height = 0, window_width = 0;
    HINSTANCE hDLL;
    typedef int (*NemuConnectFunc)(const wchar_t*, int);
    typedef int (*NemuCaptureFunc)(int, unsigned int, int, int*, int*, unsigned char*);
    typedef int (*NemuTouchDownFunc)(int, int, int, int);
    typedef int (*NemuTouchUpFunc)(int, int);
    typedef void (*NemuDisconnectFunc)(int);
    typedef int (*NemuKeyDownFunc)(int, int, int);
    typedef int (*NemuKeyUpFunc)(int, int, int);
public:
    ~MuMuController();
    MuMuController(const std::string adb_path, const int adb_port, const wchar_t* mumu_path, const int index);
    bool initialize() override;
    bool connect() override;
    bool disconnect() override;
    bool click(const int x, const int y) override;
    bool longClick(const int x,const int y, const int duration) override;
    bool inputKey(const int key_code) override;
    //swipe cannot use currently
    bool swipe(const int x_1, const int y_1, const int x_2, const int y_2, const int ms_duration) override;
    bool screencap(cv::Mat& image_) override;
};
}