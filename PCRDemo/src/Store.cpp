#include <Store.h>
#include <define.hpp>

void pcr::Store(pcr::global_params& params, pcr::store_params& s_params) {
	cv::Mat store_icon = cv::imread("./res/pcr/store_icon.png");
	cv::Mat store = cv::imread("./res/pcr/store.png");

	bool stop_condition = false;
	int times = 0;

	ar::info("Enter Store");
#define f() \
    params.controller->click(155, 885); \
    ar::point p_store_icon = params.image_recognition->compareImageReturnCentrePoint(frame, store_icon, 0.95f); \
    if (!p_store_icon.is_empty) stop_condition = true;
	FUNCTION_LOOPS(f);
#undef f

#define f() \
    ar::point p_store_icon = params.image_recognition->compareImageReturnCentrePoint(frame, store_icon, 0.95f); \
    if (!p_store_icon.is_empty) params.controller->click(p_store_icon.x, p_store_icon.y); \
    ar::point p_store = params.image_recognition->compareImageReturnCentrePoint(frame, store, 0.95f); \
    if (!p_store.is_empty) stop_condition = true;
	FUNCTION_LOOPS(f);
#undef f
	if (s_params.is_buy_lotion == true) store_buy_lotion(params);
	if (s_params.is_buy_refining_stone == true) store_buy_refining_stone(params);
}

void pcr::store_buy_lotion(pcr::global_params& params) {
	cv::Mat common_off = cv::imread("./res/pcr/common_off.png");
	cv::Mat common_on = cv::imread("./res/pcr/common_on.png");
	cv::Mat lotion_on = cv::imread("./res/pcr/lotion_on.png");
	cv::Mat lotion_off = cv::imread("./res/pcr/lotion_off.png");
	cv::Mat select_all_on = cv::imread("./res/pcr/select_all_on.png");
	cv::Mat select_all_off = cv::imread("./res/pcr/select_all_off.png");
	cv::Mat confirm_blue = cv::imread("./res/pcr/confirm_blue.png");
	cv::Mat select_all_gray = cv::imread("./res/pcr/select_all_gray.png");
	cv::Mat buy_all = cv::imread("./res/pcr/buy_all.png");

	bool stop_condition = false;
	int times = 0;

	ar::info("Buy lotion");

#define f() \
    ar::point p_common_off = params.image_recognition->compareImageReturnCentrePoint(frame, common_off, 0.95f); \
    if (!p_common_off.is_empty) params.controller->click(p_common_off.x, p_common_off.y); \
	ar::point p_common_on = params.image_recognition->compareImageReturnCentrePoint(frame, common_on, 0.95f); \
    if (!p_common_on.is_empty) stop_condition = true;
	FUNCTION_LOOPS(f);
#undef f

#define f() \
	ar::point p_select_all_gray = params.image_recognition->compareImageReturnCentrePoint(frame, select_all_gray, 0.95f); \
    if (!p_select_all_gray.is_empty) return; \
    ar::point p_lotion_off = params.image_recognition->compareImageReturnCentrePoint(frame, lotion_off, 0.95f); \
    if (!p_lotion_off.is_empty) params.controller->click(p_lotion_off.x, p_lotion_off.y); \
	ar::point p_lotion_on = params.image_recognition->compareImageReturnCentrePoint(frame, lotion_on, 0.95f); \
    if (!p_lotion_on.is_empty) stop_condition = true;
	FUNCTION_LOOPS(f);
#undef f

#define f() \
    ar::point p_select_all_off = params.image_recognition->compareImageReturnCentrePoint(frame, select_all_off, 0.95f); \
    if (!p_select_all_off.is_empty) params.controller->click(p_select_all_off.x, p_select_all_off.y); \
	ar::point p_select_all_on = params.image_recognition->compareImageReturnCentrePoint(frame, select_all_on, 0.95f); \
    if (!p_select_all_on.is_empty) stop_condition = true;
	FUNCTION_LOOPS(f);
#undef f

#define f() \
    ar::point p_buy_all = params.image_recognition->compareImageReturnCentrePoint(frame, buy_all, 0.95f); \
    if (!p_buy_all.is_empty) params.controller->click(p_buy_all.x, p_buy_all.y); \
	ar::point p_confirm_blue = params.image_recognition->compareImageReturnCentrePoint(frame, confirm_blue, 0.95f); \
	if (!p_confirm_blue.is_empty) stop_condition = true;
	FUNCTION_LOOPS(f);
#undef f

#define f() \
    ar::point p_confirm_blue = params.image_recognition->compareImageReturnCentrePoint(frame, confirm_blue, 0.95f); \
	if (!p_confirm_blue.is_empty) params.controller->click(p_confirm_blue.x, p_confirm_blue.y); \
	ar::point p_select_all_gray = params.image_recognition->compareImageReturnCentrePoint(frame, select_all_gray, 0.95f); \
	if (!p_select_all_gray.is_empty) stop_condition = true;
	FUNCTION_LOOPS(f);
#undef f

}

