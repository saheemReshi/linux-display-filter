# Standalone cat detector

This stage reads an image and prints cat bounding boxes. It does not launch,
modify, or communicate with TinyWL. It uses C++ and OpenCV's CPU DNN backend,
with one OpenCV thread requested. No Python runtime is involved.

## Dependency and build

On Arch/Omarchy, install the C++ development library if needed:

```sh
sudo pacman -S --needed opencv
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

You can also pass `../capture.ppm`. The input is letterboxed to 416 by 416,
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
no sockets, worker threads, GPU rendering, or output commits.

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

This is a C++ interface. A C-compatible wrapper for TinyWL is a later stage.

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
