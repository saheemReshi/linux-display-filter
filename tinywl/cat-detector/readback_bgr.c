/* SPDX-License-Identifier: MIT */
#include "readback_bgr.h"
#include <stdbool.h>
#include <stdlib.h>
#include <drm_fourcc.h>

uint8_t *cat_readback_to_bgr(const uint8_t *pixels, size_t size_bytes,
		uint32_t width, uint32_t height, size_t stride, uint32_t format) {
	bool rgb_order = format == DRM_FORMAT_ABGR8888 || format == DRM_FORMAT_XBGR8888;
	bool bgr_order = format == DRM_FORMAT_ARGB8888 || format == DRM_FORMAT_XRGB8888;
	/* Keep readback's dimension bounds and check row addressing before use. */
	if (pixels == NULL || (!rgb_order && !bgr_order) ||
			width == 0 || height == 0 || width > 8192 || height > 8192 ||
			stride < (size_t)width * 4 || height > SIZE_MAX / stride ||
			size_bytes < stride * height) {
		return NULL;
	}
	size_t bgr_stride = (size_t)width * 3;
	uint8_t *bgr = malloc(bgr_stride * height);
	if (bgr == NULL) {
		return NULL;
	}
	for (uint32_t y = 0; y < height; ++y) {
		const uint8_t *src = pixels + (size_t)y * stride;
		uint8_t *dst = bgr + (size_t)y * bgr_stride;
		for (uint32_t x = 0; x < width; ++x) {
			const uint8_t *p = src + (size_t)x * 4;
			dst[x * 3 + 0] = rgb_order ? p[2] : p[0];
			dst[x * 3 + 1] = p[1];
			dst[x * 3 + 2] = rgb_order ? p[0] : p[2];
		}
	}
	return bgr;
}
