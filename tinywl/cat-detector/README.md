# Standalone cat detector

The standalone command reads an image and prints cat bounding boxes. The same
library is also linked into TinyWL for synchronous per-rendered-frame cat
masking. It uses C++ and OpenCV's CPU DNN backend, with one OpenCV thread
requested. No Python runtime is involved.

## Dependency and build

On Arch/Omarchy, install the C++ development library if needed:

```sh
sudo pacman -S --needed opencv libdrm
```

Build from the TinyWL directory:

```sh
make -C cat-detector
```

The Makefile uses `opencv5`, the pkg-config name of Arch's OpenCV 5 package.
It links only DNN, image codecs, image processing, and core; optional GUI and
3D visualization modules are not needed.

## Model and run

The model is OpenCV Zoo's NanoDet-m-plus-1.5x_416, about 3.8 MB. Its COCO
class list includes cats. Run the following from the `cat-detector` directory:

```sh
curl --fail --location --output nanodet.onnx https://media.githubusercontent.com/media/opencv/opencv_zoo/main/models/object_detection_nanodet/object_detection_nanodet_2022nov.onnx
sha256sum --check model.sha256
./detect-cats nanodet.onnx /path/to/cat.jpg
```

The input is letterboxed to 416 by 416,
then normalized with the model's BGR channel means and standard deviations.
The program decodes the raw outputs, keeps cats with confidence at least 0.5,
and suppresses overlapping duplicate predictions using an IoU threshold of 0.6.

Printed `x`, `y`, `width`, and `height` are in the original image's pixels,
with a top-left origin. An image with no accepted cats is a successful run
and prints `no cats detected`. Usage errors return 2; load/inference errors
return 1. The first model-forward duration is printed to standard error;
it excludes image decoding, preprocessing, and box decoding, and is not a
steady-state frame-rate measurement.

The local image matrices release their resources when they leave scope. The
detector retains its network until the detector object is destroyed. There are
no sockets or worker threads in the detector. The standalone command does not
render; TinyWL's integration performs the GPU mask pass and output commit.

## Reusing the detector

`cat_detector.hpp` declares `CatDetector`, `CatDetection`, and `DetectionResult`.
`cat_detector.cpp` implements loading, preprocessing, inference, and decoding.
`main.cpp` only loads the image file, calls the detector, and prints the result.

```cpp
CatDetector detector("nanodet.onnx"); // Load once.
DetectionResult first = detector.detect(first_bgr_image);
DetectionResult second = detector.detect(second_bgr_image);
```

Each input must be a nonempty `CV_8UC3` image: 8-bit BGR with three channels.
The detector borrows it during the synchronous call and does not modify or
retain it. Each result owns a vector of boxes/confidences and the forward-pass
duration in milliseconds. Previous results remain valid after another call.

The constructor stores the loaded network and preprocessing settings as object
members. `detect()` creates fresh local image/output arrays and candidate lists
on every call; it never reloads the model or carries old boxes into a new result.
Calls on one detector must be sequential because its network has mutable input
and execution state. OpenCV's global thread count remains the caller's choice;
the standalone program requests one thread before constructing its detector.

This is the C++ interface. C callers use the wrapper described below.

## C interface

`cat_detector_c.h` is usable from C; it exposes no OpenCV or C++ container types.
`cat_detector_c.cpp` implements the wrapper. The build also produces the static
library `libcat-detector.a`. Compile a consumer as C, then use the C++ driver for
the final link with that library and the OpenCV libraries. The existing C++ CLI
continues to work.

- `cat_detector_create` loads the model and returns an opaque handle, or NULL
  with error text. It requests one OpenCV thread, a process-wide setting.
- `cat_detector_detect` borrows a `cat_image_bgr` and writes a `cat_result`.
  Pixels are 8-bit BGR (three bytes per pixel); the supplied allocation must
  cover `stride * height` bytes, including any row padding. Dimensions, stride,
  byte count, and overflow are checked before constructing an OpenCV view.
- The caller owns both pixels and the result's box array. Set `boxes` and
  `capacity` before calling; no cross-language pixel/result allocation is needed.
- `CAT_OK` means all boxes were copied; zero cats is a successful result.
  `CAT_BUFFER_TOO_SMALL` means no boxes were copied, and `count` tells the caller
  the required capacity. `CAT_ERROR` reports invalid input or an exception.
  Count and timing reset on entry; insufficient capacity returns actual count
  and inference timing. Boxes remain unchanged on either failure status.
- Error text uses a caller-owned buffer. It is cleared on success and truncated
  with a terminating zero byte when space is limited. Passing NULL omits text.
- The wrapper catches C++ exceptions and returns errors to C. Results contain
  plain integer coordinates and float confidence values; they outlive the call.
- `cat_detector_destroy` releases the handle. Do not call `free` on it. NULL is
  accepted. Calls on one handle must be sequential.

The C detector accepts BGR pixels only. TinyWL converts its readback pixels
before calling it from the rendering callback.

## Preparing readback pixels

`readback_bgr.h` declares `cat_readback_to_bgr`; `readback_bgr.c` implements it in
C. It is included in `libcat-detector.a` and called by TinyWL. It uses
the existing libdrm format headers; it has no OpenCV dependency of its own.

Pass the CPU pixels, their allocation size, dimensions, source stride, and DRM
format. The supported layouts match TinyWL's current little-endian readback:

| DRM format | Source bytes | BGR output |
| --- | --- | --- |
| ARGB8888 / XRGB8888 | B G R A-or-X | B G R |
| ABGR8888 / XBGR8888 | R G B A-or-X | B G R |

