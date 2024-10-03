#include <MainLineVeryHard.h>
#include <define.hpp>

void pcr::main_line_veryhard(pcr::global_params params, pcr::main_line_veryhard_params veryhard_params)
{
    LOAD_IMAGE(adventure, "./res/pcr/adventure.png");
    LOAD_IMAGE(main_line, "./res/pcr/main_line.png");
    LOAD_IMAGE(sweep, "./res/pcr/sweep.png");
    LOAD_IMAGE(multi_drop, "./res/pcr/multi_drop.png");
    LOAD_IMAGE(veryhard_on, "./res/pcr/veryhard_on.png");
    LOAD_IMAGE(veryhard_off, "./res/pcr/veryhard_off.png");

    bool is_auto_execute = false;

    bool stop_condition = false;
    int times = 0;

    ar::info("Enter venture");

    stop_condition = false; times = 0;
    do {
        Sleep(params.operate_duration); times++;
        if (times > params.run_max_times) {
            ar::error("Runtime too long !");
            std::exit(0);
        } 
        
        cv::Mat frame; 
        params.controller->screencap(frame); 
        if (frame.empty()) {
            ar::error("Frame empty !");
            params.controller->disconnect();
            std::exit(0);
        } 
        
        params.controller->click(800, 875); 
        ar::point p_main_line = params.image_recognition->compareImageReturnCentrePoint(frame, main_line, 0.95f);
        if (!p_main_line.is_empty) stop_condition = true;
    } while (!stop_condition);

    ar::info("Enter venture");
    stop_condition = false; times = 0; 
    do {
        Sleep(params.operate_duration); times++; 
        if (times > params.run_max_times) {
            ar::error("Runtime too long !"); 
            std::exit(0);
        } 
        
        cv::Mat frame; params.controller->screencap(frame);
        if (frame.empty()) {
            ar::error("Frame empty !");
            params.controller->disconnect();
            std::exit(0);
        }
        
        ar::point p_main_line = params.image_recognition->compareImageReturnCentrePoint(frame, main_line, 0.95f);
        if (!p_main_line.is_empty) params.controller->click(p_main_line.x, p_main_line.y);
        ar::point p_sweep = params.image_recognition->compareImageReturnCentrePoint(frame, sweep, 0.95f); 
        if (!p_sweep.is_empty) stop_condition = true;
    } while (!stop_condition);

    ar::info("Select veryhard");
    stop_condition = false; times = 0;
    do {
        Sleep(params.operate_duration); times++;
        if (times > params.run_max_times) {
            ar::error("Runtime too long !");
            std::exit(0);
        } 
        
        cv::Mat frame; params.controller->screencap(frame);
        if (frame.empty()) {
            ar::error("Frame empty !");
            params.controller->disconnect();
            std::exit(0);
        }
        
        ar::point p_veryhard_on = params.image_recognition->compareImageReturnCentrePoint(frame, veryhard_on, 0.95f);
        if (!p_veryhard_on.is_empty) stop_condition = true;
        ar::point p_veryhard_off = params.image_recognition->compareImageReturnCentrePoint(frame, veryhard_off, 0.95f);
        if (!p_veryhard_off.is_empty) params.controller->click(p_veryhard_off.x, p_veryhard_off.y);
    } while (!stop_condition);
#undef f

    ar::info("Swipe Selected character");
    //To be continue ...
}