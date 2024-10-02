#include <MainLineVeryHard.h>
#include <define.hpp>

void pcr::main_line_veryhard(pcr::global_params params, pcr::main_line_veryhard_params veryhard_params)
{
    cv::Mat adventure = cv::imread("./res/pcr/adventure.png");
    cv::Mat main_line = cv::imread("./res/pcr/main_line.png");
    cv::Mat sweep = cv::imread("./res/pcr/sweep.png");
    cv::Mat multi_drop = cv::imread("./res/pcr/multi_drop.png");
    cv::Mat veryhard_on = cv::imread("./res/pcr/veryhard_on.png");
    cv::Mat veryhard_off = cv::imread("./res/pcr/veryhard_off.png");

    bool is_auto_execute = false;

    bool stop_condition = false;
    int times = 0;

    ar::info("Enter venture");

#define f() \
    params.controller->click(800, 875); \
    ar::point p_main_line = params.image_recognition->compareImageReturnCentrePoint(frame, main_line, 0.95f); \
    if (!p_main_line.is_empty) stop_condition = true;
    FUNCTION_LOOPS(f);
#undef f

    ar::info("Enter venture");
#define f() \
    ar::point p_main_line = params.image_recognition->compareImageReturnCentrePoint(frame, main_line, 0.95f); \
    if (!p_main_line.is_empty) params.controller->click(p_main_line.x, p_main_line.y); \
    ar::point p_sweep = params.image_recognition->compareImageReturnCentrePoint(frame, sweep, 0.95f); \
    if (!p_sweep.is_empty) stop_condition = true;
    FUNCTION_LOOPS(f);
#undef f

    ar::info("Select veryhard");
#define f() \
    ar::point p_veryhard_on = params.image_recognition->compareImageReturnCentrePoint(frame, veryhard_on, 0.95f); \
    if (!p_veryhard_on.is_empty) stop_condition = true; \
    ar::point p_veryhard_off = params.image_recognition->compareImageReturnCentrePoint(frame, veryhard_off, 0.95f); \
    if (!p_veryhard_off.is_empty) params.controller->click(p_veryhard_off.x, p_veryhard_off.y);
    FUNCTION_LOOPS(f);
#undef f

    ar::info("Swipe Selected character");
    //To be continue ...
}