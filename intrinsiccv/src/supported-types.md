<!--
SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>

SPDX-License-Identifier: Apache-2.0
-->

# Supported types of implemented functions
Note: functions listed here are not necessarily exposed to adapter API layer.

## Basic arithmetic operations
|                    | s8  | u8  | s16 | u16 | s32 | u32 | s64 | u64 |
|--------------------|-----|-----|-----|-----|-----|-----|-----|-----|
| Saturating Add     |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |
| Saturating Sub     |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |
| Saturating Absdiff |  x  |  x  |  x  |  x  |  x  |     |     |     |
| Saturating Multiply|  x  |  x  |  x  |  x  |  x  |     |     |     |
| Threshold binary   |     |  x  |     |     |     |     |     |     |
| AddAbsWithThreshold|     |     |  x  |     |     |     |     |     |
| Scale              |     |  x  |     |     |     |     |     |     |

## Colour conversions
|  | gray-RGB | gray-RGBA | RGB-RGB | RGBA-RGBA | RGB-BGR | RGBA-BGRA | RGB-BGRA | RGB-RGBA | RGBA-BGR | RGBA-RGB |
|--|----------|-----------|---------|-----------|---------|-----------|----------|----------|----------|----------|
|u8|  x       |  x        |  x      |  x        |  x      |  x        |  x       |  x       |  x       |  x       |

|          | YUV-RBG | YUV-BGR | YUV-RGBA | YUV-BGRA |
|----------|---------|---------|----------|----------|
| u8       |  x      |  x      |  x       |  x       |

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
|             | Erode | Dilate | Sobel | Canny | Gaussian Blur  |
|-------------|-------|--------|-------|-------|----------------|
| u8          |  x    |  x     |  x    |  x    |  x             |
