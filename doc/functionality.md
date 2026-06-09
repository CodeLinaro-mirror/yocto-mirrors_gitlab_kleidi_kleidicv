<!--
SPDX-FileCopyrightText: 2023 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

SPDX-License-Identifier: Apache-2.0
-->

# Supported types of implemented functions
Note: functions listed here are not necessarily exposed to adapter API layer.
See `doc/opencv.md` for details of the functionality available in OpenCV.

## Analysis

Analysis and statistics operations.

|                 | s8  | u8  | s16 | u16 | s32 | u32 | s64 | f32 |
|-----------------|-----|-----|-----|-----|-----|-----|-----|-----|
| Sum             |     |     |     |     |     |     |     |  x  |
| Minmax          |  x  |  x  |  x  |  x  |  x  |     |     |  x  |
| Minmax loc      |     |  x  |     |     |     |     |     |     |
| Count non-zeros |     |  x  |     |     |     |     |     |     |

### Optical flow

Optical-flow operations.

|                                  | u8 input |
|----------------------------------|----------|
| LK optical-flow pyramid builder  |    x     |
| LK optical flow from image pair  |    x     |
| LK optical flow from pyramids    |    x     |

Note: LK optical-flow APIs support `uint8_t` input images with 1 to 4 channels.

## Arithmetics

Arithmetic and element-wise operations.

### Numeric operations

Numeric and comparison operations.

|                              | s8  | u8  | s16 | u16 | s32 | u32 | s64 | u64 | f32 | f64 | u8->f16 |
|------------------------------|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|---------|
| Exp                          |     |     |     |     |     |     |     |     |  x  |     |         |
| Saturating Add               |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |     |     |         |
| Saturating Sub               |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |     |     |         |
| Saturating Absdiff           |  x  |  x  |  x  |  x  |  x  |     |     |     |     |     |         |
| Saturating Multiply          |  x  |  x  |  x  |  x  |  x  |     |     |     |     |     |         |
| Threshold binary             |     |  x  |     |     |     |     |     |     |     |     |         |
| SaturatingAddAbsWithThreshold|     |     |  x  |     |     |     |     |     |     |     |         |
| Scale                        |     |  x  |     |     |     |     |     |     |  x  |     |    x    |
| CompareEqual                 |     |  x  |     |     |     |     |     |     |     |     |         |
| CompareGreater               |     |  x  |     |     |     |     |     |     |     |     |         |
| InRange                      |     |  x  |     |     |     |     |     |     |  x  |     |         |

Note: Scale u8->f16 using SVE/SME is significantly faster when `alpha` and
`beta` are exact `float16_t` values. This can be forced by explicit casting:
`static_cast<float16_t>(0.3)`.

### Bitwise operations

Bitwise element-wise operations.

|                              | u8  |
|------------------------------|-----|
| Bitwise And                  |  x  |

## Conversions

Color, data type, and channel packing conversions.

### Color conversions

Color conversion operations.

|                                      | u8  |
|--------------------------------------|-----|
| Gray-RGB                             |  x  |
| Gray-RGBA                            |  x  |
| RGB-BGR                              |  x  |
| BGR-RGB                              |  x  |
| RGBA-BGRA                            |  x  |
| BGRA-RGBA                            |  x  |
| RGB-YUV422                           |  x  |
| BGR-YUV422                           |  x  |
| RGBA-YUV422                          |  x  |
| BGRA-YUV422                          |  x  |
| YUV420 (planar & semi-planar) - BGR  |  x  |
| YUV420 (planar & semi-planar) - BGRA |  x  |
| YUV420 (planar & semi-planar) - RGB  |  x  |
| YUV420 (planar & semi-planar) - RGBA |  x  |
| YUV422 - BGR                         |  x  |
| YUV422 - BGRA                        |  x  |
| YUV422 - RGB                         |  x  |
| YUV422 - RGBA                        |  x  |
| YUV - BGR                            |  x  |
| YUV - RGB                            |  x  |
| YUV - BGRA                           |  x  |
| YUV - RGBA                           |  x  |
| RGB - YUV                            |  x  |
| RGBA - YUV                           |  x  |
| BGR - YUV                            |  x  |
| BGRA - YUV                           |  x  |
| RGB - YUV420 (planar & semi-planar)  |  x  |
| RGBA - YUV420 (planar & semi-planar) |  x  |
| BGR - YUV420 (planar & semi-planar)  |  x  |
| BGRA - YUV420 (planar & semi-planar) |  x  |

