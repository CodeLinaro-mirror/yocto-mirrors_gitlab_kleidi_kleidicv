<!--
SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>

SPDX-License-Identifier: Apache-2.0
-->

# Changelog

This file documents significant changes between KleidiCV releases.

KleidiCV uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

This changelog aims to follow the guiding principles of
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## 0.2.0 - not yet released

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

### Fixed

### Changed
- Filter context creation API specification.
- Gaussian Blur API specification.
- In the OpenCV HAL, the following operations are multithreaded:
  * cvtColor
  * threshold
  * convertTo
  * exp
  * compare
  * minMaxIdx
  * GaussianBlur
  * Sobel
  * sepFilter2D
  * resize
- Improved performance of Compare Equal and Greater SC API.
  (Only Compare Greater is exposed to OpenCV.)

### Removed
- Support for OpenCV 4.9.

## 0.1.0 - 2024-05-21

Initial release.
