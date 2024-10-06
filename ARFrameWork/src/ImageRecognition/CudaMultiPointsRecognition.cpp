#if defined(ENABLE_CUDA)
#include <ImageRecognition/CudaMultiPointsRecognition.h>
#include <Log/MiniLog.h>
#include <TemplateMatchMPRCuda.cuh>
#include <vector>
#include <string>

ar::point ar::CudaMultiPointsRecognition::compareImageReturnCentrePoint(cv::Mat& image, cv::Mat& temp, const float& threshold) {
	ar::point point;
	if (image.empty() || temp.empty()) { ar::error("Image empty !"); return point; }
	if (image.rows * image.cols < temp.rows * temp.cols) {
		ar::error("temp size : {} > image size : {} !",
			temp.rows * temp.cols, image.rows * image.cols);
		return point;
	}
	int res[2] = { -1, -1 };
	std::string msg = "";
	if (image.channels() != 1) cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);
	if (temp.channels() != 1) cv::cvtColor(temp, temp, cv::COLOR_BGR2GRAY);
	if (!ar::templateMatchMPRCuda(res, msg, image.data, temp.data, image.cols, image.rows, temp.cols, temp.rows, threshold, num_points))
	{
		ar::error("{}", msg);
		return point;
	}
	if (res[0] != -1 && res[1] != -1)
		point = { (res[0] + temp.cols / 2), (res[1] + temp.rows / 2), 0, false };
	return point;
}

ar::point ar::CudaMultiPointsRecognition::compareImageReturnCentrePoint(const std::string& image_path, const std::string& temp_path, const float& threshold) {
	ar::point point;
	cv::Mat image = cv::imread(image_path);
	cv::Mat temp = cv::imread(temp_path);
	if (image.empty() || temp.empty()) { ar::error("Image empty !"); std::exit(0); }
	if (image.rows * image.cols < temp.rows * temp.cols) {
		ar::error("temp size : {} > image size : {} !",
			temp.rows * temp.cols, image.rows * image.cols);
		return point;
	}
	int res[2] = { -1, -1 };
	std::string msg = "";
	if (image.channels() != 1) cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);
	if (temp.channels() != 1) cv::cvtColor(temp, temp, cv::COLOR_BGR2GRAY);
	if (!ar::templateMatchMPRCuda(res, msg, image.data, temp.data, image.cols, image.rows, temp.cols, temp.rows, threshold, num_points))
	{
		ar::error("{}", msg);
		return point;
	}
	if (res[0] != -1 && res[1] != -1)
		point = { (res[0] + temp.cols) / 2, (res[1] + temp.rows) / 2, 0, false };
	return point;
}

#endif