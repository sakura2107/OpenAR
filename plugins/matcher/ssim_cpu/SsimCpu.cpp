#include "SsimCpu.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ar {

namespace {

constexpr int32_t kWorkingSize = 640;

// SSIM constants (identical to the ssim-cuda backend):
// C1 = (0.01 * 255)^2, C2 = (0.03 * 255)^2, C3 = C2 / 2.
constexpr float kC1 = 6.5025f;
constexpr float kC2 = 58.5225f;
constexpr float kC3 = 29.26125f;

bool validate_inputs(const cv::Mat &image, const cv::Mat &templ, int32_t max_input_bytes,
                     ar::log::ILogger *logger) {
    if (image.empty() || templ.empty()) {
        AR_LOG(logger, ar::log::Level::error, "Image empty !");
        return false;
    }
    if (templ.rows > image.rows || templ.cols > image.cols) {
        AR_LOG(logger, ar::log::Level::error, "Template is larger than the image !");
        return false;
    }
    if (image.total() * image.elemSize() > static_cast<size_t>(max_input_bytes) ||
        templ.total() * templ.elemSize() > static_cast<size_t>(max_input_bytes)) {
        AR_LOG(logger, ar::log::Level::error, "Image too large is not allow !");
        return false;
    }
    if (image.type() != templ.type() || (image.type() != CV_8UC3 && image.type() != CV_8UC1)) {
        AR_LOG(logger, ar::log::Level::error, "Both image type must be bgr or gray !");
        return false;
    }
    return true;
}

// Windowed SSIM for one channel, computed with three windowed reductions
// (mean of I, mean of I^2, correlation with the centered template). With
// anchor (0,0) every map pixel p corresponds to the window whose top-left is
// p; only the sub-rectangle of fully-inside windows is meaningful.
cv::Mat ssim_channel(const cv::Mat &image, const cv::Mat &templ, float templ_mean, float templ_var,
                     float inv_area) {
    const int out_w = image.cols - templ.cols + 1;
    const int out_h = image.rows - templ.rows + 1;
    const cv::Size window(templ.cols, templ.rows);
    const cv::Point anchor(0, 0);

    cv::Mat image_f;
    image.convertTo(image_f, CV_32F);

    cv::Mat mean_i, mean_i2, corr;
    cv::boxFilter(image_f, mean_i, CV_32F, window, anchor, true, cv::BORDER_REPLICATE);
    cv::boxFilter(image_f.mul(image_f), mean_i2, CV_32F, window, anchor, true, cv::BORDER_REPLICATE);
    cv::Mat centered_kernel;
    templ.convertTo(centered_kernel, CV_32F);
    centered_kernel -= templ_mean;
    cv::filter2D(image_f, corr, CV_32F, centered_kernel, anchor, 0, cv::BORDER_REPLICATE);

    // Valid windows only.
    mean_i = mean_i(cv::Rect(0, 0, out_w, out_h));
    mean_i2 = mean_i2(cv::Rect(0, 0, out_w, out_h));
    corr = corr(cv::Rect(0, 0, out_w, out_h));

    // cov(I, T) = sum(I * (T - mean_T)) / area, since sum(T - mean_T) == 0.
    cv::Mat covar = corr * inv_area;
    cv::Mat var_i = mean_i2 - mean_i.mul(mean_i);
    cv::max(var_i, 0.0f, var_i); // guard tiny negatives from float cancellation

    cv::Mat sqrt_var_i;
    cv::sqrt(var_i, sqrt_var_i);
    const float sqrt_var_t = std::sqrt(templ_var);

    cv::Mat luminance =
        (mean_i * (2 * templ_mean) + kC1) / (mean_i.mul(mean_i) + templ_mean * templ_mean + kC1);
    cv::Mat contrast = (sqrt_var_i * (2 * sqrt_var_t) + kC2) / (var_i + templ_var + kC2);
    cv::Mat structure = (covar + kC3) / (sqrt_var_i * sqrt_var_t + kC3);
    return luminance.mul(contrast).mul(structure);
}

} // namespace

ar::vision::MatchResult SsimCpuMatcher::find(const cv::Mat &image, const cv::Mat &templ, float threshold,
                                             const cv::Rect *roi, ar::log::ILogger *logger) {
    ar::vision::MatchResult result;
    if (!validate_inputs(image, templ, max_input_bytes_, logger))
        return result;

    cv::Rect search_rect(0, 0, image.cols, image.rows);
    if (roi) {
        if (roi->x < 0 || roi->y < 0 || roi->width <= 0 || roi->height <= 0 ||
            roi->x + roi->width > image.cols || roi->y + roi->height > image.rows) {
            AR_LOG(logger, ar::log::Level::error, "Image match range is outside image bounds");
            return result;
        }
        if (templ.cols > roi->width || templ.rows > roi->height)
            return result;
        search_rect = *roi;
    }

    const cv::Mat search = image(search_rect);

    const float ratio =
        static_cast<float>(kWorkingSize) / static_cast<float>(std::max(search.cols, search.rows));
    const auto scaled = [ratio](int32_t value) {
        return std::max(1, static_cast<int32_t>(std::lround(value * ratio)));
    };
    cv::Mat search_resized;
    cv::Mat templ_resized;
    cv::resize(search, search_resized, cv::Size(scaled(search.cols), scaled(search.rows)), 0, 0,
               cv::INTER_LINEAR);
    cv::resize(templ, templ_resized, cv::Size(scaled(templ.cols), scaled(templ.rows)), 0, 0,
               cv::INTER_LINEAR);
    if (templ_resized.cols > search_resized.cols || templ_resized.rows > search_resized.rows)
        return result;

    const int channels = search_resized.channels();
    const float inv_area = 1.0f / static_cast<float>(templ_resized.cols * templ_resized.rows);

    std::vector<cv::Mat> search_channels;
    std::vector<cv::Mat> templ_channels;
    cv::split(search_resized, search_channels);
    cv::split(templ_resized, templ_channels);

    cv::Mat ssim_sum;
    for (int c = 0; c < channels; ++c) {
        cv::Mat templ_f;
        templ_channels[c].convertTo(templ_f, CV_32F);
        const double mean = cv::mean(templ_f)[0];
        cv::Mat centered = templ_f - mean;
        const double var = centered.dot(centered) / templ_f.total();

        const cv::Mat channel_ssim =
            ssim_channel(search_channels[c], templ_channels[c], static_cast<float>(mean),
                         static_cast<float>(var), inv_area);
        ssim_sum = c == 0 ? channel_ssim : ssim_sum + channel_ssim;
    }
    if (ssim_sum.empty())
        return result;
    cv::Mat ssim_map = ssim_sum / static_cast<float>(channels);

    double best_score = -1.0;
    cv::Point best_pos(-1, -1);
    cv::minMaxLoc(ssim_map, nullptr, &best_score, nullptr, &best_pos);

    result.score = static_cast<float>(best_score);
    if (result.score >= threshold) {
        const cv::Point top_left(static_cast<int32_t>(std::lround(best_pos.x / ratio)) + search_rect.x,
                                 static_cast<int32_t>(std::lround(best_pos.y / ratio)) + search_rect.y);
        result.found = true;
        result.box = cv::Rect(top_left, templ.size());
        result.center = cv::Point(top_left.x + templ.cols / 2, top_left.y + templ.rows / 2);
    }
    return result;
}

} // namespace ar
