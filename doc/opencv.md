<!--
SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>

SPDX-License-Identifier: Apache-2.0
-->

# OpenCV Hardware Acceleration Layer

[OpenCV](https://github.com/opencv/opencv) can be built with KleidiCV
acceleration via KleidiCV's OpenCV Hardware Acceleration Layer (HAL).
For details of building OpenCV with KleidiCV see [the build documentation](build.md).

## Functionality in KleidiCV OpenCV HAL

### [`cv::add()`](https://docs.opencv.org/4.10.0/d2/de8/group__core__array.html#ga10ac1bfb180e2cfda1701d06c24fdbd6), [`cv::subtract()`](https://docs.opencv.org/4.10.0/d2/de8/group__core__array.html#gaa0f00d98b4b5edeaeb7b8333b2de353b), [`cv::absdiff()`](https://docs.opencv.org/4.10.0/d2/de8/group__core__array.html#ga6fef31bc8c4071cbc114a758a2b79c14)
Element-wise addition, subtraction and absolute difference.

Notes on parameters:
* `src1.depth() == src2.depth()` - `CV_8U`, `CV_8S`, `CV_16U` & `CV_16S` are supported.\
  `CV_32S` is not supported as KleidiCV does not currently provide the
  non-saturating implementation required by OpenCV for this type.

### [`cv::multiply()`](https://docs.opencv.org/4.10.0/d2/de8/group__core__array.html#ga979d898a58d7f61c53003e162e7ad89f)
Element-wise multiplication.

Notes on parameters:
* `src1.depth() == src2.depth()` - `CV_8U`, `CV_8S`, `CV_16U` & `CV_16S` are supported.\
  `CV_32S` is not supported as KleidiCV does not currently provide the
  non-saturating implementation required by OpenCV for this type.
* `scale` - only a value of 1.0 is supported.

### [`cv::bitwise_and()`](https://docs.opencv.org/4.10.0/d2/de8/group__core__array.html#ga60b4d04b251ba5eb1392c34425497e14)
Bitwise conjunction of two arrays.

### [`cv::cvtColor()`](https://docs.opencv.org/4.10.0/d8/d01/group__imgproc__color__conversions.html#ga397ae87e1288a81d2363b61574eb8cab)
Converts the color space of an image.

Notes on parameters:
* `code` - supported [`OpenCV color conversion codes`](https://docs.opencv.org/4.10.0/d8/d01/group__imgproc__color__conversions.html#ga4e0972be5de079fed4e3a10e24ef5ef0) are:

#### [`COLOR_GRAY2RGB`](https://docs.opencv.org/4.10.0/d8/d01/group__imgproc__color__conversions.html#gga4e0972be5de079fed4e3a10e24ef5ef0a164663625b9c473f69dd47cdd7a31acc),[`COLOR_GRAY2RGBA`](https://docs.opencv.org/4.10.0/d8/d01/group__imgproc__color__conversions.html#gga4e0972be5de079fed4e3a10e24ef5ef0aecc1b054bb042ee91478e591cf1f19b8)
Converts grayscale images to RGB or RGBA.

Notes on parameters:
* `src.depth()` - only supports `CV_8U` depth.
* `dstCn` - supports 3 for RGB, 4 for RGBA.

#### [`COLOR_RGB2BGR`](https://docs.opencv.org/4.10.0/d8/d01/group__imgproc__color__conversions.html#gga4e0972be5de079fed4e3a10e24ef5ef0a01a06e50cd3689f5e34e26daf3faaa39), [`COLOR_BGR2RGB`](https://docs.opencv.org/4.10.0/d8/d01/group__imgproc__color__conversions.html#gga4e0972be5de079fed4e3a10e24ef5ef0ad3db9ff253b87d02efe4887b2f5d77ee),[`COLOR_RGBA2BGRA`](https://docs.opencv.org/4.10.0/d8/d01/group__imgproc__color__conversions.html#gga4e0972be5de079fed4e3a10e24ef5ef0a488919a903d6434cacc6a47cf058d803), [`COLOR_BGRA2RGBA`](https://docs.opencv.org/4.10.0/d8/d01/group__imgproc__color__conversions.html#gga4e0972be5de079fed4e3a10e24ef5ef0a06d2c61b23b77b9a3075388146ba5180)
RGB to RGB/RGBA image conversion. All supported permutations listed in the table below.
|     | RGB | BGR | RGBA | BGRA |
|-----|-----|-----|------|------|
| RGB |     |  x  |      |      |
| BGR |  x  |     |      |      |
| RGBA|     |     |      |   x  |
| BGRA|     |     |   x  |      |

Notes on parameters:
* `src.depth()` - only supports `CV_8U` depth.
* `src.channels()` - supports 3 for RGB and 4 for RGBA.
* `dst.channels()` - supports 3 for RGB and 4 for RGBA.

#### [`COLOR_YUV2RGB_I420)`](https://docs.opencv.org/4.10.0/d8/d01/group__imgproc__color__conversions.html#gga4e0972be5de079fed4e3a10e24ef5ef0a35687717fabb536c1e1ec0857714aaf9),[`COLOR_YUV2BGR_I420`](https://docs.opencv.org/4.10.0/d8/d01/group__imgproc__color__conversions.html#gga4e0972be5de079fed4e3a10e24ef5ef0a305e5da3816c78b3d1ffa0498424e94f),[`COLOR_YUV2RGBA_I420`](https://docs.opencv.org/4.10.0/d8/d01/group__imgproc__color__conversions.html#gga4e0972be5de079fed4e3a10e24ef5ef0a18346327c937bca2aa2856914ff11507),[`COLOR_YUV2BGRA_I420`](https://docs.opencv.org/4.10.0/d8/d01/group__imgproc__color__conversions.html#gga4e0972be5de079fed4e3a10e24ef5ef0a0ffa81c19231ddd2e9cee8616a3a4673)
YUV420 to RGB/RGBA image conversion (semi-planar). Function accepts Y plane and UV planes separately.\
All supported permutations listed in the table below.
|   | RGB | BGR | RGBA | BGRA |
|---|-----|-----|------|------|
|YUV|  x  |  x  |   x  |  x   |

Notes on parameters:
* `dst.channels()` - supports 3 for RGB and 4 for RGBA.

#### [`COLOR_YUV2RGB)`](https://docs.opencv.org/4.10.0/d8/d01/group__imgproc__color__conversions.html#gga4e0972be5de079fed4e3a10e24ef5ef0ab09d8186a9e5aaac83acd157a1be43b0),[`COLOR_YUV2BGR`](https://docs.opencv.org/4.10.0/d8/d01/group__imgproc__color__conversions.html#gga4e0972be5de079fed4e3a10e24ef5ef0ab053f0cf23ae1b0bfee1964fd9a182c9)
YUV to RGB image conversion, 3 channels to 3 channels, no subsampling.\
All supported permutations listed in the table below.
|   | RGB | BGR |
|---|-----|-----|
|YUV|  x  |  x  |

#### [`COLOR_RGB2YUV)`](https://docs.opencv.org/4.10.0/d8/d01/group__imgproc__color__conversions.html#gga4e0972be5de079fed4e3a10e24ef5ef0adc0f8a1354c98d1701caad4b384e0d18),[`COLOR_BGR2YUV`](https://docs.opencv.org/4.10.0/d8/d01/group__imgproc__color__conversions.html#gga4e0972be5de079fed4e3a10e24ef5ef0a611d58d4a431fdbc294b4c79701f3d1a)
RGB/RGBA to YUV image conversion, 3 or 4 channels to 3 channels, no subsampling.\
All supported permutations listed in the table below.
|    | YUV |
|----|-----|
|RGB |  x  |
|RGBA|  x  |
|BGR |  x  |
|BGRA|  x  |

Notes on parameters:
* `src.depth()` - only supports `CV_8U` depth.
* `src.channels()` - supports 3 for RGB/BGR and 4 for RGBA/BGRA.

### [`cv::threshold()`](https://docs.opencv.org/4.10.0/d7/d1b/group__imgproc__misc.html#gae8a4a146d1ca78c626a53577199e9c57)
Threshold operation that would apply `maxValue` to any element above threshold, and set the rest to zero.

Notes on parameters:
* `src.depth()` - only supports `CV_8U` depth.
* `type` - currently only binary threshold operation is supported ([`cv::THRESH_BINARY`](https://docs.opencv.org/4.10.0/d7/d1b/group__imgproc__misc.html#ggaa9e58d2860d4afa658ef70a9b1115576a147222a96556ebc1d948b372bcd7ac59)).

### [`cv::sepFilter2D()`](https://docs.opencv.org/4.10.0/d4/d86/group__imgproc__filter.html#ga910e29ff7d7b105057d1625a4bf6318d)
Applies a separable linear filter to an image.\
Currently only the 5x5 kernel size is supported with `CV_8U`, `CV_16U` and `CV_32F` source, destination and kernel depths.\
In-place filtering not supported.

Notes on parameters:
* `src`, `dst`, `kernelX`, `kernelY` - the number of channels must match between source and destination. The types must match between source, destination and kernels. However, the kernels must have 1 channel.
* `anchor` - values other than `(-1, -1)` or the midpoints of the X and Y kernels respectively are not supported.
* `delta` - values other than `0.0` are not supported.
* `borderType` - pixel extrapolation method.
Supported [OpenCV border types](https://docs.opencv.org/4.10.0/d2/de8/group__core__array.html#ga209f2f4869e304c82d07739337eae7c5) are:
  + `cv::BORDER_REPLICATE`
  + `cv::BORDER_REFLECT`
  + `cv::BORDER_WRAP`
  + `cv::BORDER_REFLECT_101`

### [`cv::GaussianBlur()`](https://docs.opencv.org/4.10.0/d4/d86/group__imgproc__filter.html#gaabe8c836e97159a9193fb0b11ac52cf1)
Blurs an image using a Gaussian filter.\
The filter's standard deviation must be the same in horizontal and vertical directions (`sigmaX == sigmaY`).\
In-place filtering not supported.

Notes on parameters:
* `src.depth()` - only supports `CV_8U` depth.
* `src.width()`,`src.height()` - should be greater than or equal to the size of the kernel in the given direction.
* `ksize` - supported kernel sizes are 3x3, 5x5, 7x7 and 15x15.
* `borderType` - supported [OpenCV border types](https://docs.opencv.org/4.10.0/d2/de8/group__core__array.html#ga209f2f4869e304c82d07739337eae7c5) are:
  + `cv::BORDER_REPLICATE`
  + `cv::BORDER_REFLECT`
  + `cv::BORDER_WRAP`
  + `cv::BORDER_REFLECT_101`

### [`cv::dilate()`](https://docs.opencv.org/4.10.0/d4/d86/group__imgproc__filter.html#ga4ff0f3318642c4f469d0e11f242f3b6c)
Notes on parameters:
* `src.depth()`,`dst.depth()`,`kernel.depth()` - only support `CV_8U` depth.
* `borderType` - only supports [`BORDER_CONSTANT`](https://docs.opencv.org/4.10.0/d2/de8/group__core__array.html#gga209f2f4869e304c82d07739337eae7c5aed2e4346047e265c8c5a6d0276dcd838).

### [`cv::erode()`](https://docs.opencv.org/4.10.0/d4/d86/group__imgproc__filter.html#gaeb1e0c1033e3f6b891a25d0511362aeb)
Notes on parameters:
* `src.depth()`,`dst.depth()`,`kernel.depth()` - only support `CV_8U` depth.
* `borderType` - only supports [`BORDER_CONSTANT`](https://docs.opencv.org/4.10.0/d2/de8/group__core__array.html#gga209f2f4869e304c82d07739337eae7c5aed2e4346047e265c8c5a6d0276dcd838).

### [`cv::resize()`](https://docs.opencv.org/4.10.0/da/d54/group__imgproc__transform.html#ga47a974309e9102f5f08231edc7e7529d)
In-place operation not supported.

Notes on parameters:
* `src.type()` - only supports `CV_8UC1` and `CV_32FC1`. Relative sizes can be:
  + 0.5x0.5 (`CV_8UC1` only)
  + 2x2, 4x4 and 8x8 (`CV_32FC1` only).
* `dst.width()`,`dst.height()` - must be the same multiple of both `src.width()` and `src.height()` respectively, and that multiple must be either 0.5, 2, 4 or 8.
* `fx`,`fy` - must be 0 or `dst.width() / src.width()`.
* `interpolation` - supported [`InterpolationFlags`](https://docs.opencv.org/4.10.0/da/d54/group__imgproc__transform.html#ga5bb5a1fea74ea38e1a5445ca803ff121) are:
  + `INTER_LINEAR`
  + `INTER_AREA` with scaling factor 0.5x0.5 only.

|src.type()|interpolation |   fx x fy   |
|----------|--------------|-------------|
| CV_8UC1  | INTER_AREA   |   0.5x0.5   |
|          | INTER_LINEAR |   0.5x0.5   |
| CV_32FC1 | INTER_AREA   |   0.5x0.5   |
|          | INTER_LINEAR |2x2, 4x4, 8x8|

### [`cv::Sobel()`](https://docs.opencv.org/4.10.0/d4/d86/group__imgproc__filter.html#gacea54f142e81b6758cb6f375ce782c8d)
Applies Sobel gradient filter to a given image.\
In-place filtering not supported.

Notes on parameters:
* `src.depth()` - only supports `CV_8U` depth.
* `src.width()`,`src.height()` - image width and height should be `>=3`.
* `ddepth` - only supports `CV_16S` depth.
* `dx`,`dy` - either vertical `{dx,dy} == {0,1}` or horizontal `{dx,dy} == {1,0}` operation is supported.
* `ksize` - only supports 3x3 kernel size.
* `scale` - scaling is not supported (`1.0`).
* `delta` - delta value is not supported.
* `borderType` - only supports [`cv::BORDER_REPLICATE`](https://docs.opencv.org/4.10.0/d2/de8/group__core__array.html#gga209f2f4869e304c82d07739337eae7c5aa1de4cff95e3377d6d0cbe7569bd4e9f).

### [`cv::transpose()`](https://docs.opencv.org/4.10.0/d2/de8/group__core__array.html#ga46630ed6c0ea6254a35f447289bd7404)
Transposes a matrix.

Notes on parameters:
* In-place `transpose` is only supported for square matrixes. (`src.width() == src.height()`)

### [`cv::minMaxIdx()`](https://docs.opencv.org/4.10.0/d2/de8/group__core__array.html#ga7622c466c628a75d9ed008b42250a73f)
Finds the minimum and maximum element values and their positions.

Notes on parameters:
* `src.depth()` - supported element sizes (without specifying index) are:
  + `CV_8S`
  + `CV_8U`
  + `CV_16S`
  + `CV_16U`
  + `CV_32S`
  + `CV_32F`
* `minIdx`,`maxIdx` - are only supported for `src.depth() == CV_8U`.

### [`cv::Mat::convertTo()`](https://docs.opencv.org/4.10.0/d3/d63/classcv_1_1Mat.html#adf88c60c5b4980e05bb556080916978b)
This function will scale given input using `alpha` and `beta` if they are significant enough, and if `src.depth()` equals `dst.depth()`.\
Supported depths:
  + `CV_8U`
  + `CV_32F`

Additionally, it is able to convert between data types as follows:

| src.depth() | dst.depth() |
|-------------|-------------|
|    CV_32F   |    CV_8S    |
|    CV_32F   |    CV_8U    |
|    CV_8S    |    CV_32F   |
|    CV_8U    |    CV_32F   |

### [`cv::exp()`](https://docs.opencv.org/4.10.0/d2/de8/group__core__array.html#ga3e10108e2162c338f1b848af619f39e5)
Exponential function. Currently only `CV_32F` type is supported.

### [`cv::compare()`](https://docs.opencv.org/4.10.0/d2/de8/group__core__array.html#ga303cfb72acf8cbb36d884650c09a3a97)
Performs element-wise comparison of two arrays.\
Currently comparing an array and scalar value is not supported (the HAL only allows for matching sizes of the sources).

Notes on parameters:
* `src1.depth()` == `src2.depth()` - only support `CV_8U`.
* `cmpop` - flag specifying correspondence between the arrays.
Supported [OpenCV cmp types](https://docs.opencv.org/4.10.0/d2/de8/group__core__array.html#ga0cc47ff833d40b58ecbe1d609a53d784) are:
  + `cv::CMP_GT`

### [`cv::inRange()`](https://docs.opencv.org/4.10.0/d2/de8/group__core__array.html#ga48af0ab51e36436c5d04340e036ce981)
Checks whether array elements fall between user set lower and upper bounds.\
Currently only scalar bounds are supported.

Notes on parameters:
* `src.depth()` - only supports `CV_8U` and `CV_32F` depths and 1 channel.
* `src`, `lowerb` and `upperb` need to have the same type.
