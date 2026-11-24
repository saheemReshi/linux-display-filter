// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>
#include <opencv2/dnn.hpp>

struct CatDetection {
	cv::Rect box; // Integer x/y/width/height in the original image's pixels.
	float confidence;
};

struct DetectionResult {
	// Owns box values; destruction releases the vector's allocation.
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
	cv::dnn::Net net_; // Loaded model/weights reused for successive frames.
	cv::dnn::Image2BlobParams params_; // Resize, padding, and channel normalization.
};
