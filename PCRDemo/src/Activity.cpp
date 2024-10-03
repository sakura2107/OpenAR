#include <Activity.h>
#include <define.hpp>

void pcr::execute_activity(pcr::global_params& params) {
    LOAD_IMAGE(main_line, "./res/pcr/main_line.png");
    LOAD_IMAGE(store_icon, "./res/pcr/store_icon.png");
	LOAD_IMAGE(activity_level, "./res/pcr/activity_level.png");
	LOAD_IMAGE(xuefei, "./res/pcr/xuefei.png");
    LOAD_IMAGE(hard_off, "./res/pcr/hard_off.png");
    LOAD_IMAGE(hard_lock, "./res/pcr/hard_lock.png");
    LOAD_IMAGE(hard_on, "./res/pcr/hard_on.png");
    LOAD_IMAGE(download_sound, "./res/pcr/download_sound.png");
    LOAD_IMAGE(menu, "./res/pcr/menu.png");
    LOAD_IMAGE(story_skip, "./res/pcr/story_skip.png");
    LOAD_IMAGE(confirm_blue, "./res/pcr/confirm_blue.png")

	bool stop_condition = false;
	int times = 0;

    bool is_uncomplete = true;

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
        
        params.controller->click(802, 891); 
        ar::point p_main_line = params.image_recognition->compareImageReturnCentrePoint(frame, main_line, 0.95f); 
        if (!p_main_line.is_empty) stop_condition = true;
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
        
        params.controller->click(152, 891); 
        ar::point p_store_icon = params.image_recognition->compareImageReturnCentrePoint(frame, store_icon, 0.95f);
        if (!p_store_icon.is_empty) stop_condition = true;
    } while (!stop_condition);

#define f() \
    if(times % 5 == 0)params.controller->click(1350, 220); \
    ar::point p_download_sound = params.image_recognition->compareImageReturnCentrePoint(frame, download_sound, 0.95f); \
    if (!p_download_sound.is_empty) params.controller->click(p_download_sound.x, p_download_sound.y); \
    ar::point p_menu = params.image_recognition->compareImageReturnCentrePoint(frame, menu, 0.95f); \
    if (!p_menu.is_empty) params.controller->click(p_menu.x, p_menu.y); \
    ar::point p_story_skip = params.image_recognition->compareImageReturnCentrePoint(frame, story_skip, 0.95f); \
    if (!p_story_skip.is_empty) params.controller->click(p_story_skip.x, p_story_skip.y); \
    ar::point p_confirm_blue = params.image_recognition->compareImageReturnCentrePoint(frame, confirm_blue, 0.95f); \
    if (!p_confirm_blue.is_empty) params.controller->click(p_confirm_blue.x, p_confirm_blue.y); \
    ar::point p_activity_level = params.image_recognition->compareImageReturnCentrePoint(frame, activity_level, 0.95f); \
    if (!p_activity_level.is_empty) {ar::info("Find Activity !"); stop_condition = true;} \
    if(times > params.run_max_times) {ar::info("Cannot find Activity ! Maybe it is not this time ..."); return;}
    FUNCTION_LOOPS(f);
#undef f

    ar::info("Try to find character ...");
#define f() \
    ar::point p_activity_level = params.image_recognition->compareImageReturnCentrePoint(frame, activity_level, 0.95f); \
    if (!p_activity_level.is_empty) params.controller->click(p_activity_level.x, p_activity_level.y); \
    ar::point p_hard_lock = params.image_recognition->compareImageReturnCentrePoint(frame, hard_lock, 0.95f); \
    if (!p_hard_lock.is_empty) {stop_condition = true;} \
    ar::point p_hard_off = params.image_recognition->compareImageReturnCentrePoint(frame, hard_off, 0.95f); \
    if (!p_hard_off.is_empty) {is_uncomplete = false; stop_condition = true;} \
    ar::point p_hard_on = params.image_recognition->compareImageReturnCentrePoint(frame, hard_on, 0.95f); \
    if (!p_hard_on.is_empty) { is_uncomplete = false; stop_condition = true; }
    FUNCTION_LOOPS(f);
#undef f

    if (is_uncomplete == true) auto_active(params);
    else return;
}


