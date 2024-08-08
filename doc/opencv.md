<!--
SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>

SPDX-License-Identifier: Apache-2.0
-->

# OpenCV Hardware Acceleration Layer

[OpenCV](https://github.com/opencv/opencv) can be built with KleidiCV
acceleration via KleidiCV's OpenCV Hardware Acceleration Layer (HAL).
For details of building OpenCV with KleidiCV see [the build documentation](build.md).

## Performance

For single-threaded use cases, enabling the KleidiCV OpenCV HAL is likely to
provide a performance boost for the functions that it implements.
Multithreading is planned for KleidiCV but at present it is single-threaded
only. Therefore it is recommended to check the performance yourself in order
to decide whether to enable KleidiCV in a multicore environment.

## Functionality in KleidiCV OpenCV HAL

### `add`, `subtract`, `absdiff`
Element-wise addition, subtraction and absolute difference.

Notes on parameters:
* `depth` - `CV_8U`, `CV_8S`, `CV_16U` & `CV_16S` are supported.
  `CV_32S` is not supported as KleidiCV does not currently provide the
  non-saturating implementation required by OpenCV for this type.

### `multiply`
Element-wise multiplication.

Notes on parameters:
* `depth` - `CV_8U`, `CV_8S`, `CV_16U` & `CV_16S` are supported.
  `CV_32S` is not supported as KleidiCV does not currently provide the
  non-saturating implementation required by OpenCV for this type.
* `scale` - only a value of 1.0 is supported.

### `bitwise_and`
Bitwise conjunction of two arrays.

### `gray_to_bgr`
Converts grayscale images to RGB or RGBA.

Notes on parameters:
* `depth` - only supports `CV_8U` depth.
* `dcn` - Destination Channel Number. Supports 3 for RGB, 4 for RGBA

### `bgr_to_bgr`
RGB to RGB/RGBA image conversion. All supported permutations listed in the table below.
|     | RGB | BGR | RGBA | BGRA |
|-----|-----|-----|------|------|
| RGB |     |  x  |      |      |
| BGR |  x  |     |      |      |
| RGBA|     |     |      |   x  |
| BGRA|     |     |   x  |      |

Notes on parameters:
* `depth` - only supports `CV_8U` depth.
* `scn` - Source Channel Number. Supports 3 for RGB and 4 for RGBA
* `dcn` - Destination Channel Number. Supports 3 for RGB and 4 for RGBA
* `swapBlue` - toggles destination channel order.

### `yuv_to_bgr_sp_ex`
YUV420 to RGB/RGBA image conversion (semi-planar). Function accepts Y plane and UV planes separately.\
All supported permutations listed in the table below.
|   | RGB | BGR | RGBA | BGRA |
|---|-----|-----|------|------|
|YUV|  x  |  x  |   x  |  x   |

Notes on parameters:
* `dcn` - Destination Channel Number. Supports 3 for RGB and 4 for RGBA
* `swapBlue` - toggles destination channels order.
* `uIdx` - sets particular YUV format. NV12 (`uIdx = 0`) and NV21 (`uIdx != 0`) are supported.

### `yuv_to_bgr_sp`
Wrapper for [`yuv_to_bgr_sp_ex`](#yuv_to_bgr_sp_ex) that accepts YUV as a single buffer.

### `yuv_to_bgr`
YUV to RGB image conversion, 3 channels to 3 channels, no subsampling.
All supported permutations listed in the table below.
|   | RGB | BGR |
|---|-----|-----|
|YUV|  x  |  x  |

### `bgr_to_yuv`
RGB/RGBA to YUV image conversion, 3 or 4 channels to 3 channels, no subsampling.
All supported permutations listed in the table below.
|    | YUV |
|----|-----|
|RGB |  x  |
|RGBA|  x  |
|BGR |  x  |
|BGRA|  x  |

Notes on parameters:
* `depth` - only supports `CV_8U` depth.
* `scn` - Source Channel Number. Supports 3 for RGB/BGR and 4 for RGBA/BGRA
* `swapBlue` - toggles source channels order.
* `isCbCr` - sets particular YUV format. Only `false` (YUV) is supported.

### `threshold`
Binary threshold operation that would apply `maxValue` to any element above threshold, and set the rest to zero.

Notes on parameters:
* `depth` - only supports `CV_8U` depth.
* `thresh` - threshold value elements will be compared against.
* `maxValue` - value that elements above `thresh` will be set to.
* `thresholdType` - currently only binary threshold operation is supported ([cv::THRESH_BINARY](https://docs.opencv.org/5.x/d7/d1b/group__imgproc__misc.html#gaa9e58d2860d4afa658ef70a9b1115576)).

### `gaussian_blur`
Blurs an image using a Gaussian filter.\
Currently does not support non-zero margins. Kernel shape is restricted to square (`kernelWidth == kernelHeight`). The filter's
standard deviation must be the same in horizontal and vertical directions (`sigma_x == sigma_y`).

Notes on parameters:
* `depth` - only supports `CV_8U` depth.
* `width`,`height` - Image width and height should be greater than or equal to the size of the kernel in the given direction.
* `ksize_width == ksize_height` - kernel size. Only 3x3, 5x5, 7x7 and 15x15 kernels are supported.
* `border_type` - pixel extrapolation method.
Supported [OpenCV border types](https://docs.opencv.org/5.x/d2/de8/group__core__array.html#ga209f2f4869e304c82d07739337eae7c5) are:
  + `cv::BORDER_REPLICATE`
  + `cv::BORDER_REFLECT`
  + `cv::BORDER_WRAP`
  + `cv::BORDER_REFLECT_101`

### `morphology_init`
Initialize parameters for [`morphology_operation`](#morphology_operation) and populate `context`.

Notes on parameters:
* `operation` - erode and dilate operations are supported. See [OpenCV definitions](https://docs.opencv.org/5.x/d4/d86/group__imgproc__filter.html#ga7be549266bad7b2e6a04db49827f9f32).
* `src_type/dst_type/kernel_type` - type of source, destination and kernel. Only `CV_8U` depth is supported.
* `border_type` - only constant border type is supported.
* `allow_submatrix` - not supported.
* `allow_in_place` - not supported.

### `morphology_operation`
Perform morphology operation set up by [`morphology_init`](#morphology_init) on a source buffer and put result in destination buffer.

Notes on parameters:\
Only parameters set by `morphology_init` will be used in operation. Parameters taken into account by the operation:
* `context` - initialized using `morphology_init`.
* `src_data`, `src_step`
* `dst_data`, `dst_step`
* `height`, `width`

The rest of parameters for `morphology_operation` function are currently not supported.

### `morphology_free`
Release context set up by [`morphology_init`](#morphology_init).

### `resize`
Notes on parameters:
* In-place operation not supported.
* `src_type` - only supports `CV_8UC1` or `CV_32FC1`, relative sizes can be 0.5x0.5 (`CV_8UC1` only), 2x2, 4x4 and 8x8 (`CV_32FC1` only).
* `dst_width`,`dst_height` - must both be the same multiple of `src_width` and `src_height` respectively, and that multiple must be either 0.5, 2, 4 or 8.
* `inv_scale_x`,`inv_scale_y` - must be 0 or `dst_width / src_width`.
* `interpolation` - Must be `INTER_LINEAR` or `INTER_AREA` (0.5 by 0.5 only).

### `sobel`
Applies Sobel gradient filter to a given image.

Notes on parameters:
* In-place operation not supported.
* `src_depth` - only supports `CV_8U` depth.
* `dst_depth` - only supports `CV_16U` depth.
* `width`,`height` - image width and height should be `>=3`.
* `ksize` - only kernel 3x3 is supported.
* `scale` - scaling is not supported (`1.0`).
* `delta` - delta value is not supported.
* `border_type` - [`cv::BORDER_REPLICATE`](https://docs.opencv.org/5.x/d2/de8/group__core__array.html#ga209f2f4869e304c82d07739337eae7c5) is supported.
* margins are not supported.
* `dx`,`dy` - either vertical `{dx,dy} == {0,1}` or horizontal `{dx,dy == 1,0` operation is supported.

### `transpose`
Transposes a matrix.

Notes on parameters:
* In-place transpose is only supported for square matrixes. (`src_width == src_height`)
* `element_size` - supported source/destination sizes
  + `uint8_t`
  + `uint16_t`

### `minMaxIdx`
Finds the minimum and maximum element values and their positions.

Notes on parameters:
* `minIdx`,`maxIdx` - only supported for `depth == CV_8U`
* `depth` - supported element size (without specifying index):
  + `CV_8S`
  + `CV_8U`
  + `CV_16S`
  + `CV_16U`
  + `CV_32S`
  + `CV_32F`

### `convertTo`
This function will scale given input using `scale` and `shift` if they are significant enough, and if `src_depth` equals `dst_depth`. Supported depths:
  + `CV_8U`
  + `CV_32F`

Additionally, it is able to convert between data types as follows:

| src_depth | dst_depth |
|-----------|-----------|
|   CV_32F  |   CV_8S   |
|   CV_32F  |   CV_8U   |
|   CV_8S   |   CV_32F  |
|   CV_8U   |   CV_32F  |

### `exp`
Exponential function. Currently only `CV_32F` type is supported.

### `compare`
Performs element-wise comparison of two arrays.
Currently comparing an array and scalar value is not supported (the HAL only allows for matching sizes of the sources).

Notes on parameters:
* `src1_data`, `src2_data`, `dst_data` - only support `CV_8U` depth.
* `operation` - flag specifying correspondence between the arrays.
Supported [OpenCV cmp types](https://docs.opencv.org/5.x/d2/de8/group__core__array.html#ga0cc47ff833d40b58ecbe1d609a53d784) are:
  + `cv::CMP_GT`