The converter validates the format, nonzero dimensions up to 8192, row stride,
byte count, and row-addressing overflow. It allocates `width * 3 * height` bytes
and copies pixels while omitting the fourth channel. Source row padding is
skipped, and the destination has no padding. No resizing or normalization occurs
here; those are already part of the detector.

NULL means invalid input or allocation failure. On success the source remains
unchanged and the caller owns the returned pixels. Set `cat_image_bgr.stride` to
`width * 3` and `size_bytes` to `width * 3 * height` when passing that buffer to
the detector. Keep it alive until the synchronous detection call returns, then
release it with `free()`.

## TinyWL integration

Run from the TinyWL directory (one level above this README's directory):

```sh
make
WLR_BACKENDS=wayland WLR_RENDERER=gles2 ./tinywl -s foot
```

On a virtual terminal, omit `WLR_BACKENDS=wayland` as in the existing running
instructions. This synchronous GPU path requires the GLES2 renderer.

The model is loaded once before the backend starts. The default path is
`cat-detector/nanodet.onnx`, relative to the working directory. Set
`TINYWL_CAT_MODEL` to use another path. A load failure prints an error and exits.

Every rendered frame fully repaints the scene, reads the output buffer once,
converts it to BGR, runs detection, and draws opaque grey rectangles over all
accepted cat boxes in an additional GPU pass. `glFinish()` waits while the GLES2
pass context is current; only then is the pass finalized and output committed.
If detection or rendering fails, that frame is not committed. The normal
scene-needs-frame check is retained: an unchanged desktop does not run inference
repeatedly solely to match the monitor's refresh rate.

Repainting and commit damage cover the whole output in this prototype, so old
masks disappear when cats move or are no longer detected.

There is one detector per server, with sequential calls across outputs. Each
output retains a reusable box array, growing it and retrying detection if
necessary. Readback and BGR allocations are freed each frame; output box storage
is freed when its output is destroyed, and the detector at server shutdown.

Debug logs report cat counts and model-forward time for processed frames; that
time excludes rendering, readback, conversion, and grey drawing. This deliberately
synchronous prototype can reduce frame rate and delay input while processing.

To test, open a cat photo inside TinyWL. Grey regions should appear without
pressing a key. Move the image and then close it to check mask movement/removal.
Alt+Escape exits TinyWL; Alt+F1 cycles between windows. Detection and masking
run automatically, without experimental capture or overlay keybindings.

## Reading the implementation

`cat_detector.cpp` contains the box-decoding function, the constructor, and
`detect()`. The command-line entry point is in `main.cpp`. The sequence is:

1. `imread` loads the image as BGR pixels into a `cv::Mat`, OpenCV's image/array
   container. The constructor's `readNetFromONNX` loads the network structure
   and trained weights once. Both use automatic resource management.
2. `Image2BlobParams` describes `(pixel - mean) * scale` for each color channel.
   Letterboxing resizes without stretching and adds padding to reach 416 square.
   `blobFromImageWithParams` packs the result as a floating-point tensor with
   dimensions batch, channels, height, width: 1 by 3 by 416 by 416.
3. `setInput` provides that tensor; `forward` synchronously runs the network.
   Its outputs are scores, not a modified image. For each grid location there
   are 80 class scores and 32 box-edge scores. Outputs are matched by shape
   because their ordering differs between graph engines.
4. Each grid has a stride: the spacing of its locations in model-input pixels.
   `minMaxLoc` finds the strongest class at each location. Only cat (class 15)
   with score at least 0.5 is retained.
5. Each of the four edges has eight distance scores. `edge_distance` subtracts
   the largest score before exponentiation for numerical stability, normalizes
   the weights (softmax), and computes their weighted mean over distances 0-7.
   Multiplying by stride converts that distance to model-input pixels. Subtract
   left/top distances and add right/bottom distances around the grid center to
   construct the candidate rectangle.
6. `floor` and `ceil` round the rectangle outward to integer coordinates.
   `blobRectToImageRect` undoes the resize and padding. Intersection with the
   original image rectangle clips coordinates to valid pixels. Empty boxes
   are discarded; the others and their scores are stored in `std::vector`s.
7. `NMSBoxes` removes overlapping duplicate predictions. IoU means intersection
   area divided by union area; here suppression uses a threshold of 0.6. The
   retained indices select which boxes and confidence scores to print.
8. `detect()` copies retained boxes and scores into its returned result. Errors
   throw exceptions to the caller. The standalone program's final `catch`
   prints the error and returns 1. Successful execution returns 0 even when no
   cats are found. Its argument-count check returns 2 on misuse.

`cv::Mat` uses reference counting: reshaping an output changes its view without
copying all its data. The `outputs` vector stays alive throughout decoding.
This executable loads the model on each invocation for a simple file-based
test. Later compositor integration should retain a loaded network across frames;
it should not start this executable separately for every frame.

## Sources

- Model, labels, normalization and decoding reference:
  https://github.com/opencv/opencv_zoo/tree/main/models/object_detection_nanodet
- Model license: Apache-2.0; a copy is in `MODEL-LICENSE`.
- Optional two-cat test image:
  https://huggingface.co/datasets/huggingface/documentation-images/resolve/main/coco_sample.png
- `model.sha256` records the SHA-256 from the upstream Git LFS model pointer.
- OpenCV preprocessing/mapping API:
  https://docs.opencv.org/4.x/d9/d3c/structcv_1_1dnn_1_1Image2BlobParams.html
