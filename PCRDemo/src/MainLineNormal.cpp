#include <MainLineNormal.h>
#include <define.hpp>

void pcr::main_line_normal(pcr::global_params& params, pcr::main_line_normal_params& normal_params){
    LOAD_IMAGE(main_line, "./res/pcr/main_line.png");
    LOAD_IMAGE(sweep, "./res/pcr/sweep.png");
    LOAD_IMAGE(multi_drop, "./res/pcr/multi_drop.png");

    //识别是否自动执行，选择否可以自定义执行，选择是会先判断是否有双倍掉落再执行
    bool is_auto_execute = false;
    bool stop_condition = false;
    int times = 0;

    ar::info("Enter venture");
    stop_condition = false; times = 0;
    do {
        Sleep(params.operate_duration);
        times++;

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

        params.controller->click(800, 875); 
        ar::point p_main_line = params.image_recognition->compareImageReturnCentrePoint(frame, main_line, 0.95f); 
        if (!p_main_line.is_empty) stop_condition = true;
    } while (!stop_condition);;

    stop_condition = false; times = 0; 
    do {
        Sleep(params.operate_duration); 
        times++; 

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
    } while (!stop_condition);;


    //检查是否有双倍
    bool is_executue = true;
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
        
        ar::point p_multi_drop = params.image_recognition->compareImageReturnCentrePoint(frame, multi_drop, 0.95f); 
        if (!p_multi_drop.is_empty) {
            ar::info("Main line normal is multi_drop now !"); 
            is_auto_execute = normal_params.is_auto_execute_in_multi_drop; 
            stop_condition = true;
        } 
        //超过一定时间没看到双倍掉落图标自动放弃执行
        if (times > 15) {
            stop_condition = true; 
            is_executue = false;
        };
    } while (!stop_condition);

    if (is_auto_execute && is_executue) {
        pcr::scheme_zero(params);
    }
    else {
        if(normal_params.scheme_num == 0) pcr::scheme_zero(params);
        //add different scheme ...
    }
}

