// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

/// @file ctypes.h
/// @brief Helper type definitions

#ifndef KLEIDICV_CTYPES_H
#define KLEIDICV_CTYPES_H

#ifdef __cplusplus
#include <cinttypes>
#include <cstddef>
#else  // __cplusplus
#include "inttypes.h"
#include "stddef.h"
#endif  // __cplusplus

// This is defined in arm_neon.h or arm_sve.h, but we need it before including
// those.
typedef __fp16 float16_t;

#include "kleidicv/config.h"

/// Error values reported by KleidiCV
typedef enum KLEIDICV_NODISCARD {
  /// Success.
  KLEIDICV_OK = 0,
  /// Requested operation is not implemented.
  KLEIDICV_ERROR_NOT_IMPLEMENTED,
  /// Null pointer was passed as an argument.
  KLEIDICV_ERROR_NULL_POINTER,
  /// A value was encountered outside the representable or valid range.
  KLEIDICV_ERROR_RANGE,
  /// Could not allocate memory.
  KLEIDICV_ERROR_ALLOCATION,
  /// A value did not meet alignment requirements.
  KLEIDICV_ERROR_ALIGNMENT,
  /// The provided context (like @ref kleidicv_morphology_context_t) is not
  /// compatible with the operation.
  KLEIDICV_ERROR_CONTEXT_MISMATCH,
} kleidicv_error_t;

/// Struct to represent a point
typedef struct {
  /// x coordinate
  size_t x;
  /// y coordinate
  size_t y;
} kleidicv_point_t;

/// Struct to represent a rectangle
typedef struct {
  /// Width of the rectangle
  size_t width;
  /// Height of the rectangle
  size_t height;
} kleidicv_rectangle_t;

/// KleidiCV border types
typedef enum {
  /// The border is a constant value.
  KLEIDICV_BORDER_TYPE_CONSTANT,
  /// The border is the value of the first/last element.
  KLEIDICV_BORDER_TYPE_REPLICATE,
  /// The border is the mirrored value of the first/last elements.
  KLEIDICV_BORDER_TYPE_REFLECT,
  /// The border simply acts as a "wrap around" to the beginning/end.
  KLEIDICV_BORDER_TYPE_WRAP,
  /// Like KLEIDICV_BORDER_TYPE_REFLECT, but the first/last elements are
  /// ignored.
  KLEIDICV_BORDER_TYPE_REVERSE,
  /// The border is the "continuation" of the input rows. It is the caller's
  /// responsibility to provide the input data (and an appropriate stride value)
  /// in a way that the rows can be under and over read. E.g. can be used when
  /// executing an operation on a region of a picture.
  KLEIDICV_BORDER_TYPE_TRANSPARENT,
  /// The border is a hard border, there are no additional values to use.
  KLEIDICV_BORDER_TYPE_NONE,
} kleidicv_border_type_t;

/// KleidiCV interpolation types
typedef enum {
  /** Nearest neighbour interpolation */
  KLEIDICV_INTERPOLATION_NEAREST,
  /** Bilinear interpolation */
  KLEIDICV_INTERPOLATION_LINEAR,
} kleidicv_interpolation_type_t;

/// Internal structure where morphology operations store their state
typedef struct kleidicv_morphology_context_t_ kleidicv_morphology_context_t;

/// Internal structure where filter operations store their state
typedef struct kleidicv_filter_context_t_ kleidicv_filter_context_t;

