<!--
SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>

SPDX-License-Identifier: Apache-2.0
-->

# Supported types of implemented functions
Note: functions listed here are not necessarily exposed to adapter API layer.
See `doc/opencv.md` for details of the functionality available in OpenCV.

## Arithmetic operations
|                              | s8  | u8  | s16 | u16 | s32 | u32 | s64 | u64 | f32 | f64 |
|------------------------------|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|
| Exp                          |     |     |     |     |     |     |     |     |  x  |     |
| Saturating Add               |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |     |     |
| Saturating Sub               |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |     |     |
| Saturating Absdiff           |  x  |  x  |  x  |  x  |  x  |     |     |     |     |     |
| Saturating Multiply          |  x  |  x  |  x  |  x  |  x  |     |     |     |     |     |
| Threshold binary             |     |  x  |     |     |     |     |     |     |     |     |
| SaturatingAddAbsWithThreshold|     |     |  x  |     |     |     |     |     |     |     |
| Scale                        |     |  x  |     |     |     |     |     |     |  x  |     |
| CompareEqual                 |     |  x  |     |     |     |     |     |     |     |     |
| CompareGreater               |     |  x  |     |     |     |     |     |     |     |     |

# Logical operations
|                              | u8  |
|------------------------------|-----|
| Bitwise And                  |  x  |

## Color conversions
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

## Data type conversions
|            | u8  | s8  | f32 |
|------------|-----|-----|-----|
| To float32 |  x  |  x  |     |
| To uint8   |     |     |  x  |
| To int8    |     |     |  x  |

## Aggregate operations
|                 | s8  | u8  | s16 | u16 | s32 | u32 | s64 | f32 |
|-----------------|-----|-----|-----|-----|-----|-----|-----|-----|
| Minmax          |  x  |  x  |  x  |  x  |  x  |     |     |  x  |
| Minmax loc      |     |  x  |     |     |     |     |     |     |
| Count non-zeros |     |  x  |     |     |     |     |     |     |

## Matrix transformation functions
|                 | 8-bit | 16-bit | 32-bit | 64-bit |
|-----------------|-------|--------|--------|--------|
| Merge           |   x   |    x   |    x   |    x   |
| Split           |   x   |    x   |    x   |    x   |
| Transpose       |   x   |    x   |    x   |    x   |

## Image filters
|                                      | u8  |
|--------------------------------------|-----|
| Erode                                |  x  |
| Dilate                               |  x  |
| Sobel (3x3)                          |  x  |
| Gaussian Blur (3x3, 5x5, 7x7, 15x15) |  x  |

## Resize to quarter
|             | u8  |
|-------------|-----|
| 0.5x0.5     |  x  |

## Resize with linear interpolation
|             | u8  | f32 |
|-------------|-----|-----|
| 2x2         |  x  |  x  |
| 4x4         |  x  |  x  |
| 8x8         |     |  x  |
