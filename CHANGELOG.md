<!--
SPDX-FileCopyrightText: 2024 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>

SPDX-License-Identifier: Apache-2.0
-->

# Changelog

This file documents significant changes between KleidiCV releases.

KleidiCV uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

This changelog aims to follow the guiding principles of
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## 0.5.0 - 2025-06-10

### Added
- Median Blur for 5x5 and 7x7 kernels.
- Gaussian Blur for 21x21 kernels.
- Support for GCC 8.4.
- C example.

### Changed
- Multithreaded dispatching in the OpenCV HAL of convertScale.

### Fixed
- Inplace operation of kleidicv_scale_f32.

## 0.4.0 - 2025-03-25

### Added
- Implementation of Rotate 90 degrees clockwise.
- Remap implementations for u8 and u16 images, Replicated and Constant borders:
  - Integer coordinates with Nearest Neighbour method
    - 1 channel only
  - Fixed-point coordinates with Linear interpolation
    - 1 and 4 channels
  - Floating-point coordinates with Nearest Neighbour and Linear interpolation
    - 1 and 2 channels
- WarpPerspective implementation for 1-channel u8 input:
  - Nearest Neighbour and Linear interpolation methods
  - Replicated and Constant borders
- Support for OpenCV 4.11.

### Changed
- Increased precision of sum for 32 bit floats and expose it to OpenCV HAL.

### Fixed
- Handling of cv::erode and cv::dilate non-default constant borders.

### Removed
- Support for OpenCV 4.10.

## 0.3.0 - 2024-12-12

### Added
- Implementation of cv::pyrDown in the OpenCV HAL.
- Implementation of cv::buildOpticalFlowPyramid in the OpenCV HAL.
- Sum implementation for 1-channel f32 input (not exposed to OpenCV).

### Changed
- Build options `KLEIDICV_ENABLE_SVE2` and `KLEIDICV_ENABLE_SME2` take effect directly.
  Previously the build scripts had additional checks that attempted to identify whether the compiler supported SVE2/SME2 - these checks have been removed.
- The default setting for `KLEIDICV_ENABLE_SVE2` is on for some popular compilers known to support SVE2, otherwise off.
- `KLEIDICV_ENABLE_SME2` defaults to off. This is because the ACLE SME specification has not yet been finalized.
- In the OpenCV HAL, cvtColor for gray-RGBA & BGRA-RGBA are multithreaded.
- Improved performance of 8-bit int to 32-bit float conversion.
- Renamed functions:
  - kleidicv_float_conversion_f32_s8 to kleidicv_f32_to_s8
  - kleidicv_float_conversion_f32_u8 to kleidicv_f32_to_u8
  - kleidicv_float_conversion_s8_f32 to kleidicv_s8_to_f32
  - kleidicv_float_conversion_u8_f32 to kleidicv_u8_to_f32

## 0.2.0 - 2024-09-30

### Added
- Exponential function for float.
- Bitwise and.
- Gaussian Blur for 7x7 kernels.
- Gaussian Blur for 15x15 kernels.
- Separable Filter 2D for 5x5 kernels (not exposed to OpenCV).
- Enable specifying standard deviation for Gaussian blur.
- Scale function for float.
- Add, subtract, multiply & absdiff enabled in OpenCV HAL.
- MinMax enabled in OpenCV HAL, float version added.
- Resize 4x4 for float.
- Resize 8x8 for float.
- Resize 0.5x0.5 for uint8_t.
- Conversion from float to (u)int8_t and vice versa.
- KLEIDICV_LIMIT_SME2_TO_SELECTED_ALGORITHMS configuration option.
- Conversion from non-subsampled, interleaved YUV to RGB/BGR.
- InRange single channel, scalar bounds for uint8_t and float.

### Changed
- Filter context creation API specification.
- Gaussian Blur API specification.
- In the OpenCV HAL, the following operations are multithreaded:
  * cvtColor
  * convertTo
  * exp
  * minMaxIdx
  * GaussianBlur
  * Sobel
  * resize

### Removed
- Support for OpenCV 4.9.
- threshold from OpenCV HAL.

## 0.1.0 - 2024-05-21

Initial release.
