// SPDX-License-Identifier: MIT
// NanoDet normalization and output layout follow the OpenCV Zoo reference:
// https://github.com/opencv/opencv_zoo/tree/main/models/object_detection_nanodet
#include "cat_detector.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <stdexcept>

// Each box edge is predicted as eight scores for distances 0 through 7.
// Softmax turns these scores into probabilities; their mean gives a distance.
static float edge_distance(const float *logits) {
	float maximum = *std::max_element(logits, logits + 8);
	float total = 0.0f, weighted = 0.0f;
	for (int i = 0; i < 8; ++i) {
		float weight = std::exp(logits[i] - maximum);
		total += weight;
		weighted += i * weight;
	}
	return weighted / total;
}

CatDetector::CatDetector(const std::string &model_path)
		: net_(cv::dnn::readNetFromONNX(model_path)) {
	// OpenCV 5's default graph engine executes this network on CPU.
	net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
	// OpenCV reads BGR pixels. These channel-specific constants belong to
	// this model, not to TinyWL. Letterboxing preserves the aspect ratio.
	params_.size = cv::Size(input_size, input_size);
	params_.mean = cv::Scalar(103.53, 116.28, 123.675);
	params_.scalefactor = cv::Scalar(1.0 / 57.375, 1.0 / 57.12, 1.0 / 58.395);
	params_.paddingmode = cv::dnn::DNN_PMODE_LETTERBOX;
}

DetectionResult CatDetector::detect(const cv::Mat &image) {
	if (image.empty() || image.type() != CV_8UC3) {
		throw std::invalid_argument("Expected a nonempty 8-bit, three-channel BGR image");
	}
	cv::Mat blob = cv::dnn::blobFromImageWithParams(image, params_);
	net_.setInput(blob);
	std::vector<cv::Mat> outputs;
	auto start = std::chrono::steady_clock::now();
	net_.forward(outputs, net_.getUnconnectedOutLayersNames());
	auto finish = std::chrono::steady_clock::now();

	// Outputs contain class scores and box-edge scores for each grid size.
	// Cat is class 15 in this model's zero-based COCO labels.
	constexpr int cat_class = 15;
	constexpr float confidence_threshold = 0.5f;
	const std::array<int, 4> strides = {8, 16, 32, 64};
	if (outputs.empty() || outputs.size() % 2 != 0 || outputs.size() > 8) {
		throw std::runtime_error("Unexpected NanoDet output count");
	}
	std::vector<cv::Rect> boxes;
	std::vector<float> confidences;
	for (size_t level = 0; level < outputs.size() / 2; ++level) {
		int stride = strides[level];
		int grid = input_size / stride;
		int count = grid * grid;
		// Graph engines may reorder outputs. Match the 80 class channels
		// and 32 edge channels by shape, not by their position in the list.
		cv::Mat scores, edges;
		for (const cv::Mat &out : outputs) {
			if (out.type() != CV_32F || out.dims < 2) {
				continue;
			}
			int channels = out.size[out.dims - 1];
			if (channels == 80 && out.total() == static_cast<size_t>(count * 80)) {
				scores = out.reshape(1, count);
			} else if (channels == 32 && out.total() == static_cast<size_t>(count * 32)) {
				edges = out.reshape(1, count);
			}
		}
		if (scores.empty() || edges.empty()) {
			throw std::runtime_error("Missing NanoDet outputs for stride " + std::to_string(stride));
		}
		for (int i = 0; i < count; ++i) {
			double confidence;
			cv::Point label;
			cv::minMaxLoc(scores.row(i), nullptr, &confidence, nullptr, &label);
			if (label.x != cat_class || confidence < confidence_threshold) {
				continue;
			}
			float cx = (i % grid) * stride + 0.5f * (stride - 1);
			float cy = (i / grid) * stride + 0.5f * (stride - 1);
			const float *p = edges.ptr<float>(i);
			int x1 = static_cast<int>(std::floor(std::clamp(
				cx - stride * edge_distance(p), 0.0f, float(input_size))));
			int y1 = static_cast<int>(std::floor(std::clamp(
				cy - stride * edge_distance(p + 8), 0.0f, float(input_size))));
			int x2 = static_cast<int>(std::ceil(std::clamp(
				cx + stride * edge_distance(p + 16), 0.0f, float(input_size))));
			int y2 = static_cast<int>(std::ceil(std::clamp(
				cy + stride * edge_distance(p + 24), 0.0f, float(input_size))));
			// Undo the resize and padding, then clip to the original image.
			cv::Rect box = params_.blobRectToImageRect(
				cv::Rect(x1, y1, x2 - x1, y2 - y1), image.size());
			box &= cv::Rect(0, 0, image.cols, image.rows);
			if (!box.empty()) {
				boxes.push_back(box);
				confidences.push_back(static_cast<float>(confidence));
			}
		}
	}

	// Several grid cells can predict the same cat. Non-maximum suppression
	// keeps the strongest box and removes heavily overlapping duplicates.
	std::vector<int> keep;
	cv::dnn::NMSBoxes(boxes, confidences, confidence_threshold, 0.6f, keep);
	DetectionResult result = {};
	for (int i : keep) {
		result.cats.push_back({boxes[i], confidences[i]});
	}
	result.inference_ms = std::chrono::duration<double, std::milli>(finish - start).count();
	return result;
}
