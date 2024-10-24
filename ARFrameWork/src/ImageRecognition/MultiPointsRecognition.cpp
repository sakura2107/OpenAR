#include <ImageRecognition/MultiPointsRecognition.h>
#include <ImageRecognition/TemplateMatchMPR.h>
#include <Log/MiniLog.h>

ar::point ar::MultiPointsRecognition::compareImageReturnCentrePoint(cv::Mat& image, cv::Mat& temp, const float& threshold) {
	ar::point point;
	if (num_points < 8) ar::warn("num_points < 8 are not recommand !");
	if (temp.empty() || image.empty()) {
		ar::error("Image empty !");
		return point;
	}
	if (temp.rows * temp.cols > image.rows * image.cols) {
		ar::error("temp size : {} > image size : {} !", temp.rows * temp.cols, image.rows * image.cols);
		return point;
	}
	if (temp.channels() != 1) cv::cvtColor(temp, temp, cv::COLOR_BGR2GRAY);
	if (image.channels() != 1) cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);
	int res[2] = { -1, -1 };
	std::string res_msg = "";
	bool err = false;
	err = ar::templateMatchMPR(res,
		res_msg,
		image.data,
		temp.data,
		image.cols,
		image.rows,
		temp.cols,
		temp.rows,
		threshold,
		num_points);
	if (!err) {
		ar::error("{}", res_msg);
		return point;
	}

	if (res[0] != -1 && res[1] != -1)
		point = {(res[0] + temp.cols / 2), (res[1] + temp.rows / 2), 0, false};
	return point;
}

ar::point ar::MultiPointsRecognition::compareImageReturnCentrePoint(const std::string& image_path, const std::string& temp_path, const float& threshold) {
	ar::point point;
	cv::Mat image = cv::imread(image_path);
	cv::Mat temp = cv::imread(temp_path);
	if (num_points < 8) ar::warn("num_points < 8 are not recommand !");
	if (num_points < 8) ar::warn("num_points < 8 are not recommand !");
	if (temp.empty() || image.empty()) {
		ar::error("Image empty !");
		return point;
	}
	if (temp.rows * temp.cols > image.rows * image.cols) {
		ar::error("temp size : {} > image size : {} !", temp.rows * temp.cols, image.rows * image.cols);
		return point;
	}
	if (temp.channels() != 1) cv::cvtColor(temp, temp, cv::COLOR_BGR2GRAY);
	if (image.channels() != 1) cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);
	int res[2] = { -1, -1 };
	std::string res_msg = "";
	bool err = false;
	err = ar::templateMatchMPR(res,
		res_msg,
		image.data,
		temp.data,
		image.cols,
		image.rows,
		temp.cols,
		temp.rows,
		threshold,
		num_points);
	if (!err) {
		ar::error("{}", res_msg);
		return point;
	}

	if (res[0] != -1 && res[1] != -1)
		point = { (res[0] + temp.cols / 2), (res[1] + temp.rows / 2), 0, false };
	return point;
}