void pcr::store_buy_refining_stone(pcr::global_params& params) {
	LOAD_IMAGE(common_off, "./res/pcr/common_off.png");
	LOAD_IMAGE(common_on, "./res/pcr/common_on.png");
	LOAD_IMAGE(confirm_blue, "./res/pcr/confirm_blue.png");
	LOAD_IMAGE(refining_stone_on, "./res/pcr/refining_stone_on.png");
	LOAD_IMAGE(refining_stone_off, "./res/pcr/refining_stone_off.png");
	LOAD_IMAGE(select_all_gray, "./res/pcr/select_all_gray.png");
	LOAD_IMAGE(select_all_on, "./res/pcr/select_all_on.png");
	LOAD_IMAGE(select_all_off, "./res/pcr/select_all_off.png");
	LOAD_IMAGE(buy_all, "./res/pcr/buy_all.png");

	bool stop_condition = false;
	int times = 0;

	ar::info("Buy refining stone");
#define f() \
	ar::point p_confirm_blue = params.image_recognition->compareImageReturnCentrePoint(frame, confirm_blue, 0.95f); \
    if (!p_confirm_blue.is_empty) params.controller->click(p_confirm_blue.x, p_confirm_blue.y); \
    ar::point p_common_off = params.image_recognition->compareImageReturnCentrePoint(frame, common_off, 0.95f); \
    if (!p_common_off.is_empty) params.controller->click(p_common_off.x, p_common_off.y); \
	ar::point p_common_on = params.image_recognition->compareImageReturnCentrePoint(frame, common_on, 0.95f); \
    if (!p_common_on.is_empty) stop_condition = true;
	FUNCTION_LOOPS(f);
#undef f

#define f() \
    ar::point p_refining_stone_off = params.image_recognition->compareImageReturnCentrePoint(frame, refining_stone_off, 0.95f); \
    if (!p_refining_stone_off.is_empty) params.controller->click(p_refining_stone_off.x, p_refining_stone_off.y); \
	ar::point p_refining_stone_on = params.image_recognition->compareImageReturnCentrePoint(frame, refining_stone_on, 0.95f); \
    if (!p_refining_stone_on.is_empty) stop_condition = true;
	FUNCTION_LOOPS(f);
#undef f

#define f() \
	ar::point p_select_all_gray = params.image_recognition->compareImageReturnCentrePoint(frame, select_all_gray, 0.95f); \
    if (!p_select_all_gray.is_empty) return; \
    ar::point p_refining_stone_off = params.image_recognition->compareImageReturnCentrePoint(frame, refining_stone_off, 0.95f); \
    if (!p_refining_stone_off.is_empty) params.controller->click(p_refining_stone_off.x, p_refining_stone_off.y); \
	ar::point p_refining_stone_on = params.image_recognition->compareImageReturnCentrePoint(frame, refining_stone_on, 0.95f); \
    if (!p_refining_stone_on.is_empty) stop_condition = true;
	FUNCTION_LOOPS(f);
#undef f

#define f() \
    ar::point p_select_all_off = params.image_recognition->compareImageReturnCentrePoint(frame, select_all_off, 0.95f); \
    if (!p_select_all_off.is_empty) params.controller->click(p_select_all_off.x, p_select_all_off.y); \
	ar::point p_select_all_on = params.image_recognition->compareImageReturnCentrePoint(frame, select_all_on, 0.95f); \
    if (!p_select_all_on.is_empty) stop_condition = true;
	FUNCTION_LOOPS(f);
#undef f

#define f() \
    ar::point p_buy_all = params.image_recognition->compareImageReturnCentrePoint(frame, buy_all, 0.95f); \
    if (!p_buy_all.is_empty) params.controller->click(p_buy_all.x, p_buy_all.y); \
	ar::point p_confirm_blue = params.image_recognition->compareImageReturnCentrePoint(frame, confirm_blue, 0.95f); \
	if (!p_confirm_blue.is_empty) stop_condition = true;
	FUNCTION_LOOPS(f);
#undef f

#define f() \
    ar::point p_confirm_blue = params.image_recognition->compareImageReturnCentrePoint(frame, confirm_blue, 0.95f); \
	if (!p_confirm_blue.is_empty) params.controller->click(p_confirm_blue.x, p_confirm_blue.y); \
	ar::point p_select_all_gray = params.image_recognition->compareImageReturnCentrePoint(frame, select_all_gray, 0.95f); \
	if (!p_select_all_gray.is_empty) stop_condition = true;
	FUNCTION_LOOPS(f);
#undef f


}