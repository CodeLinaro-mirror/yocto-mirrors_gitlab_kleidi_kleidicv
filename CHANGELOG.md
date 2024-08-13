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
  * add
  * sub
  * mul
  * absdiff
  * bitwise_and
  * minMaxIdx
  * GaussianBlur
  * Sobel
  * sepFilter2D
- Improved performance of Compare Equal and Greater SC API.

### Removed


## 0.1.0 - 2024-05-21

Initial release.
