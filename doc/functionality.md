<!--
SPDX-FileCopyrightText: 2023 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>

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
| InRange                      |     |  x  |     |     |     |     |     |     |  x  |     |

# Logical operations
|                              | u8  |
|------------------------------|-----|
| Bitwise And                  |  x  |

## Color conversions
|              | u8  |
|--------------|-----|
| Gray-RGB     |  x  |
| Gray-RGBA    |  x  |
| RGB-BGR      |  x  |
| BGR-RGB      |  x  |
| RGBA-BGRA    |  x  |
| BGRA-RGBA    |  x  |
| YUV420-BGR   |  x  |
| YUV420-BGRA  |  x  |
| YUV420-RGB   |  x  |
| YUV420-RGBA  |  x  |
| YUV-BGR      |  x  |
| YUV-RGB      |  x  |
| RGB-YUV      |  x  |
| RGBA-YUV     |  x  |
| BGR-YUV      |  x  |
| BGRA-YUV     |  x  |

## Data type conversions
|            | u8  | s8  | f32 |
|------------|-----|-----|-----|
| To float32 |  x  |  x  |     |
| To uint8   |     |     |  x  |
| To int8    |     |     |  x  |

## Aggregate operations
|                 | s8  | u8  | s16 | u16 | s32 | u32 | s64 | f32 |
|-----------------|-----|-----|-----|-----|-----|-----|-----|-----|
| Sum             |     |     |     |     |     |     |     |  x  |
| Minmax          |  x  |  x  |  x  |  x  |  x  |     |     |  x  |
| Minmax loc      |     |  x  |     |     |     |     |     |     |
| Count non-zeros |     |  x  |     |     |     |     |     |     |

## Matrix transformation functions
|                               | 8-bit | 16-bit | 32-bit | 64-bit |
|-------------------------------|-------|--------|--------|--------|
| Merge                         |   x   |    x   |    x   |    x   |
| Split                         |   x   |    x   |    x   |    x   |
| Transpose                     |   x   |    x   |    x   |    x   |
| Rotate (90 degrees clockwise) |   x   |    x   |    x   |    x   |

## Image filters
|                                             | s8  | u8  | s16 | u16 | s32 | u32 | f32 |
|---------------------------------------------|-----|-----|-----|-----|-----|-----|-----|
| Erode                                       |     |  x  |     |     |     |     |     |
| Dilate                                      |     |  x  |     |     |     |     |     |
| Sobel (3x3)                                 |     |  x  |     |     |     |     |     |
| Separable Filter 2D (5x5)                   |     |  x  |  x  |  x  |     |     |     |
| Gaussian Blur (3x3, 5x5, 7x7, 15x15, 21x21) |     |  x  |     |     |     |     |     |
| Median Blur (5x5, 7x7)                      |  x  |  x  |  x  |  x  |  x  |  x  |  x  |

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

# Remap
|                                                   |  1ch u8 | 1ch u16 | 2ch u8 | 2ch u16 | 4ch u8 | 4ch u16 |
|---------------------------------------------------|---------|---------|--------|---------|--------|---------|
| Remap int16 coordinates                           |    x    |    x    |        |         |        |         |
| Remap int16+uint16 fixed-point coordinates        |    x    |    x    |        |         |   x    |    x    |
| Remap float32 coordinates - nearest interpolation |    x    |    x    |    x   |    x    |        |         |
| Remap float32 coordinates - linear interpolation  |    x    |    x    |    x   |    x    |        |         |

# WarpPerspective
|                      |  u8 |
|----------------------|-----|
| Nearest neighbour    |  x  |
| Linear interpolation |  x  |
