#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/features2d.hpp>
#include <vector>

namespace ar{

struct ORBRecognition{
public:
    ORBRecognition(cv::Mat large_img, cv::Mat small_img, int feature_points);
    ORBRecognition(cv::Mat large_img, cv::Mat small_img, float threshold);
    ORBRecognition(cv::Mat large_img, cv::Mat small_img, int feature_points, float threshold);
    bool init_feature_detector();
    bool calc_descriptors();
    bool match_decriptors();
    bool choose_good_matches();
    //debug_func
    bool draw_good_matches(const int& radius, const cv::Scalar color);

private:
    //inputs
    cv::Mat large_img;
    cv::Mat small_img;
    unsigned int feature_points = 1000;
    float threshold = 0.3f;

    cv::Ptr<cv::ORB> orb;
    cv::Mat descriptors_small, descriptors_large;
    std::vector<cv::KeyPoint> keypoints_small, keypoints_large;
    std::vector<cv::DMatch> matches;
    std::vector<cv::DMatch> good_matches;
};

}