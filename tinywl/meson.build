add_languages('cpp', required: true)
cpp = meson.get_compiler('cpp')
opencv = dependency('opencv5')
cat_deps = [
	cpp.find_library('opencv_dnn'),
	cpp.find_library('opencv_imgproc'),
	cpp.find_library('opencv_core'),
]
cat_library = static_library(
	'tinywl-cat-detector',
	'cat-detector/cat_detector.cpp',
	'cat-detector/cat_detector_c.cpp',
	'cat-detector/readback_bgr.c',
	dependencies: [
		opencv.partial_dependency(compile_args: true, includes: true),
		dependency('libdrm'),
	],
	override_options: ['cpp_std=c++17'],
)

executable(
	'tinywl',
	'tinywl.c',
	dependencies: [wlroots, dependency('glesv2'), dependency('pixman-1')] + cat_deps,
	link_with: cat_library,
	link_language: 'cpp',
)
