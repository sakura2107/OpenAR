#pragma once
#include "plugin/PluginApi.h"
#ifdef __cplusplus
extern "C" {
#endif
#define ARFW_PLUGIN_ABI_V2 2
#define ARFW_FAMILY_MATCHER 1u
#define ARFW_FAMILY_CONTROLLER 2u
#define ARFW_FAMILY_DEVICE 4u
#define ARFW_FAMILY_OCR 8u
#define ARFW_CAP_CONFIG 1u
#define ARFW_CAP_CONTEXT 2u
#define ARFW_CAP_OCR_RESULT 4u
typedef int32_t (*arfw_cancel_fn)(void *user);
typedef enum ArfwErrorCodeV2 {
    ARFW_ERROR_INVALID_ARGUMENT = 0,
    ARFW_ERROR_UNSUPPORTED = 1,
    ARFW_ERROR_OFFLINE = 2,
    ARFW_ERROR_TIMEOUT = 3,
    ARFW_ERROR_CANCELLED = 4,
    ARFW_ERROR_DEPENDENCY_MISSING = 5,
    ARFW_ERROR_MODEL_INVALID = 6,
    ARFW_ERROR_BACKEND_FAILED = 7,
    ARFW_ERROR_IO_FAILED = 8,
    ARFW_ERROR_RESOURCE_INVALID = 9,
    ARFW_ERROR_CONFLICT = 10,
    ARFW_ERROR_STALE_FRAME = 11,
    ARFW_ERROR_BUDGET_EXCEEDED = 12
} ArfwErrorCodeV2;
typedef struct ArfwErrorV2 {
    uint32_t struct_size;
    int32_t code;
    int64_t native_code;
    const char *operation;
    const char *message;
} ArfwErrorV2;
/* All structures use the platform's natural alignment, x64 only. UTF-8 text.
   Input pointers are borrowed during the call. Output text lives until destroy.
   One instance may be called by one thread at a time; distinct instances are independent. */
typedef struct ArfwPluginDescriptorV2 {
    uint32_t struct_size, abi_version;
    const char *backend_name;
    uint32_t families, capabilities;
} ArfwPluginDescriptorV2;
typedef struct ArfwImageViewV2 {
    uint32_t struct_size;
    const uint8_t *data;
    uint64_t byte_length;
    int64_t stride;
    int32_t width, height, channels;
} ArfwImageViewV2;
typedef struct ArfwRectV2 {
    int32_t x, y, width, height;
} ArfwRectV2;
typedef struct ArfwMatcherConfigV2 {
    uint32_t struct_size;
    int32_t max_input_bytes;
} ArfwMatcherConfigV2;
typedef struct ArfwControllerConfigV2 {
    uint32_t struct_size;
    ArfwControllerConfig base;
    const char *serial;
    uint32_t command_timeout_ms;
} ArfwControllerConfigV2;
typedef struct ArfwOcrParamsV2 {
    uint32_t struct_size;
    ArfwOcrParams base;
    float det_line_merge_ratio;
} ArfwOcrParamsV2;
typedef struct ArfwOcrItemV2 {
    uint32_t struct_size;
    const char *text;
    uint64_t text_length;
    float score;
    ArfwRectV2 box;
} ArfwOcrItemV2;
/* Additional exports; all legacy v1 exports remain available.
 const ArfwPluginDescriptorV2* arfw_plugin_descriptor_v2(void);
 void* arfw_matcher_create_v2(const ArfwMatcherConfigV2*, arfw_log_fn, void*, char*, int32_t);
 int32_t arfw_matcher_find_v2(void*, const ArfwImageViewV2*, const ArfwImageViewV2*, float, const ArfwRectV2*,
 float*, int32_t*, int32_t*, char*, int32_t); void* arfw_controller_create_v2(const ArfwControllerConfigV2*,
 arfw_log_fn, void*, char*, int32_t); int32_t arfw_controller_set_context_v2(void*, arfw_cancel_fn, void*,
 uint32_t timeout_ms); int32_t arfw_controller_last_error_v2(void*, char*, int32_t); int32_t
 arfw_controller_error_v2(void*, ArfwErrorV2*); // borrowed strings until next operation int32_t
 arfw_controller_screenshot_v2(void*, ArfwImageViewV2*); // packed output; release with v1 free_image int32_t
 arfw_controller_start_app_v2(void*, const char*); int32_t arfw_controller_stop_app_v2(void*, const char*);
 void* arfw_ocr_create_v2(const ArfwOcrParamsV2*, arfw_log_fn, void*, char*, int32_t);
 int32_t arfw_ocr_run_v2(void*, const ArfwImageViewV2*, const ArfwRectV2*, void**, char*, int32_t);
 int32_t arfw_ocr_result_count_v2(void* result);
 int32_t arfw_ocr_result_item_v2(void* result, int32_t index, ArfwOcrItemV2*);
 void arfw_ocr_result_destroy_v2(void* result);
 Optional additive OCR exports (v1/v2 layouts unchanged):
 void* arfw_ocr_create_profile_v3(const char* profile_utf8, int32_t device_id, arfw_log_fn, void*, char*, int32_t);
 int32_t arfw_ocr_run_mode_v3(void*, const ArfwImageViewV2*, const ArfwRectV2*, int32_t mode,
                            void** result, double* det_ms, double* rec_ms, char*, int32_t);
 mode: 0=det+rec, 1=already-cropped text, 2=already-cropped numeric CTC, 3=det only.
 Result ownership and access use arfw_ocr_result_*_v2. Timings exclude model loading and disk I/O.
*/
#ifdef __cplusplus
}
#endif
