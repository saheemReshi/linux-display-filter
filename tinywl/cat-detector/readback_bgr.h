/* SPDX-License-Identifier: MIT */
#ifndef CAT_READBACK_BGR_H
#define CAT_READBACK_BGR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Accepts TinyWL's four little-endian 8888 DRM formats and dimensions <=8192.
 * Source must cover stride * height bytes. Source pixels are borrowed unchanged.
 * Returns a new tightly packed BGR image (width * 3 bytes per row), or NULL on
 * invalid input/allocation failure. The caller must free() the returned pixels. */
uint8_t *cat_readback_to_bgr(const uint8_t *pixels, size_t size_bytes,
	uint32_t width, uint32_t height, size_t stride, uint32_t format);

#ifdef __cplusplus
}
#endif

#endif