### Data type conversions

Data type conversion operations.

|            | u8  | s8  | f32 |
|------------|-----|-----|-----|
| To float32 |  x  |  x  |     |
| To uint8   |     |     |  x  |
| To int8    |     |     |  x  |

### Channel packing and unpacking

Channel packing and unpacking operations.

|                               | 8-bit | 16-bit | 32-bit | 64-bit |
|-------------------------------|-------|--------|--------|--------|
| Merge                         |   x   |    x   |    x   |    x   |
| Split                         |   x   |    x   |    x   |    x   |

## Filters

Filtering operations.

|                                                   | s8  | u8  | s16 | u16 | s32 | u32 | f32 |
|---------------------------------------------------|-----|-----|-----|-----|-----|-----|-----|
| Sobel (3x3)                                       |     |  x  |     |     |     |     |     |
| Separable Filter 2D (5x5)                         |     |  x  |  x  |  x  |     |     |     |
| Gaussian Blur (3x3, 5x5, 7x7, 9x9, 15x15, 21x21)  |     |  x  |     |     |     |     |     |
| Gaussian Blur any kernel size, Replicated Borders |     |  x  |     |     |     |     |     |
| Median Blur (3x3, 5x5, 7x7)                       |  x  |  x  |  x  |  x  |  x  |  x  |  x  |
| Median Blur (generic imp, max size 255x255)       |     |  x  |     |     |     |     |     |

## Morphology

Morphological operations.

|                                                   | s8  | u8  | s16 | u16 | s32 | u32 | f32 |
|---------------------------------------------------|-----|-----|-----|-----|-----|-----|-----|
| Erode                                             |     |  x  |     |     |     |     |     |
| Dilate                                            |     |  x  |     |     |     |     |     |

## Resize

Resize operations.

### Linear interpolation

Resize operations using linear interpolation.

Float32 data:
* 1-channel 2x2, 4x4 and 8x8 upsize

Uint8 data:
| Channels                               | 1 | 2 | 3 | 4 |
|----------------------------------------|---|---|---|---|
| Downsize 1/3 to 1                      | x | x | x |   |
| Downsize 0.5x0.5                       | x | x | x | x |
| Upsize                                 | x | x | x |   |

Note: For uint8 downsize and upsize, table specifies horizontal scaling. Vertical scaling factor is independent, it can be anything, even vertical upsizing is supported when the horizontal dimension is downsized, and vice versa.

Note: For the linear upsize, horizontal scaling factor practically has no limit, but for
bigger factors the performance is worse.

## Transform

Geometric transform operations.

### Geometric transforms

Geometric transform operations.

|                               | 8-bit | 16-bit | 24-bit | 32-bit | 48-bit | 64-bit |
|-------------------------------|-------|--------|--------|--------|--------|--------|
| Transpose                     |   x   |    x   |    x   |    x   |    x   |    x   |
| Rotate (+-90 degrees)         |   x   |    x   |    x   |    x   |    x   |    x   |
| Add padding by copy (examples)|   x   |    x   |    x   |    x   |    x   |    x   |

Note: `Add padding by copy` is not limited by channel count in the same way as
the other operations in this table. It supports any positive pixel size, so the
listed 8/16/24/32/48/64-bit entries are examples rather than the full set of
supported channel packings.

### Remap

Remap operations.

|                                                   |  1ch u8 | 1ch u16 | 2ch u8 | 2ch u16 | 4ch u8 | 4ch u16 |
|---------------------------------------------------|---------|---------|--------|---------|--------|---------|
| Remap int16 coordinates                           |    x    |    x    |        |         |        |         |
| Remap int16+uint16 fixed-point coordinates        |    x    |    x    |        |         |   x    |    x    |
| Remap float32 coordinates - nearest interpolation |    x    |    x    |    x   |    x    |        |         |
| Remap float32 coordinates - linear interpolation  |    x    |    x    |    x   |    x    |        |         |

### WarpPerspective

Perspective warp operations.

|                      |  u8 |
|----------------------|-----|
| Nearest neighbour    |  x  |
| Linear interpolation |  x  |
