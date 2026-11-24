// SPDX-License-Identifier: MIT
#include "cat_detector.hpp"
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <opencv2/imgcodecs.hpp>

// Standalone image-file tester. TinyWL calls cat_detector_c.cpp instead of
// launching this executable, so there is no file-loading step per live frame.
int main(int argc, char **argv) {
	if (argc != 3) {
		std::cerr << "Usage: " << argv[0] << " MODEL.onnx IMAGE\n";
		return 2;
	}
	try {
		// Limit OpenCV CPU parallelism for this standalone test process.
		cv::setNumThreads(1);
		// Decode the file into an owned, three-channel BGR image. empty()
		// below detects a failed load; OpenCV releases its pixels automatically.
		cv::Mat image = cv::imread(argv[2], cv::IMREAD_COLOR);
		if (image.empty()) {
			throw std::runtime_error("Could not read input image");
		}
		// Load weights once, then synchronously detect cats in the loaded image.
		CatDetector detector(argv[1]);
		DetectionResult result = detector.detect(image);
		std::cout << "image_width=" << image.cols << " image_height=" << image.rows << '\n';
		// These stream settings print decimal scores with three fractional digits.
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
		// what() supplies the failure description; exit 1 signals a failed run.
		std::cerr << "Cat detector: " << error.what() << '\n';
		return 1;
	}
}
