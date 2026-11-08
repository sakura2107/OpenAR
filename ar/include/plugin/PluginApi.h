#pragma once
// ============================================================================
//  ARFrameWork plugin ABI (stable contract, plain C, no C++ types).
//
//  A plugin is a Windows DLL dropped into the "plugins" directory next to the
//  executable. At startup the framework scans the directory, checks the ABI
//  version and registers every backend the DLL exposes behind the matching
//  C++ interface (IImageMatcher / IController / IDeviceController /
//  IOcrEngine), so script code cannot tell a plugin from a built-in backend.
//
//  A DLL may implement any of the four families below; each family is detected
//  by the presence of its create export and registered under the name
//  reported by its backend_name export.
//
//  Images are passed as interleaved bytes + row stride in bytes
//  (cv::Mat::step); plugins read/write the caller's memory directly, no pixel
//  data is ever copied across the boundary except screenshots, which the
//  plugin allocates and the framework copies and releases via the plugin's
//  free_image export.
//
//  Log level values match ar::log::Level: 0=debug 1=info 2=warning 3=error
//  4=fatal. Device status values match ar::device::Status: ok=1, offline=2,
//  unknown=0, invalid_command=-1, invalid_index=-2.
// ============================================================================
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Bump whenever the signatures in this header change; the loader refuses
// plugins whose version differs from its own.
#define ARFW_PLUGIN_ABI_VERSION 1

enum ArfwPluginStatus {
    ARFW_PLUGIN_OK = 0,
    ARFW_PLUGIN_ERR_ARGS = 1,   // invalid arguments / dimensions
    ARFW_PLUGIN_ERR_FAILED = 2, // backend failure, message in err_buf
};

// Logging callback handed to the plugin at create time (may be null).
typedef void (*arfw_log_fn)(void *user, int32_t level, const char *message);

// ---------------------------------------------------------------------------
// Every family requires the name/version/destroy exports shown here, plus the
// family-specific create/find/etc. All are plain C, marked ARFW_PLUGIN_EXPORT:
//
//   int32_t      arfw_plugin_abi_version(void);
//   const char*  arfw_plugin_backend_name(void);
//   void         arfw_xxx_destroy(void* handle);
// ---------------------------------------------------------------------------

// --- image matcher (ar::vision::IImageMatcher) ------------------------------
//
//   void*   arfw_matcher_create(arfw_log_fn log, void* user);
//   int32_t arfw_matcher_prepare(void* m, const uint8_t* templ_data,
//                                int64_t templ_step, int32_t templ_w,
//                                int32_t templ_h, int32_t channels);
//   int32_t arfw_matcher_find(void* m,
//                             const uint8_t* image_data, int64_t image_step,
//                             int32_t image_w, int32_t image_h, int32_t channels,
//                             const uint8_t* templ_data, int64_t templ_step,
//                             int32_t templ_w, int32_t templ_h, float threshold,
//                             int32_t roi_x, int32_t roi_y,
//                             int32_t roi_w, int32_t roi_h,  /* roi_w <= 0: whole image */
//                             float* out_score, int32_t* out_x, int32_t* out_y,
//                             char* err_buf, int32_t err_buf_size);
//
// find: on OK, *out_score is the best similarity and *out_x/*out_y the best
// window's top-left in full-image coordinates, or -1 when below `threshold`.

// --- controller (ar::control::IController) ----------------------------------
typedef struct ArfwControllerConfig {
    const char *adb_path; /* null -> ".\adb.exe" */
    int32_t adb_port;
    const char *mumu_path; /* null or "" -> working directory */
    int32_t mumu_index;
} ArfwControllerConfig;
//
//   void*   arfw_controller_create(const ArfwControllerConfig* config,
//                                  arfw_log_fn log, void* user);
//   int32_t arfw_controller_connect(void* c);
//   int32_t arfw_controller_disconnect(void* c);
//   int32_t arfw_controller_click(void* c, int32_t x, int32_t y);
//   int32_t arfw_controller_long_press(void* c, int32_t x, int32_t y, int32_t ms);
//   int32_t arfw_controller_swipe(void* c, int32_t x1, int32_t y1,
//                                 int32_t x2, int32_t y2, int32_t ms);
//   int32_t arfw_controller_press_key(void* c, int32_t keycode);
//   /* Screenshot: the DLL mallocs a tightly packed BGR buffer; the framework
//      copies it and hands it back through arfw_controller_free_image. */
//   int32_t arfw_controller_screenshot(void* c, int32_t* out_w, int32_t* out_h,
//                                      int32_t* out_channels, uint8_t** out_data);
//   void    arfw_controller_free_image(void* c, uint8_t* data);

// --- device controller (ar::device::IDeviceController) ----------------------
typedef struct ArfwAppState {
    int32_t installed;
    int32_t running;
} ArfwAppState;
//
//   void*   arfw_device_create(const char* device_path, int32_t index,
//                              arfw_log_fn log, void* user);
//   int32_t arfw_device_check_state(void* d);      /* Status values */
//   int32_t arfw_device_launch(void* d);
//   int32_t arfw_device_shutdown(void* d);
//   int32_t arfw_device_restart(void* d);
//   int32_t arfw_device_show_window(void* d);
//   int32_t arfw_device_hide_window(void* d);
//   int32_t arfw_device_launch_app(void* d, const char* package);
//   int32_t arfw_device_close_app(void* d, const char* package);
//   int32_t arfw_device_app_state(void* d, const char* package,
//                                 ArfwAppState* out_state);

// --- OCR engine (ar::vision::IOcrEngine) ------------------------------------
typedef struct ArfwOcrParams {
    const char *det_model;  /* detection onnx */
    const char *rec_model;  /* recognition onnx */
    const char *dictionary; /* character dictionary file */
    float text_threshold;
    float box_threshold;
    float unclip_ratio;
    int32_t score_mode;
    int32_t device_id; /* gpu backends; <0 resolves AR_OCR_CUDA_DEVICE */
} ArfwOcrParams;

typedef struct ArfwOcrItem {
    char text[512]; /* UTF-8, truncated to fit */
    float score;
    int32_t box_x, box_y, box_w, box_h;
} ArfwOcrItem;
//
//   void*   arfw_ocr_create(const ArfwOcrParams* params, arfw_log_fn log, void* user);
//   /* Recognize: may be called with out_items=null/max_items=0 to query the
//      item count; *out_count always receives the total number of items. */
//   int32_t arfw_ocr_recognize(void* ocr,
//                              const uint8_t* image_data, int64_t image_step,
//                              int32_t image_w, int32_t image_h, int32_t channels,
//                              int32_t roi_x, int32_t roi_y,
//                              int32_t roi_w, int32_t roi_h,  /* roi_w <= 0: whole image */
//                              ArfwOcrItem* out_items, int32_t max_items,
//                              int32_t* out_count,
//                              char* err_buf, int32_t err_buf_size);

#if defined(_WIN32)
#ifdef __cplusplus
#define ARFW_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define ARFW_PLUGIN_EXPORT __declspec(dllexport)
#endif
#else
#define ARFW_PLUGIN_EXPORT
#endif

#ifdef __cplusplus
} // extern "C"
#endif