/// Supported color conversions and base formats/modifier flags.
typedef enum {
  // Base formats:
  /// Base YUV444 format (interleaved, full resolution)
  KLEIDICV_COLOR_CONVERSION_FMT_YUV444 = 0x01,
  /// Base YUV420 semi-planar format (NV12/NV21)
  KLEIDICV_COLOR_CONVERSION_FMT_YUV420SP = 0x02,
  /// Base YUV420 planar format (I420/IYUV or YV12)
  KLEIDICV_COLOR_CONVERSION_FMT_YUV420P = 0x03,
  /// Base YUV422 format (interleaved 4:2:2)
  KLEIDICV_COLOR_CONVERSION_FMT_YUV422 = 0x04,
  /// Mask to extract the base YUV format (lower 4 bits) from a color conversion
  /// value. Use this to identify whether the base format is YUV444, YUV420SP,
  /// YUV420P, or YUV422.
  KLEIDICV_COLOR_CONVERSION_YUV_FMT_MASK = 0x0F,

  // Modifier flags:
  /// Indicates VU chroma order (V before U)
  KLEIDICV_COLOR_CONVERSION_FLAG_VU = 0x10,
  /// Indicates image data is in BGR format (instead of RGB)
  KLEIDICV_COLOR_CONVERSION_FLAG_BGR = 0x20,
  /// Indicates that alpha channel is present (RGBA or BGRA)
  KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA = 0x40,
  /// Indicates chroma byte precedes luma (Y) byte in packed 4:2:2 data
  KLEIDICV_COLOR_CONVERSION_FLAG_CHROMA_FIRST = 0x80,

  // YUV444 conversions:
  /// Convert YUV444 to RGB
  KLEIDICV_YUV444_TO_RGB = KLEIDICV_COLOR_CONVERSION_FMT_YUV444,
  /// Convert YUV444 to BGR
  KLEIDICV_YUV444_TO_BGR =
      KLEIDICV_COLOR_CONVERSION_FMT_YUV444 | KLEIDICV_COLOR_CONVERSION_FLAG_BGR,
  /// Convert YUV444 to RGBA
  KLEIDICV_YUV444_TO_RGBA = KLEIDICV_COLOR_CONVERSION_FMT_YUV444 |
                            KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA,
  /// Convert YUV444 to BGRA
  KLEIDICV_YUV444_TO_BGRA = KLEIDICV_COLOR_CONVERSION_FMT_YUV444 |
                            KLEIDICV_COLOR_CONVERSION_FLAG_BGR |
                            KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA,

  // YUV420 semi-planar (NV12/NV21) conversions:
  /// Convert NV12 (Y + interleaved UV) to RGB
  KLEIDICV_NV12_TO_RGB = KLEIDICV_COLOR_CONVERSION_FMT_YUV420SP,
  /// Convert NV12 (Y + interleaved UV) to BGR
  KLEIDICV_NV12_TO_BGR = KLEIDICV_COLOR_CONVERSION_FMT_YUV420SP |
                         KLEIDICV_COLOR_CONVERSION_FLAG_BGR,
  /// Convert NV12 (Y + interleaved UV) to RGBA
  KLEIDICV_NV12_TO_RGBA = KLEIDICV_COLOR_CONVERSION_FMT_YUV420SP |
                          KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA,
  /// Convert NV12 (Y + interleaved UV) to BGRA
  KLEIDICV_NV12_TO_BGRA = KLEIDICV_COLOR_CONVERSION_FMT_YUV420SP |
                          KLEIDICV_COLOR_CONVERSION_FLAG_BGR |
                          KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA,

  /// Convert NV21 (Y + interleaved VU) to RGB
  KLEIDICV_NV21_TO_RGB = KLEIDICV_COLOR_CONVERSION_FMT_YUV420SP |
                         KLEIDICV_COLOR_CONVERSION_FLAG_VU,
  /// Convert NV21 (Y + interleaved VU) to BGR
  KLEIDICV_NV21_TO_BGR = KLEIDICV_COLOR_CONVERSION_FMT_YUV420SP |
                         KLEIDICV_COLOR_CONVERSION_FLAG_VU |
                         KLEIDICV_COLOR_CONVERSION_FLAG_BGR,
  /// Convert NV21 (Y + interleaved VU) to RGBA
  KLEIDICV_NV21_TO_RGBA = KLEIDICV_COLOR_CONVERSION_FMT_YUV420SP |
                          KLEIDICV_COLOR_CONVERSION_FLAG_VU |
                          KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA,
  /// Convert NV21 (Y + interleaved VU) to BGRA
  KLEIDICV_NV21_TO_BGRA = KLEIDICV_COLOR_CONVERSION_FMT_YUV420SP |
                          KLEIDICV_COLOR_CONVERSION_FLAG_VU |
                          KLEIDICV_COLOR_CONVERSION_FLAG_BGR |
                          KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA,

  // YUV420 planar (I420/IYUV/YV12) conversions:
  /// Convert I420/IYUV (Y, U, V planes) to RGB
  KLEIDICV_IYUV_TO_RGB = KLEIDICV_COLOR_CONVERSION_FMT_YUV420P,
  /// Convert I420/IYUV (Y, U, V planes) to BGR
  KLEIDICV_IYUV_TO_BGR = KLEIDICV_COLOR_CONVERSION_FMT_YUV420P |
                         KLEIDICV_COLOR_CONVERSION_FLAG_BGR,
  /// Convert I420/IYUV (Y, U, V planes) to RGBA
  KLEIDICV_IYUV_TO_RGBA = KLEIDICV_COLOR_CONVERSION_FMT_YUV420P |
                          KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA,
  /// Convert I420/IYUV (Y, U, V planes) to BGRA
  KLEIDICV_IYUV_TO_BGRA = KLEIDICV_COLOR_CONVERSION_FMT_YUV420P |
                          KLEIDICV_COLOR_CONVERSION_FLAG_BGR |
                          KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA,

  /// Convert YV12 (Y, V, U planes) to RGB
  KLEIDICV_YV12_TO_RGB =
      KLEIDICV_COLOR_CONVERSION_FMT_YUV420P | KLEIDICV_COLOR_CONVERSION_FLAG_VU,
  /// Convert YV12 (Y, V, U planes) to BGR
  KLEIDICV_YV12_TO_BGR = KLEIDICV_COLOR_CONVERSION_FMT_YUV420P |
                         KLEIDICV_COLOR_CONVERSION_FLAG_VU |
                         KLEIDICV_COLOR_CONVERSION_FLAG_BGR,
  /// Convert YV12 (Y, V, U planes) to RGBA
  KLEIDICV_YV12_TO_RGBA = KLEIDICV_COLOR_CONVERSION_FMT_YUV420P |
                          KLEIDICV_COLOR_CONVERSION_FLAG_VU |
                          KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA,
  /// Convert YV12 (Y, V, U planes) to BGRA
  KLEIDICV_YV12_TO_BGRA = KLEIDICV_COLOR_CONVERSION_FMT_YUV420P |
                          KLEIDICV_COLOR_CONVERSION_FLAG_VU |
                          KLEIDICV_COLOR_CONVERSION_FLAG_BGR |
                          KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA,

  // YUV422 packed conversions:
  /// Convert YUYV (Y, U, Y, V order) to RGB
  KLEIDICV_YUYV_TO_RGB = KLEIDICV_COLOR_CONVERSION_FMT_YUV422,
  /// Convert YUYV (Y, U, Y, V order) to BGR
  KLEIDICV_YUYV_TO_BGR =
      KLEIDICV_COLOR_CONVERSION_FMT_YUV422 | KLEIDICV_COLOR_CONVERSION_FLAG_BGR,
  /// Convert YUYV (Y, U, Y, V order) to RGBA
  KLEIDICV_YUYV_TO_RGBA = KLEIDICV_COLOR_CONVERSION_FMT_YUV422 |
                          KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA,
  /// Convert YUYV (Y, U, Y, V order) to BGRA
  KLEIDICV_YUYV_TO_BGRA = KLEIDICV_COLOR_CONVERSION_FMT_YUV422 |
                          KLEIDICV_COLOR_CONVERSION_FLAG_BGR |
                          KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA,

  /// Convert YVYU (Y, V, Y, U order) to RGB
  KLEIDICV_YVYU_TO_RGB =
      KLEIDICV_COLOR_CONVERSION_FMT_YUV422 | KLEIDICV_COLOR_CONVERSION_FLAG_VU,
  /// Convert YVYU (Y, V, Y, U order) to BGR
  KLEIDICV_YVYU_TO_BGR = KLEIDICV_COLOR_CONVERSION_FMT_YUV422 |
                         KLEIDICV_COLOR_CONVERSION_FLAG_VU |
                         KLEIDICV_COLOR_CONVERSION_FLAG_BGR,
  /// Convert YVYU (Y, V, Y, U order) to RGBA
  KLEIDICV_YVYU_TO_RGBA = KLEIDICV_COLOR_CONVERSION_FMT_YUV422 |
                          KLEIDICV_COLOR_CONVERSION_FLAG_VU |
                          KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA,
  /// Convert YVYU (Y, V, Y, U order) to BGRA
  KLEIDICV_YVYU_TO_BGRA =
      KLEIDICV_COLOR_CONVERSION_FMT_YUV422 | KLEIDICV_COLOR_CONVERSION_FLAG_VU |
      KLEIDICV_COLOR_CONVERSION_FLAG_BGR | KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA,

  /// Convert UYVY (U, Y, V, Y order) to RGB
  KLEIDICV_UYVY_TO_RGB = KLEIDICV_COLOR_CONVERSION_FMT_YUV422 |
                         KLEIDICV_COLOR_CONVERSION_FLAG_CHROMA_FIRST,
  /// Convert UYVY (U, Y, V, Y order) to BGR
  KLEIDICV_UYVY_TO_BGR = KLEIDICV_COLOR_CONVERSION_FMT_YUV422 |
                         KLEIDICV_COLOR_CONVERSION_FLAG_CHROMA_FIRST |
                         KLEIDICV_COLOR_CONVERSION_FLAG_BGR,
  /// Convert UYVY (U, Y, V, Y order) to RGBA
  KLEIDICV_UYVY_TO_RGBA = KLEIDICV_COLOR_CONVERSION_FMT_YUV422 |
                          KLEIDICV_COLOR_CONVERSION_FLAG_CHROMA_FIRST |
                          KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA,
  /// Convert UYVY (U, Y, V, Y order) to BGRA
  KLEIDICV_UYVY_TO_BGRA = KLEIDICV_COLOR_CONVERSION_FMT_YUV422 |
                          KLEIDICV_COLOR_CONVERSION_FLAG_CHROMA_FIRST |
                          KLEIDICV_COLOR_CONVERSION_FLAG_BGR |
                          KLEIDICV_COLOR_CONVERSION_FLAG_ALPHA,
} kleidicv_color_conversion_t;

#endif  // KLEIDICV_CTYPES_H
