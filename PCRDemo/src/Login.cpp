#include <Login.h>
#include <define.hpp>

void pcr::login(pcr::global_params& params)
{
    LOAD_IMAGE(skip, "res/pcr/skip.png");
    LOAD_IMAGE(confirm_blue, "res/pcr/confirm_blue.png");
    LOAD_IMAGE(close_white, "res/pcr/close_white.png");
    LOAD_IMAGE(main_line, "res/pcr/main_line.png");
    LOAD_IMAGE(download, "res/pcr/download.png");

    bool stop_condition = false;
    int times = 0;
    ar::info("Start launch pcr !");

    stop_condition = false; times = 0; 
    do {
        Sleep(params.operate_duration); times++; if (times > params.run_max_times) {
            ar::error("Runtime too long !"); std::exit(0);
        } 
        cv::Mat frame; 
        params.controller->screencap(frame); 
        if (frame.empty()) {
            ar::error("Frame empty !");
            params.controller->disconnect();
            std::exit(0);
        }
        params.controller->click(804, 890); 
        ar::point p_skip = params.image_recognition->compareImageReturnCentrePoint(frame, skip, 0.95f); 
        if (!p_skip.is_empty) params.controller->click(p_skip.x, p_skip.y); 
        ar::point p_confirm_blue = params.image_recognition->compareImageReturnCentrePoint(frame, confirm_blue, 0.95f); 
        if (!p_confirm_blue.is_empty) params.controller->click(p_confirm_blue.x, p_confirm_blue.y); 
        ar::point p_close_white = params.image_recognition->compareImageReturnCentrePoint(frame, close_white, 0.95f); 
        if (!p_close_white.is_empty) params.controller->click(p_close_white.x, p_close_white.y); 
        ar::point p_download = params.image_recognition->compareImageReturnCentrePoint(frame, download, 0.95f); 
        if (!p_download.is_empty) params.controller->click(p_download.x, p_download.y); 
        ar::point p_main_line = params.image_recognition->compareImageReturnCentrePoint(frame, main_line, 0.95f); 
        if (!p_main_line.is_empty) stop_condition = true;
    } while (!stop_condition);
}