void pcr::auto_active(pcr::global_params& params) {
    LOAD_IMAGE(xuefei, "./res/pcr/xuefei.png");
    LOAD_IMAGE(auto_run_off, "./res/pcr/auto_run_off.png");
    LOAD_IMAGE(auto_run_on, "./res/pcr/auto_run_on.png");
    LOAD_IMAGE(challenge, "./res/pcr/challenge.png");
    LOAD_IMAGE(auto_next_step, "./res/pcr/auto_next_step.png");
    LOAD_IMAGE(battle_start, "./res/pcr/battle_start.png");
    LOAD_IMAGE(confirm_white, "./res/pcr/confirm_white.png");
    LOAD_IMAGE(auto_run_stop, "./res/pcr/auto_run_stop.png");
    LOAD_IMAGE(next_step, "./res/pcr/next_step.png");
    LOAD_IMAGE(close_white, "./res/pcr/close_white.png");
    LOAD_IMAGE(return_icon, "./res/pcr/return_icon.png");
    LOAD_IMAGE(activity_boss_normal, "./res/pcr/activity_boss_normal.png");
    LOAD_IMAGE(skip, "./res/pcr/skip.png");
    LOAD_IMAGE(normal_off, "./res/pcr/normal_off.png");
    LOAD_IMAGE(confirm_blue, "./res/pcr/confirm_blue.png");
    LOAD_IMAGE(activity_boss_hard, "./res/pcr/activity_boss_hard.png");
    LOAD_IMAGE(activity_boss_veryhard, "./res/pcr/activity_boss_veryhard.png");
    LOAD_IMAGE(hard_on, "./res/pcr/hard_on.png");

    bool stop_condition = false;
    int times = 0;

    ar::info("start to run auto activity!");
#define f() \
    for(int i = 0; i < 50; i++){ \
        Sleep(5); \
        ar::point p_xuefei = params.image_recognition->compareImageReturnCentrePoint(frame, xuefei, 0.95f); \
        if (!p_xuefei.is_empty) params.controller->click(p_xuefei.x, p_xuefei.y); \
        ar::point p_auto_run_off = params.image_recognition->compareImageReturnCentrePoint(frame, auto_run_off, 0.95f); \
        if (!p_auto_run_off.is_empty) params.controller->click(p_auto_run_off.x, p_auto_run_off.y); \
        ar::point p_auto_run_on = params.image_recognition->compareImageReturnCentrePoint(frame, auto_run_on, 0.95f); \
        if (!p_auto_run_on.is_empty) { ar::info("Auto run on"); stop_condition = true; break; } \
    }
    FUNCTION_LOOPS(f);
#undef f

#define f() \
    ar::point p_challenge = params.image_recognition->compareImageReturnCentrePoint(frame, challenge, 0.95f); \
    if (!p_challenge.is_empty) params.controller->click(p_challenge.x, p_challenge.y); \
    ar::point p_auto_next_step = params.image_recognition->compareImageReturnCentrePoint(frame, auto_next_step, 0.95f); \
    if (!p_auto_next_step.is_empty) params.controller->click(p_auto_next_step.x, p_auto_next_step.y); \
    ar::point p_battle_start = params.image_recognition->compareImageReturnCentrePoint(frame, battle_start, 0.95f); \
    if (!p_battle_start.is_empty) stop_condition = true;
    FUNCTION_LOOPS(f);
#undef f

#define f() \
    times = 0; \
    ar::point p_battle_start = params.image_recognition->compareImageReturnCentrePoint(frame, battle_start, 0.95f); \
    if (!p_battle_start.is_empty) params.controller->click(p_battle_start.x, p_battle_start.y); \
    ar::point p_auto_run_stop = params.image_recognition->compareImageReturnCentrePoint(frame, auto_run_stop, 0.95f); \
    if (!p_auto_run_stop.is_empty) stop_condition = true;
    FUNCTION_LOOPS(f);
#undef f

#define f() \
    ar::point p_confirm_white = params.image_recognition->compareImageReturnCentrePoint(frame, confirm_white, 0.95f); \
    if (!p_confirm_white.is_empty) params.controller->click(p_confirm_white.x, p_confirm_white.y); \
    ar::point p_next_step = params.image_recognition->compareImageReturnCentrePoint(frame, next_step, 0.95f); \
    if (!p_next_step.is_empty) params.controller->click(p_next_step.x, p_next_step.y); \
    ar::point p_close_white = params.image_recognition->compareImageReturnCentrePoint(frame, close_white, 0.95f); \
    if (!p_close_white.is_empty) stop_condition = true;
    FUNCTION_LOOPS(f);
#undef f

#define f() \
    params.controller->click(1591, 552); \
    ar::point p_return_icon = params.image_recognition->compareImageReturnCentrePoint(frame, return_icon, 0.95f); \
    if (!p_return_icon.is_empty) params.controller->click(p_return_icon.x, p_return_icon.y); \
    ar::point p_activity_boss_normal = params.image_recognition->compareImageReturnCentrePoint(frame, activity_boss_normal, 0.95f); \
    if (!p_activity_boss_normal.is_empty) stop_condition = true;
    FUNCTION_LOOPS(f);
#undef f

    ar::info("attack boss normal !");
#define f() \
    times = 0; \
    params.controller->click(1591, 552); \
    ar::point p_activity_boss_normal = params.image_recognition->compareImageReturnCentrePoint(frame, activity_boss_normal, 0.95f); \
    if (!p_activity_boss_normal.is_empty) params.controller->click(p_activity_boss_normal.x, p_activity_boss_normal.y); \
    ar::point p_challenge = params.image_recognition->compareImageReturnCentrePoint(frame, challenge, 0.95f); \
    if (!p_challenge.is_empty) params.controller->click(p_challenge.x, p_challenge.y); \
    ar::point p_battle_start = params.image_recognition->compareImageReturnCentrePoint(frame, battle_start, 0.95f); \
    if (!p_battle_start.is_empty) params.controller->click(p_battle_start.x, p_battle_start.y); \
    ar::point p_skip = params.image_recognition->compareImageReturnCentrePoint(frame, skip, 0.95f); \
    if (!p_skip.is_empty) params.controller->click(p_skip.x, p_skip.y); \
    ar::point p_normal_off = params.image_recognition->compareImageReturnCentrePoint(frame, normal_off, 0.95f); \
    if (!p_normal_off.is_empty) stop_condition = true; \
    ar::point p_confirm_blue = params.image_recognition->compareImageReturnCentrePoint(frame, confirm_blue, 0.95f); \
    if (!p_confirm_blue.is_empty) params.controller->click(p_confirm_blue.x, p_confirm_blue.y);
    FUNCTION_LOOPS(f);
#undef f

#define f() \
    for(int i = 0; i < 50; i++){ \
        Sleep(5); \
        ar::point p_xuefei = params.image_recognition->compareImageReturnCentrePoint(frame, xuefei, 0.95f); \
        if (!p_xuefei.is_empty) params.controller->click(p_xuefei.x, p_xuefei.y); \
        ar::point p_auto_run_off = params.image_recognition->compareImageReturnCentrePoint(frame, auto_run_off, 0.95f); \
        if (!p_auto_run_off.is_empty) params.controller->click(p_auto_run_off.x, p_auto_run_off.y); \
        ar::point p_auto_run_on = params.image_recognition->compareImageReturnCentrePoint(frame, auto_run_on, 0.95f); \
        if (!p_auto_run_on.is_empty) { ar::info("Auto run on"); stop_condition = true; break; } \
    }
    FUNCTION_LOOPS(f);
#undef f

    ar::info("Start auto");
#define f() \
    ar::point p_challenge = params.image_recognition->compareImageReturnCentrePoint(frame, challenge, 0.95f); \
    if (!p_challenge.is_empty) params.controller->click(p_challenge.x, p_challenge.y); \
    ar::point p_auto_next_step = params.image_recognition->compareImageReturnCentrePoint(frame, auto_next_step, 0.95f); \
    if (!p_auto_next_step.is_empty) params.controller->click(p_auto_next_step.x, p_auto_next_step.y); \
    ar::point p_battle_start = params.image_recognition->compareImageReturnCentrePoint(frame, battle_start, 0.95f); \
    if (!p_battle_start.is_empty) stop_condition = true;
    FUNCTION_LOOPS(f);
#undef f

#define f() \
    times = 0; \
    ar::point p_battle_start = params.image_recognition->compareImageReturnCentrePoint(frame, battle_start, 0.95f); \
    if (!p_battle_start.is_empty) params.controller->click(p_battle_start.x, p_battle_start.y); \
    ar::point p_auto_run_stop = params.image_recognition->compareImageReturnCentrePoint(frame, auto_run_stop, 0.95f); \
    if (!p_auto_run_stop.is_empty) stop_condition = true;
    FUNCTION_LOOPS(f);
#undef f

#define f() \
    ar::point p_confirm_white = params.image_recognition->compareImageReturnCentrePoint(frame, confirm_white, 0.95f); \
    if (!p_confirm_white.is_empty) params.controller->click(p_confirm_white.x, p_confirm_white.y); \
    ar::point p_next_step = params.image_recognition->compareImageReturnCentrePoint(frame, next_step, 0.95f); \
    if (!p_next_step.is_empty) params.controller->click(p_next_step.x, p_next_step.y); \
    ar::point p_close_white = params.image_recognition->compareImageReturnCentrePoint(frame, close_white, 0.95f); \
    if (!p_close_white.is_empty) stop_condition = true;
    FUNCTION_LOOPS(f);
#undef f

#define f() \
    params.controller->click(1591, 552); \
    ar::point p_return_icon = params.image_recognition->compareImageReturnCentrePoint(frame, return_icon, 0.95f); \
    if (!p_return_icon.is_empty) params.controller->click(p_return_icon.x, p_return_icon.y); \
    ar::point p_activity_boss_hard = params.image_recognition->compareImageReturnCentrePoint(frame, activity_boss_hard, 0.95f); \
    if (!p_activity_boss_hard.is_empty) stop_condition = true;
    FUNCTION_LOOPS(f);
#undef f

    ar::info("attack boss hard !");
#define f() \
    times = 0; \
    ar::point p_activity_boss_hard = params.image_recognition->compareImageReturnCentrePoint(frame, activity_boss_hard, 0.95f); \
    if (!p_activity_boss_hard.is_empty) params.controller->click(p_activity_boss_hard.x, p_activity_boss_hard.y); \
    ar::point p_challenge = params.image_recognition->compareImageReturnCentrePoint(frame, challenge, 0.95f); \
    if (!p_challenge.is_empty) params.controller->click(p_challenge.x, p_challenge.y); \
    ar::point p_battle_start = params.image_recognition->compareImageReturnCentrePoint(frame, battle_start, 0.95f); \
    if (!p_battle_start.is_empty) params.controller->click(p_battle_start.x, p_battle_start.y); \
    ar::point p_skip = params.image_recognition->compareImageReturnCentrePoint(frame, skip, 0.95f); \
    if (!p_skip.is_empty) params.controller->click(p_skip.x, p_skip.y); \
    ar::point p_confirm_blue = params.image_recognition->compareImageReturnCentrePoint(frame, confirm_blue, 0.95f); \
    if (!p_skip.is_empty) params.controller->click(p_skip.x, p_skip.y); \
    ar::point p_next_step = params.image_recognition->compareImageReturnCentrePoint(frame, next_step, 0.95f); \
    if (!p_next_step.is_empty) params.controller->click(p_next_step.x, p_next_step.y); \
    ar::point p_hard_on = params.image_recognition->compareImageReturnCentrePoint(frame, hard_on, 0.95f); \
    if (!p_hard_on.is_empty) stop_condition = true;
    FUNCTION_LOOPS(f);
#undef f

#define f() \
    params.controller->click(1591, 552); \
    ar::point p_return_icon = params.image_recognition->compareImageReturnCentrePoint(frame, return_icon, 0.95f); \
    if (!p_return_icon.is_empty) params.controller->click(p_return_icon.x, p_return_icon.y); \
    ar::point p_activity_boss_hard = params.image_recognition->compareImageReturnCentrePoint(frame, activity_boss_hard, 0.95f); \
    if (!p_activity_boss_hard.is_empty) stop_condition = true;
    FUNCTION_LOOPS(f);
#undef f

    ar::info("attack boss veryhard !");
#define f() \
    times = 0; \
    ar::point p_activity_boss_hard = params.image_recognition->compareImageReturnCentrePoint(frame, activity_boss_veryhard, 0.95f); \
    if (!p_activity_boss_hard.is_empty) params.controller->click(p_activity_boss_hard.x, p_activity_boss_hard.y); \
    ar::point p_challenge = params.image_recognition->compareImageReturnCentrePoint(frame, challenge, 0.95f); \
    if (!p_challenge.is_empty) params.controller->click(p_challenge.x, p_challenge.y); \
    ar::point p_battle_start = params.image_recognition->compareImageReturnCentrePoint(frame, battle_start, 0.95f); \
    if (!p_battle_start.is_empty) params.controller->click(p_battle_start.x, p_battle_start.y); \
    ar::point p_skip = params.image_recognition->compareImageReturnCentrePoint(frame, skip, 0.95f); \
    if (!p_skip.is_empty) params.controller->click(p_skip.x, p_skip.y); \
    ar::point p_confirm_blue = params.image_recognition->compareImageReturnCentrePoint(frame, confirm_blue, 0.95f); \
    if (!p_skip.is_empty) params.controller->click(p_skip.x, p_skip.y); \
    ar::point p_next_step = params.image_recognition->compareImageReturnCentrePoint(frame, next_step, 0.95f); \
    if (!p_next_step.is_empty) params.controller->click(p_next_step.x, p_next_step.y);
    FUNCTION_LOOPS(f);
#undef f
}