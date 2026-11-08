#pragma once
#include <string>

#include <opencv2/opencv.hpp>

namespace ar::vision {

// Reserved for the upcoming YOLO family ("yolo-cpu" / "yolo-gpu"): the
// detector registry will follow the same self-registration pattern as
// MatcherRegistry / OcrRegistry. The interface lands together with the first
// backend; only the result type is fixed here so callers can start structuring
// code around it.
struct Detection {
    cv::Rect box;
    float score = 0.0f;
    int class_id = -1;
    std::string label;
};

} // namespace ar::vision
