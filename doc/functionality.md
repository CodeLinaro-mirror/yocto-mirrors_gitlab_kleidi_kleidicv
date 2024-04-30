<!--
SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>

SPDX-License-Identifier: Apache-2.0
-->

# Supported types of implemented functions
Note: functions listed here are not necessarily exposed to adapter API layer.
See `doc/opencv.md` for details of the functionality available in OpenCV.

## Basic arithmetic operations
|                              | s8  | u8  | s16 | u16 | s32 | u32 | s64 | u64 |
|------------------------------|-----|-----|-----|-----|-----|-----|-----|-----|
| Saturating Add               |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |
| Saturating Sub               |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |
| Saturating Absdiff           |  x  |  x  |  x  |  x  |  x  |     |     |     |
| Saturating Multiply          |  x  |  x  |  x  |  x  |  x  |     |     |     |
| Threshold binary             |     |  x  |     |     |     |     |     |     |
| SaturatingAddAbsWithThreshold|     |     |  x  |     |     |     |     |     |
| Scale                        |     |  x  |     |     |     |     |     |     |

## Colour conversions
|           | u8  |
|-----------|-----|
| Gray-RGB  |  x  |
| Gray-RGBA |  x  |
| RGB-BGR   |  x  |
| RGB-BGRA  |  x  |
| RGB-RGB   |  x  |
| RGB-RGBA  |  x  |
| RGBA-BGR  |  x  |
| RGBA-BGRA |  x  |
| RGBA-RGB  |  x  |
| RGBA-RGBA |  x  |
| YUV-BGR   |  x  |
| YUV-BGRA  |  x  |
| YUV-RGB   |  x  |
| YUV-RGBA  |  x  |
| RGB-YUV   |  x  |
| RGBA-YUV  |  x  |
| BGR-YUV   |  x  |
| BGRA-YUV  |  x  |

## Matrix operations
|                 | s8  | u8  | s16 | u16 | s32 | u32 | s64 |
|-----------------|-----|-----|-----|-----|-----|-----|-----|
| Merge           |  x  |  x  |  x  |  x  |  x  |  x  |  x  |
| Split           |  x  |  x  |  x  |  x  |  x  |  x  |  x  |
| Transpose       |  x  |  x  |  x  |  x  |  x  |  x  |  x  |
| Minmax          |  x  |  x  |  x  |  x  |  x  |  x  |     |
| Minmax loc      |     |  x  |     |     |     |     |     |
| Count non-zeros |     |  x  |     |     |     |     |     |

## Image filters
|                       | u8  |
|-----------------------|-----|
| Erode                 |  x  |
| Dilate                |  x  |
| Sobel                 |  x  |
| Canny (experimental)  |  x  |
| Gaussian Blur         |  x  |

## Resize with linear interpolation
|             | u8  | f32 |
|-------------|-----|-----|
| 2x2         |  x  |  x  |
| 4x4         |  x  |  x  |
