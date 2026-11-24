# Linux Display Filter

An experimental Wayland compositor that detects cats in rendered desktop images
and covers them with grey boxes before sending the frame to the display.

The project explores **computer-vision-based filtering at the compositor level**:
render application windows, inspect the composed image with an object-detection
model, and modify selected regions before presenting the result. The current
prototype uses cats as a simple test case. More general object filtering and
blur are future work; the implemented effect is an opaque grey rectangle.

This repository is a fork of [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots),
based on **wlroots 0.20.2**. The project-specific implementation is in
[`tinywl/`](tinywl/), built on the upstream TinyWL example compositor. The rest
of the repository retains the wlroots library and its supporting code.

## What works today

- Automatic cat detection on each newly rendered output frame, with no trigger
  keybinding.
- Synchronous processing in the compositor's event-loop thread.
- Native C++ inference using OpenCV's CPU DNN backend and a small NanoDet ONNX
  model from OpenCV Zoo. One OpenCV thread is requested; no Python runtime is
  involved.
- An additional GPU rendering pass that fills detected bounding boxes with
  solid grey. These masks are drawn into the output buffer, outside the scene
  graph.
- A standalone image-file detector for testing the model separately from the
  compositor.

## How a frame is processed

```text
Application windows
        |
        v
wlroots renders the scene into an output buffer
        |
        v
Read pixels back to CPU memory and convert them to BGR
        |
        v
OpenCV runs NanoDet and returns cat bounding boxes
        |
        v
An additional GLES2 pass draws grey boxes into the output buffer
        |
        v
Wait for GPU drawing to finish, then commit the output for display
```

The model is loaded once at startup and reused. Each output has its own box
storage, and outputs are processed sequentially through the shared detector.
An unchanged scene does not repeatedly run inference just to match the monitor's
refresh rate.

The prototype repaints the whole scene before detection so that old masks do not
appear in the next model input. It also marks the whole output as changed when
committing. Direct scanout is disabled while building the scene's output buffer,
ensuring there is a compositor-rendered image available for processing.

## Build and try it

The following Makefile workflow builds the modified TinyWL against an installed
wlroots library. It has been built with **wlroots 0.20.2** and **OpenCV 5.0.0** on
Arch/Omarchy. The build expects the `wlroots-0.20` and `opencv5` pkg-config names,
a C compiler, and a C++17 compiler. The live masking path requires the **GLES2**
renderer.

### 1. Install dependencies and build

On Arch/Omarchy:

```sh
sudo pacman -S --needed base-devel pkgconf wlroots0.20 opencv curl foot

git clone --branch display-filter https://github.com/saheemReshi/linux-display-filter.git
cd linux-display-filter/tinywl

pkg-config --modversion wlroots-0.20 opencv5
make
```

`foot` is the terminal used in the launch examples below. The Makefile compiles
TinyWL as C and links it with the C++ detector library and OpenCV.

### 2. Download the model

From `tinywl/`:

```sh
(
    cd cat-detector
    curl --fail --location --output nanodet.onnx \
        https://media.githubusercontent.com/media/opencv/opencv_zoo/main/models/object_detection_nanodet/object_detection_nanodet_2022nov.onnx
    sha256sum --check model.sha256
)
```

This downloads OpenCV Zoo's NanoDet-m-plus-1.5x_416 model, approximately 3.8 MB.
The weights are downloaded separately; the checked-in checksum verifies the file.
The detector selects the model's COCO cat class, with a confidence threshold of
0.5, and removes overlapping duplicate predictions.

### 3. Run inside your existing Wayland desktop

From `tinywl/`:

```sh
WLR_BACKENDS=wayland WLR_RENDERER=gles2 ./tinywl -s foot
```

This opens a nested compositor window with a terminal inside it. Launch a
Wayland image viewer from that terminal and open a cat photo. Detected cats
should be covered automatically. Move or close the image to check that masks
move or disappear with it. Applications must run inside TinyWL to appear in its
rendered scene.

- **Alt+F1** cycles between windows.
- **Alt+Escape** exits TinyWL.

For a direct-display test, run `WLR_RENDERER=gles2 ./tinywl -s foot` from a local
virtual terminal with a working DRM/libinput/session setup. The nested Wayland
backend above runs inside your existing desktop.

TinyWL looks for `cat-detector/nanodet.onnx` relative to its working directory.
Set `TINYWL_CAT_MODEL=/absolute/path/to/model.onnx` to use another model path.

### Test the detector without a compositor

From `tinywl/`:

```sh
make -C cat-detector
./cat-detector/detect-cats cat-detector/nanodet.onnx /path/to/cat.jpg
```

This prints bounding boxes and the model-forward time. That timing excludes
rendering, readback, preprocessing, box decoding, and mask drawing, so it is not
an end-to-end frame-time measurement.

## Where to read the code

| Location | Purpose |
| --- | --- |
| [`tinywl/tinywl.c`](tinywl/tinywl.c) | Compositor integration: `output_frame()`, pixel readback, synchronous detection, grey-mask rendering, and output commit. |
| [`tinywl/cat-detector/cat_detector.cpp`](tinywl/cat-detector/cat_detector.cpp) | Model loading, image preprocessing, inference, box decoding, and duplicate removal. |
| [`tinywl/cat-detector/cat_detector_c.h`](tinywl/cat-detector/cat_detector_c.h) and [`.cpp`](tinywl/cat-detector/cat_detector_c.cpp) | C interface connecting TinyWL to the C++ detector. |
| [`tinywl/cat-detector/readback_bgr.c`](tinywl/cat-detector/readback_bgr.c) | Converts supported four-byte output pixel formats into three-byte BGR input. |
| [`tinywl/cat-detector/main.cpp`](tinywl/cat-detector/main.cpp) | Standalone image-file test program. |
| [`tinywl/cat-detector/README.md`](tinywl/cat-detector/README.md) | Detailed detector API, memory ownership, model, and integration notes. |

## Current limitations and direction

This is a learning and experimentation prototype. CPU inference, full-frame
readback, full-scene repainting, and blocking GPU synchronization can reduce
frame rate and delay input handling, especially at higher resolutions or with
multiple outputs. Detection can miss cats or produce false positives.

The live path currently supports the GLES2 renderer and common little-endian
8-bit, four-byte RGB output formats, with dimensions up to 8192 per axis. A model
load failure stops startup; a readback, inference, or mask-rendering failure
skips committing that frame.

Potential next steps include configurable object classes, blur instead of grey
fills, allocation/readback optimizations, and asynchronous processing. These are
future directions, not implemented features.

## Upstream and licensing

wlroots provides the Wayland protocols, rendering abstractions, and display/input
backends underlying this experiment. This fork is an independent project, based
on the work of the wlroots and TinyWL contributors.

See the repository's [MIT license](LICENSE), [TinyWL license](tinywl/LICENSE),
and the detector's [model license](tinywl/cat-detector/MODEL-LICENSE). The model
and reference implementation come from
[OpenCV Zoo's NanoDet project](https://github.com/opencv/opencv_zoo/tree/main/models/object_detection_nanodet).
