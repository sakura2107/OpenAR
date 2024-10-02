#include <ReceiveGifts.h>
#include <windows.h>
#include <Log/MiniLog.h>

void pcr::receive_gifts(pcr::global_params& params){
    cv::Mat gift = cv::imread("./res/pcr/gift.png");
    cv::Mat gift_box = cv::imread("./res/pcr/gift_box.png");
    cv::Mat receive_all = cv::imread("./res/pcr/receive_all.png");
    cv::Mat confirm_blue = cv::imread("./res/pcr/confirm_blue.png");
    cv::Mat confirm_white = cv::imread("./res/pcr/confirm_white.png");
    cv::Mat receive_all_gray = cv::imread("./res/pcr/receive_all_gray.png");
    cv::Mat home_off = cv::imread("./res/pcr/home_off.png");

    bool stop_condition = false;
    int times = 0;

    ar::info("Back to home");
    do{
        Sleep(params.operate_duration);
        times++;
        if(times > params.run_max_times){ar::error("Runtime too long !"); std::exit(0);}

        cv::Mat frame;
        params.controller->screencap(frame);

        params.controller->click(155, 885);
        ar::point p_gift = params.image_recognition->compareImageReturnCentrePoint(frame, gift, 0.95f);
        if(!p_gift.is_empty) stop_condition = true;
    }while(!stop_condition);

    stop_condition = false;
    times = 0;
    ar::info("Enter gifts");
    do{
        Sleep(params.operate_duration);
        times++;
        if(times > params.run_max_times){ar::error("Runtime too long !"); std::exit(0);}

        cv::Mat frame;
        params.controller->screencap(frame);

        ar::point p_gift = params.image_recognition->compareImageReturnCentrePoint(frame, gift, 0.95f);
        if(!p_gift.is_empty) params.controller->click(p_gift.x, p_gift.y);
        ar::point p_gift_box = params.image_recognition->compareImageReturnCentrePoint(frame, gift_box, 0.95f);
        if(!p_gift_box.is_empty) stop_condition = true;
    }while(!stop_condition);

    stop_condition = false;
    times = 0;
    ar::info("Receive gifts");
    do{
        Sleep(params.operate_duration);
        times++;
        if(times > params.run_max_times){ar::error("Runtime too long !"); std::exit(0);}

        cv::Mat frame;
        params.controller->screencap(frame);

        ar::point p_receive_all = params.image_recognition->compareImageReturnCentrePoint(frame, receive_all, 0.95f);
        if(!p_receive_all.is_empty) params.controller->click(p_receive_all.x, p_receive_all.y);
        ar::point p_confirm_blue = params.image_recognition->compareImageReturnCentrePoint(frame, confirm_blue, 0.95f);
        if(!p_confirm_blue.is_empty) params.controller->click(p_confirm_blue.x, p_confirm_blue.y);
        ar::point p_confirm_white = params.image_recognition->compareImageReturnCentrePoint(frame, confirm_white, 0.95f);
        if(!p_confirm_white.is_empty) params.controller->click(p_confirm_white.x, p_confirm_white.y);
        ar::point p_receive_all_gray = params.image_recognition->compareImageReturnCentrePoint(frame, receive_all_gray, 0.95f);
        if(!p_receive_all_gray.is_empty) stop_condition = true;
    }while(!stop_condition);

    ar::info("Back to home");
    params.controller->click(1578, 500);
}