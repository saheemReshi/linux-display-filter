// SPDX-License-Identifier: MIT
#include "cat_detector_c.h"
#include "cat_detector.hpp"
#include <cstdio>
#include <limits>
#include <stdexcept>

// C sees only a handle; this implementation owns the actual C++ detector.
struct cat_detector {
	CatDetector impl;
};

static void write_error(char *error, size_t size, const char *message) {
	if (error != nullptr && size != 0) {
		// Copy at most size-1 characters and terminate with '\0'. The caller
		// owns the buffer; formatting as "%s" treats message as plain text.
		std::snprintf(error, size, "%s", message);
	}
}

extern "C" {

struct cat_detector *cat_detector_create(const char *model_path,
		char *error, size_t error_size) {
	write_error(error, error_size, "");
	try {
		if (model_path == nullptr || model_path[0] == '\0') {
			// Throw locally; the catch below turns C++ exceptions into C errors.
			throw std::invalid_argument("Model path is required");
		}
		// Ask OpenCV for one CPU thread. This setting is process-wide; we do
		// not create a detector worker or move work off TinyWL's event loop.
		cv::setNumThreads(1);
		// Construct the network once and return an owning handle to C.
		return new cat_detector{CatDetector(model_path)};
	} catch (const std::exception &failure) {
		// what() describes the exception. Copy it before the exception dies;
		// no exception may escape across this C-facing interface.
		write_error(error, error_size, failure.what());
	} catch (...) {
		write_error(error, error_size, "Unknown C++ detector error");
	}
	return nullptr;
}

int cat_detector_detect(struct cat_detector *detector,
		const struct cat_image_bgr *image, struct cat_result *result,
		char *error, size_t error_size) {
	// Clear any stale message and reset output metadata for this attempt.
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
		// Get the largest representable byte count. Validate row lengths and
		// multiplication before OpenCV is allowed to read the caller's memory.
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
		// Run preprocessing, inference, and decoding now. The returned vector
		// owns its box values; bgr still only borrows the caller's pixels.
		DetectionResult found = detector->impl.detect(bgr);
		// size() counts detections, not allocated space in the caller's array.
		result->count = found.cats.size();
		result->inference_ms = found.inference_ms;
		if (result->count > result->capacity) {
			// Report the required size without copying an incomplete box list.
			write_error(error, error_size, "Box buffer is too small");
			return CAT_BUFFER_TOO_SMALL;
		}
		for (size_t i = 0; i < found.cats.size(); ++i) {
			// Copy plain coordinates/confidence into C structs. No OpenCV object
			// or pointer to the temporary vector escapes into TinyWL.
			const CatDetection &cat = found.cats[i];
			result->boxes[i] = {cat.box.x, cat.box.y, cat.box.width,
				cat.box.height, cat.confidence};
		}
		return CAT_OK;
	} catch (const std::exception &failure) {
		// Convert OpenCV/standard-library failures to text and CAT_ERROR.
		write_error(error, error_size, failure.what());
	} catch (...) {
		write_error(error, error_size, "Unknown C++ detector error");
	}
	return CAT_ERROR;
}

void cat_detector_destroy(struct cat_detector *detector) {
	// delete invokes CatDetector/OpenCV destructors and frees the handle.
	// It also accepts nullptr; C callers must use this instead of free().
	delete detector;
}

} // extern "C"
