// SPDX-License-Identifier: MIT
#include "cat_detector.hpp"
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <opencv2/imgcodecs.hpp>

int main(int argc, char **argv) {
	if (argc != 3) {
		std::cerr << "Usage: " << argv[0] << " MODEL.onnx IMAGE\n";
		return 2;
	}
	try {
		cv::setNumThreads(1);
		cv::Mat image = cv::imread(argv[2], cv::IMREAD_COLOR);
		if (image.empty()) {
			throw std::runtime_error("Could not read input image");
		}
		CatDetector detector(argv[1]);
		DetectionResult result = detector.detect(image);
		std::cout << "image_width=" << image.cols << " image_height=" << image.rows << '\n';
		std::cout << std::fixed << std::setprecision(3);
		for (const CatDetection &cat : result.cats) {
			const cv::Rect &box = cat.box;
			std::cout << "cat confidence=" << cat.confidence
				<< " x=" << box.x << " y=" << box.y
				<< " width=" << box.width << " height=" << box.height << '\n';
		}
		if (result.cats.empty()) {
			std::cout << "no cats detected\n";
		}
		std::cerr << "First inference: " << result.inference_ms << " ms (CPU; OpenCV threads=1)\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "Cat detector: " << error.what() << '\n';
		return 1;
	}
}
