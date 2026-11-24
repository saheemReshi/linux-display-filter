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
	// Find the largest score first. Subtracting it before exp() prevents huge
	// exponentials while keeping the same normalized probabilities.
	float maximum = *std::max_element(logits, logits + 8);
	float total = 0.0f, weighted = 0.0f;
	for (int i = 0; i < 8; ++i) {
		// exp() converts a model score into a positive softmax weight. Dividing
		// weighted by total below gives the expected distance from 0 to 7.
		float weight = std::exp(logits[i] - maximum);
		total += weight;
		weighted += i * weight;
	}
	return weighted / total;
}

CatDetector::CatDetector(const std::string &model_path)
		// Parse the ONNX file (network structure and trained weights). This
		// happens at construction, not once per frame. Failure throws to the
		// caller; the C interface converts it to NULL and an error message.
		: net_(cv::dnn::readNetFromONNX(model_path)) {
	// OpenCV 5's default graph engine executes this network on CPU.
	net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
	// OpenCV reads BGR pixels. These channel-specific constants belong to
	// this model, not to TinyWL. Letterboxing preserves the aspect ratio.
	// Size holds the model's input width/height. Scalar holds per-channel
	// constants in B,G,R order: (pixel - mean) * scalefactor for each channel.
	params_.size = cv::Size(input_size, input_size);
	params_.mean = cv::Scalar(103.53, 116.28, 123.675);
	params_.scalefactor = cv::Scalar(1.0 / 57.375, 1.0 / 57.12, 1.0 / 58.395);
	// Resize to fit the square and pad the remaining space, without stretching.
	params_.paddingmode = cv::dnn::DNN_PMODE_LETTERBOX;
}

DetectionResult CatDetector::detect(const cv::Mat &image) {
	// empty() rejects missing pixels; type() must be unsigned 8-bit with
	// three channels. These checks do not verify BGR order: callers supply it.
	if (image.empty() || image.type() != CV_8UC3) {
		throw std::invalid_argument("Expected a nonempty 8-bit, three-channel BGR image");
	}
	// Build a separate floating-point tensor (a multidimensional array):
	// resize/pad, normalize channels, and arrange data for the network.
	// The original image stays unchanged.
	cv::Mat blob = cv::dnn::blobFromImageWithParams(image, params_);
	// Give OpenCV the tensor for its next inference; no inference happens yet.
	net_.setInput(blob);
	std::vector<cv::Mat> outputs;
	// A monotonic clock measures only forward(), excluding the preprocessing
	// above and all decoding below. This is not total compositor frame time.
	auto start = std::chrono::steady_clock::now();
	// Ask for the network's final output layer names, execute the network
	// synchronously, and fill outputs with raw prediction tensors.
	net_.forward(outputs, net_.getUnconnectedOutLayersNames());
	auto finish = std::chrono::steady_clock::now();

	// Outputs contain class scores and box-edge scores for each grid size.
	// Cat is class 15 in this model's zero-based COCO labels.
	constexpr int cat_class = 15;
	constexpr float confidence_threshold = 0.5f;
	const std::array<int, 4> strides = {8, 16, 32, 64};
	// Each prediction level supplies two tensors: class scores and edges.
	// Reject layouts this decoder does not understand instead of indexing them.
	if (outputs.empty() || outputs.size() % 2 != 0 || outputs.size() > 8) {
		throw std::runtime_error("Unexpected NanoDet output count");
	}
	std::vector<cv::Rect> boxes;
	std::vector<float> confidences;
	for (size_t level = 0; level < outputs.size() / 2; ++level) {
		// Stride is the spacing of this level's prediction grid in the resized
		// 416x416 image. Different levels handle different object sizes.
		int stride = strides[level];
		int grid = input_size / stride;
		int count = grid * grid;
		// Graph engines may reorder outputs. Match the 80 class channels
		// and 32 edge channels by shape, not by their position in the list.
		cv::Mat scores, edges;
		for (const cv::Mat &out : outputs) {
			// type() checks floating-point storage; dims/size describe its shape.
			if (out.type() != CV_32F || out.dims < 2) {
				continue;
			}
			int channels = out.size[out.dims - 1];
			// total() counts tensor elements. reshape() views the same data as
			// count rows with one channel; it does not copy prediction values.
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
			// row(i) views one grid cell's 80 class scores. minMaxLoc() returns
			// the highest score and its column (label.x). nullptr skips minima.
			cv::minMaxLoc(scores.row(i), nullptr, &confidence, nullptr, &label);
			if (label.x != cat_class || confidence < confidence_threshold) {
				continue;
			}
			float cx = (i % grid) * stride + 0.5f * (stride - 1);
			float cy = (i / grid) * stride + 0.5f * (stride - 1);
			// ptr<float>() borrows this cell's 32 edge scores. Four groups of
			// eight describe left, top, right, and bottom distances respectively.
			const float *p = edges.ptr<float>(i);
			// edge_distance() decodes each group; stride converts grid distances
			// to pixels. clamp() bounds coordinates to the model input. floor()
			// rounds starting edges down and ceil() rounds ending edges up.
			int x1 = static_cast<int>(std::floor(std::clamp(
				cx - stride * edge_distance(p), 0.0f, float(input_size))));
			int y1 = static_cast<int>(std::floor(std::clamp(
				cy - stride * edge_distance(p + 8), 0.0f, float(input_size))));
			int x2 = static_cast<int>(std::ceil(std::clamp(
				cx + stride * edge_distance(p + 16), 0.0f, float(input_size))));
			int y2 = static_cast<int>(std::ceil(std::clamp(
				cy + stride * edge_distance(p + 24), 0.0f, float(input_size))));
			// Rect constructs x/y/width/height. blobRectToImageRect() undoes the
			// model input's resize/padding using the original size from image.size().
			cv::Rect box = params_.blobRectToImageRect(
				cv::Rect(x1, y1, x2 - x1, y2 - y1), image.size());
			// Intersect with the original image boundary; padding predictions
			// must not create rectangles outside the actual output buffer.
			box &= cv::Rect(0, 0, image.cols, image.rows);
			if (!box.empty()) {
				// Append each nonempty candidate and its matching score. These
				// vectors own copies, independent of the raw network tensors.
				boxes.push_back(box);
				confidences.push_back(static_cast<float>(confidence));
			}
		}
	}

	// Several grid cells can predict the same cat. Non-maximum suppression
	// keeps the strongest box and removes heavily overlapping duplicates.
	std::vector<int> keep;
	// NMSBoxes fills keep with indices of retained candidates. 0.6 is the
	// overlap threshold (intersection area / union area) for duplicates.
	cv::dnn::NMSBoxes(boxes, confidences, confidence_threshold, 0.6f, keep);
	DetectionResult result = {};
	for (int i : keep) {
		// Copy only surviving detections into the owning return value.
		result.cats.push_back({boxes[i], confidences[i]});
	}
	// Convert the monotonic time difference to a numeric millisecond count.
	result.inference_ms = std::chrono::duration<double, std::milli>(finish - start).count();
	return result;
}
