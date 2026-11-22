/* SPDX-License-Identifier: MIT */
#ifndef CAT_DETECTOR_C_H
#define CAT_DETECTOR_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cat_detector; /* Opaque: only the C++ implementation knows its contents. */

struct cat_image_bgr {
	const uint8_t *data;
	int width, height;
	size_t stride;     /* Bytes between row starts; at least width * 3. */
	size_t size_bytes; /* Must cover stride * height bytes. */
};

struct cat_box {
	int x, y, width, height;
	float confidence;
};

struct cat_result {
	struct cat_box *boxes; /* Caller-owned array. */
	size_t capacity;
	size_t count;         /* Written by detect; required capacity if too small. */
	double inference_ms;
};

enum cat_status {
	CAT_OK = 0,
	CAT_ERROR = -1,
	CAT_BUFFER_TOO_SMALL = -2,
};

/* Error text is optional and truncated to fit; nonzero buffers are terminated. */
/* Create requests one OpenCV thread (a process-wide OpenCV setting). */
struct cat_detector *cat_detector_create(const char *model_path,
	char *error, size_t error_size);

/* Borrows pixels during the synchronous call. Calls on a handle are sequential.
 * No boxes are written on error or insufficient capacity. Count/timing are reset
 * on entry; CAT_BUFFER_TOO_SMALL reports required count and measured timing. */
int cat_detector_detect(struct cat_detector *detector,
	const struct cat_image_bgr *image, struct cat_result *result,
	char *error, size_t error_size);

/* Release with this function, not free(). NULL is accepted. */
void cat_detector_destroy(struct cat_detector *detector);

#ifdef __cplusplus
}
#endif

#endif
