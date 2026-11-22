// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>
#include <opencv2/dnn.hpp>

struct CatDetection {
	cv::Rect box;
	float confidence;
};

struct DetectionResult {
	std::vector<CatDetection> cats;
	double inference_ms;
};

class CatDetector {
public:
	// Load the network once; it stays alive for this object's lifetime.
	explicit CatDetector(const std::string &model_path);

	// Borrow a nonempty CV_8UC3 BGR image for this synchronous call.
	// The returned result owns its box values. Calls must be sequential.
	DetectionResult detect(const cv::Mat &image);

private:
	static constexpr int input_size = 416;
	cv::dnn::Net net_;
	cv::dnn::Image2BlobParams params_;
};
