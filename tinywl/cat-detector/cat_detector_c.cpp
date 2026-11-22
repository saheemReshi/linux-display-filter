// SPDX-License-Identifier: MIT
#include "cat_detector_c.h"
#include "cat_detector.hpp"
#include <cstdio>
#include <limits>
#include <stdexcept>

struct cat_detector {
	CatDetector impl;
};

static void write_error(char *error, size_t size, const char *message) {
	if (error != nullptr && size != 0) {
		std::snprintf(error, size, "%s", message);
	}
}

extern "C" {

struct cat_detector *cat_detector_create(const char *model_path,
		char *error, size_t error_size) {
	write_error(error, error_size, "");
	try {
		if (model_path == nullptr || model_path[0] == '\0') {
			throw std::invalid_argument("Model path is required");
		}
		cv::setNumThreads(1);
		return new cat_detector{CatDetector(model_path)};
	} catch (const std::exception &failure) {
		write_error(error, error_size, failure.what());
	} catch (...) {
		write_error(error, error_size, "Unknown C++ detector error");
	}
	return nullptr;
}

int cat_detector_detect(struct cat_detector *detector,
		const struct cat_image_bgr *image, struct cat_result *result,
		char *error, size_t error_size) {
	write_error(error, error_size, "");
	if (result != nullptr) {
		result->count = 0;
		result->inference_ms = 0.0;
	}
	try {
		if (detector == nullptr || image == nullptr || result == nullptr ||
				image->data == nullptr || image->width <= 0 || image->height <= 0 ||
				(result->capacity != 0 && result->boxes == nullptr)) {
			throw std::invalid_argument("Invalid detector, image, or result buffer");
		}
		const size_t maximum = std::numeric_limits<size_t>::max();
		if (static_cast<size_t>(image->width) > maximum / 3 ||
				image->stride < static_cast<size_t>(image->width) * 3 ||
				static_cast<size_t>(image->height) > maximum / image->stride ||
				image->size_bytes < image->stride * image->height) {
			throw std::invalid_argument("Invalid image stride or byte count");
		}

		// This Mat is a view, not an owner or a copy of the caller's pixels.
		// OpenCV requires a mutable pointer to construct the view, but detect()
		// only reads this input and creates a separate preprocessing tensor.
		cv::Mat bgr(image->height, image->width, CV_8UC3,
			const_cast<uint8_t *>(image->data), image->stride);
		DetectionResult found = detector->impl.detect(bgr);
		result->count = found.cats.size();
		result->inference_ms = found.inference_ms;
		if (result->count > result->capacity) {
			write_error(error, error_size, "Box buffer is too small");
			return CAT_BUFFER_TOO_SMALL;
		}
		for (size_t i = 0; i < found.cats.size(); ++i) {
			const CatDetection &cat = found.cats[i];
			result->boxes[i] = {cat.box.x, cat.box.y, cat.box.width,
				cat.box.height, cat.confidence};
		}
		return CAT_OK;
	} catch (const std::exception &failure) {
		write_error(error, error_size, failure.what());
	} catch (...) {
		write_error(error, error_size, "Unknown C++ detector error");
	}
	return CAT_ERROR;
}

void cat_detector_destroy(struct cat_detector *detector) {
	delete detector;
}

} // extern "C"