//方案0(自动执行)
void pcr::scheme_zero(pcr::global_params& params) {
    LOAD_IMAGE(sweep, "./res/pcr/sweep.png");
    LOAD_IMAGE(search, "./res/pcr/search.png");
    LOAD_IMAGE(cancel_all, "./res/pcr/cancel_all.png");
    LOAD_IMAGE(level_search, "./res/pcr/level_search.png");
    LOAD_IMAGE(decision, "./res/pcr/decision.png");
    LOAD_IMAGE(all_on, "./res/pcr/all_on.png");
    LOAD_IMAGE(all_off, "./res/pcr/all_off.png");
    LOAD_IMAGE(sift, "./res/pcr/sift.png");
    LOAD_IMAGE(cancel_all_check_off, "./res/pcr/cancel_all_check_off.png");
    LOAD_IMAGE(cancel_all_check_on, "./res/pcr/cancel_all_check_on.png");
    LOAD_IMAGE(power_lack, "./res/pcr/power_lack.png");
    LOAD_IMAGE(one_click_sweep, "./res/pcr/one_click_sweep.png");
    LOAD_IMAGE(challenge, "./res/pcr/challenge.png");
    LOAD_IMAGE(skip_over, "./res/pcr/skip_over.png");
    LOAD_IMAGE(skip_over_gray, "./res/pcr/skip_over_gray.png");

    bool stop_condition = false;
    int times = 0;

    ar::info("Start auto execute normal !");
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

        ar::point p_sweep = params.image_recognition->compareImageReturnCentrePoint(frame, sweep, 0.95f); 
        if (!p_sweep.is_empty) params.controller->click(p_sweep.x, p_sweep.y); 
        ar::point p_search = params.image_recognition->compareImageReturnCentrePoint(frame, search, 0.95f); 
        if (!p_search.is_empty) params.controller->click(p_search.x, p_search.y); 
        ar::point p_cancel_all = params.image_recognition->compareImageReturnCentrePoint(frame, cancel_all, 0.95f); 
        if (!p_cancel_all.is_empty) stop_condition = true;
    } while (!stop_condition);

    ar::info("Obtain the first three types of scarce gear !");
    params.controller->click(1439, 297);
    Sleep(params.operate_duration);
    params.controller->click(149, 345);
    Sleep(params.operate_duration);
    params.controller->click(349, 345);
    Sleep(params.operate_duration);
    params.controller->click(507, 345);
    Sleep(params.operate_duration);
    params.controller->click(992, 798);
    Sleep(params.operate_duration);
    params.controller->click(992, 798);

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
        
        ar::point p_search = params.image_recognition->compareImageReturnCentrePoint(frame, search, 0.95f); 
        if (!p_search.is_empty) stop_condition = true;
    } while (!stop_condition);

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

        params.controller->click(811, 249); 
        params.controller->screencap(frame); 
        ar::point p_sift = params.image_recognition->compareImageReturnCentrePoint(frame, sift, 0.95f); 
        if (!p_sift.is_empty) stop_condition = true;
    } while (!stop_condition);

    params.controller->click(478, 331);
    Sleep(params.operate_duration);
    params.controller->click(478, 331);

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
            ar::error("Frame empty !"); params.controller->disconnect(); 
            std::exit(0);
        } 
        
        ar::point p_all_off = params.image_recognition->compareImageReturnCentrePoint(frame, all_off, 0.95f); 
        if (!p_all_off.is_empty) stop_condition = true; 
        ar::point p_all_on = params.image_recognition->compareImageReturnCentrePoint(frame, all_on, 0.95f); 
        if (!p_all_on.is_empty) params.controller->click(p_all_on.x, p_all_on.y);
    } while (!stop_condition);

    params.controller->click(478, 421);
    Sleep(params.operate_duration);
    params.controller->click(973, 794);

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
        
        ar::point p_search = params.image_recognition->compareImageReturnCentrePoint(frame, search, 0.95f); 
        if (!p_search.is_empty) stop_condition = true;
    } while (!stop_condition);

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
        
        ar::point p_cancel_all_check_on = params.image_recognition->compareImageReturnCentrePoint(frame, cancel_all_check_on, 0.95f); 
        if (!p_cancel_all_check_on.is_empty) stop_condition = true; 
        ar::point p_cancel_all_check_off = params.image_recognition->compareImageReturnCentrePoint(frame, cancel_all_check_off, 0.95f); 
        if (!p_cancel_all_check_off.is_empty) stop_condition = true;
    } while (!stop_condition);

    params.controller->click(1412, 245);
    Sleep(params.operate_duration);
    params.controller->click(1412, 340);
    Sleep(params.operate_duration);
    params.controller->click(1412, 447);
    Sleep(params.operate_duration);
    params.controller->click(1412, 549);

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
        
        params.controller->click(1475, 687); 
        ar::point p_power_lack = params.image_recognition->compareImageReturnCentrePoint(frame, power_lack, 0.95f); 
        if (!p_power_lack.is_empty) stop_condition = true;
    } while (!stop_condition);


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
        
        ar::point p_power_lack = params.image_recognition->compareImageReturnCentrePoint(frame, power_lack, 0.95f); 
        if (!p_power_lack.is_empty) params.controller->click(1222, 687); else stop_condition = true; 
        //点击50次仍没反应判定为没体力
        if (times > 50) {
            ar::info("power lack ! Return to menu!"); stop_condition = true;
        };
    } while (!stop_condition);

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
        
        ar::point p_one_click_sweep = params.image_recognition->compareImageReturnCentrePoint(frame, one_click_sweep, 0.95f); 
        if (!p_one_click_sweep.is_empty) params.controller->click(p_one_click_sweep.x, p_one_click_sweep.y); 
        ar::point p_challenge = params.image_recognition->compareImageReturnCentrePoint(frame, challenge, 0.95f); 
        if (!p_challenge.is_empty) stop_condition = true;
    } while (!stop_condition);

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
        
        ar::point p_challenge = params.image_recognition->compareImageReturnCentrePoint(frame, challenge, 0.95f); 
        if (!p_challenge.is_empty) params.controller->click(p_challenge.x, p_challenge.y); 
        ar::point p_skip_over = params.image_recognition->compareImageReturnCentrePoint(frame, skip_over, 0.95f); 
        if (!p_skip_over.is_empty) params.controller->click(p_skip_over.x, p_skip_over.y); 
        ar::point p_skip_over_gray = params.image_recognition->compareImageReturnCentrePoint(frame, skip_over_gray, 0.95f); 
        if (!p_skip_over_gray.is_empty) stop_condition = true;
    } while (!stop_condition);

    stop_condition = false; times = 0; 
    do {
        Sleep(params.operate_duration); times++; 
        if (times > params.run_max_times) {
            ar::error("Runtime too long !"); 
            std::exit(0);
        } 
        
        cv::Mat frame; params.controller->screencap(frame);
        if (frame.empty()) {
            ar::error("Frame empty !"); params.controller->disconnect(); 
            std::exit(0);
        } 
        
        params.controller->click(1590, 402); 
        ar::point p_sweep = params.image_recognition->compareImageReturnCentrePoint(frame, sweep, 0.95f); 
        if (!p_sweep.is_empty) stop_condition = true;
    } while (!stop_condition);
}