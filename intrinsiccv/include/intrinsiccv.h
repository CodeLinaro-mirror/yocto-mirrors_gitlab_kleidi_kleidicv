// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

/// @mainpage
///
/// For documentation of the IntrinsicCV C API see @ref intrinsiccv.h
///
/// Project page: https://gitlab.arm.com/intrinsiccv/intrinsiccv
///
/// @see <a href="coverage/coverage_report.html">Code Coverage Report</a>

/// @file
/// @brief For an overview of the functions and their supported types see
/// @ref intrinsiccv/src/supported-types.md.

#ifndef INTRINSICCV_H
#define INTRINSICCV_H

#include "config.h"
#include "ctypes.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#define INTRINSICCV_BINARY_OP(name, type)                                    \
  void name(const type *src_a, size_t src_a_stride, const type *src_b,       \
            size_t src_b_stride, type *dst, size_t dst_stride, size_t width, \
            size_t height)

#define INTRINSICCV_BINARY_OP_SCALE(name, type, scaletype)                   \
  void name(const type *src_a, size_t src_a_stride, const type *src_b,       \
            size_t src_b_stride, type *dst, size_t dst_stride, size_t width, \
            size_t height, scaletype scale)

/// Adds the values of the corresponding elements in `src_a` and `src_b`, and
/// puts the result into `dst`.
///
/// The addition is saturated, i.e. the result is the largest number of the
/// type of the element if the addition result would overflow. Source data
/// length (in bytes) is `stride` * `height`. Width and height are the same
/// for the two sources.
///
/// @param src_a        Pointer to the first source data. Must be non-null.
/// @param src_b        Pointer to the second source data. Must be non-null.
/// @param src_a_stride Distance in bytes from the start of one row to the
///                     start of the next row for the first source data.
///                     Must not be less than width * sizeof(type).
/// @param src_b_stride Distance in bytes from the start of one row to the
///                     start of the next row for the second source data.
///                     Must not be less than width * sizeof(type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the destination data.
///                     Must not be less than width * sizeof(type).
/// @param width        Number of elements in a row.
/// @param height       Number of rows in the data.
///
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_add_s8, int8_t);
/// @copydoc intrinsiccv_saturating_add_s8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_add_u8, uint8_t);
/// @copydoc intrinsiccv_saturating_add_s8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_add_s16, int16_t);
/// @copydoc intrinsiccv_saturating_add_s8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_add_u16, uint16_t);
/// @copydoc intrinsiccv_saturating_add_s8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_add_s32, int32_t);
/// @copydoc intrinsiccv_saturating_add_s8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_add_u32, uint32_t);
/// @copydoc intrinsiccv_saturating_add_s8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_add_s64, int64_t);
/// @copydoc intrinsiccv_saturating_add_s8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_add_u64, uint64_t);

/// Subtracts the value of the corresponding element in `src_b` from `src_a`,
/// and puts the result into `dst`.
///
/// The subtraction is saturated, i.e. the result is 0 (unsigned) or the
/// smallest possible value of the type of the element if the subtraction result
/// would underflow. Source data length (in bytes) is `stride` * `height`.
/// Width and height are the same for the two sources.
///
/// @param src_a        Pointer to the first source data. Must be non-null.
/// @param src_b        Pointer to the second source data. Must be non-null.
/// @param src_a_stride Distance in bytes from the start of one row to the
///                     start of the next row for the first source data.
///                     Must not be less than width * sizeof(type).
/// @param src_b_stride Distance in bytes from the start of one row to the
///                     start of the next row for the second source data.
///                     Must not be less than width * sizeof(type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the destination data.
///                     Must not be less than width * sizeof(type).
/// @param width        Number of elements in a row.
/// @param height       Number of rows in the data.
///
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_sub_s8, int8_t);
/// @copydoc intrinsiccv_saturating_sub_s8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_sub_u8, uint8_t);
/// @copydoc intrinsiccv_saturating_sub_s8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_sub_s16, int16_t);
/// @copydoc intrinsiccv_saturating_sub_s8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_sub_u16, uint16_t);
/// @copydoc intrinsiccv_saturating_sub_s8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_sub_s32, int32_t);
/// @copydoc intrinsiccv_saturating_sub_s8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_sub_u32, uint32_t);
/// @copydoc intrinsiccv_saturating_sub_s8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_sub_s64, int64_t);
/// @copydoc intrinsiccv_saturating_sub_s8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_sub_u64, uint64_t);

/// From the corresponding elements in `src_a` and `src_b`, subtracts the lower
/// one from the higher one, and puts the result into `dst`.
///
/// The subtraction is saturated, i.e. the result is the largest number of the
/// type of the element if the result would overflow (it is only possible with
/// signed types). Source data length (in bytes) is `stride` * `height`. Width
/// and height are the same for the two sources.
///
/// @param src_a        Pointer to the first source data. Must be non-null.
/// @param src_b        Pointer to the second source data. Must be non-null.
/// @param src_a_stride Distance in bytes from the start of one row to the
///                     start of the next row for the first source data.
///                     Must not be less than width * sizeof(type).
/// @param src_b_stride Distance in bytes from the start of one row to the
///                     start of the next row for the second source data.
///                     Must not be less than width * sizeof(type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the destination data.
///                     Must not be less than width * sizeof(type).
/// @param width        Number of elements in a row.
/// @param height       Number of rows in the data.
///
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_absdiff_u8, uint8_t);
/// @copydoc intrinsiccv_saturating_absdiff_u8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_absdiff_s8, int8_t);
/// @copydoc intrinsiccv_saturating_absdiff_u8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_absdiff_u16, uint16_t);
/// @copydoc intrinsiccv_saturating_absdiff_u8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_absdiff_s16, int16_t);
/// @copydoc intrinsiccv_saturating_absdiff_u8
INTRINSICCV_BINARY_OP(intrinsiccv_saturating_absdiff_s32, int32_t);

/// Multiplies the values of the corresponding elements in `src_a` and `src_b`,
/// and puts the result into `dst`.
///
/// The multiplication is saturated, i.e. the result is the largest number of
/// the type of the element if the multiplication result would overflow. Source
/// data length (in bytes) is `stride` * `height`. Width and height are the
/// same for the two sources.
///
/// @param src_a        Pointer to the first source data. Must be non-null.
/// @param src_b        Pointer to the second source data. Must be non-null.
/// @param src_a_stride Distance in bytes from the start of one row to the
///                     start of the next row for the first source data.
///                     Must not be less than width * sizeof(type).
/// @param src_b_stride Distance in bytes from the start of one row to the
///                     start of the next row for the second source data.
///                     Must not be less than width * sizeof(type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the destination data.
///                     Must not be less than width * sizeof(type).
/// @param width        Number of elements in a row.
/// @param height       Number of rows in the data.
/// @param scale        Currently unused parameter.
///
INTRINSICCV_BINARY_OP_SCALE(intrinsiccv_saturating_multiply_u8, uint8_t,
                            double);
/// @copydoc intrinsiccv_saturating_multiply_u8
INTRINSICCV_BINARY_OP_SCALE(intrinsiccv_saturating_multiply_s8, int8_t, double);
/// @copydoc intrinsiccv_saturating_multiply_u8
INTRINSICCV_BINARY_OP_SCALE(intrinsiccv_saturating_multiply_u16, uint16_t,
                            double);
/// @copydoc intrinsiccv_saturating_multiply_u8
INTRINSICCV_BINARY_OP_SCALE(intrinsiccv_saturating_multiply_s16, int16_t,
                            double);
/// @copydoc intrinsiccv_saturating_multiply_u8
INTRINSICCV_BINARY_OP_SCALE(intrinsiccv_saturating_multiply_s32, int32_t,
                            double);

INTRINSICCV_BINARY_OP_SCALE(intrinsiccv_add_abs_with_threshold, int16_t,
                            int16_t);

/// Converts a grayscale image to RGB. All channels are 8-bit wide.
///
/// Destination data is filled as follows: R = G = B = Gray
/// resulting in | R,G,B | R,G,B | R,G,B | ... image
/// where each letter represents one byte of data, and one pixel is represented
/// by 3 bytes. There is no padding between the pixels.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * sizeof(uint8).
/// @param width       Number of elements in a row.
/// @param height      Number of rows in the data.
///
void intrinsiccv_gray_to_rgb_u8(const uint8_t *src, size_t src_stride,
                                uint8_t *dst, size_t dst_stride, size_t width,
                                size_t height);

/// Converts a grayscale image to RGBA. All channels are 8-bit wide.
///
/// Destination data is filled as follows: R = G = B = Gray, A = 0xFF
/// resulting in | R,G,B,A | R,G,B,A | R,G,B,A | ... image
/// where each letter represents one byte of data, and one pixel is represented
/// by 4 bytes. There is no padding between the pixels.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * sizeof(uint8).
/// @param width       Number of elements in a row.
/// @param height      Number of rows in the data.
///
void intrinsiccv_gray_to_rgba_u8(const uint8_t *src, size_t src_stride,
                                 uint8_t *dst, size_t dst_stride, size_t width,
                                 size_t height);

/// Converts an RGB image to BGR. All channels are 8-bit wide.
///
/// Destination data is filled as follows:
/// | B,G,R | B,G,R | B,G,R | ...
/// Each letter represents one byte of data, and one pixel is represented
/// by 3 bytes. There is no padding between the pixels.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * sizeof(uint8).
/// @param width       Number of elements in a row.
/// @param height      Number of rows in the data.
///
void intrinsiccv_rgb_to_bgr_u8(const uint8_t *src, size_t src_stride,
                               uint8_t *dst, size_t dst_stride, size_t width,
                               size_t height);

/// Copies a source RBG image to destination buffer.
/// All channels are 8-bit wide.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * sizeof(uint8).
/// @param width       Number of elements in a row.
/// @param height      Number of rows in the data.
///
void intrinsiccv_rgb_to_rgb_u8(const uint8_t *src, size_t src_stride,
                               uint8_t *dst, size_t dst_stride, size_t width,
                               size_t height);

/// Converts an RGBA image to BGRA. All channels are 8-bit wide.
///
/// Destination data is filled as follows:
/// | B,G,R,A | B,G,R,A | B,G,R,A | ...
/// Each letter represents one byte of data, and one pixel is represented
/// by 4 bytes. There is no padding between the pixels.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * sizeof(uint8).
/// @param width       Number of elements in a row.
/// @param height      Number of rows in the data.
///
void intrinsiccv_rgba_to_bgra_u8(const uint8_t *src, size_t src_stride,
                                 uint8_t *dst, size_t dst_stride, size_t width,
                                 size_t height);

/// Copies a source RBGA image to destination buffer.
/// All channels are 8-bit wide.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * sizeof(uint8).
/// @param width       Number of elements in a row.
/// @param height      Number of rows in the data.
///
void intrinsiccv_rgba_to_rgba_u8(const uint8_t *src, size_t src_stride,
                                 uint8_t *dst, size_t dst_stride, size_t width,
                                 size_t height);

/// Converts an RGB image to BGRA. All channels are 8-bit wide.
///
/// Corresponding colours are set while Alpha channel is set to 0xFF.
/// Destination data is filled as follows:
/// | B,G,R,A | B,G,R,A | B,G,R,A | ...
/// Each letter represents one byte of data, and one pixel is represented
/// by 4 bytes. There is no padding between the pixels.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * sizeof(uint8).
/// @param width       Number of elements in a row.
/// @param height      Number of rows in the data.
///
void intrinsiccv_rgb_to_bgra_u8(const uint8_t *src, size_t src_stride,
                                uint8_t *dst, size_t dst_stride, size_t width,
                                size_t height);

/// Converts an RGB image to RGBA. All channels are 8-bit wide.
///
/// Corresponding colours are set while Alpha channel is set to 0xFF.
/// Destination data is filled as follows:
/// | R,G,B,A | R,G,B,A | R,G,B,A | ...
/// Each letter represents one byte of data, and one pixel is represented
/// by 4 bytes. There is no padding between the pixels.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * sizeof(uint8).
/// @param width       Number of elements in a row.
/// @param height      Number of rows in the data.
///
void intrinsiccv_rgb_to_rgba_u8(const uint8_t *src, size_t src_stride,
                                uint8_t *dst, size_t dst_stride, size_t width,
                                size_t height);

/// Converts an RGBA image to BGR. All channels are 8-bit wide.
///
/// Corresponding colours are set while Alpha channel is discarded.
/// Destination data is filled as follows:
/// | B,G,R | B,G,R | B,G,R | ...
/// Each letter represents one byte of data, and one pixel is represented
/// by 3 bytes. There is no padding between the pixels.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * sizeof(uint8).
/// @param width       Number of elements in a row.
/// @param height      Number of rows in the data.
///
void intrinsiccv_rgba_to_bgr_u8(const uint8_t *src, size_t src_stride,
                                uint8_t *dst, size_t dst_stride, size_t width,
                                size_t height);

/// Converts an RGBA image to RGB. All channels are 8-bit wide.
///
/// Corresponding colours are set while Alpha channel is discarded.
/// Destination data is filled as follows:
/// | R,G,B | R,G,B | R,G,B | ...
/// Each letter represents one byte of data, and one pixel is represented
/// by 3 bytes. There is no padding between the pixels.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * sizeof(uint8).
/// @param width       Number of elements in a row.
/// @param height      Number of rows in the data.
///
void intrinsiccv_rgba_to_rgb_u8(const uint8_t *src, size_t src_stride,
                                uint8_t *dst, size_t dst_stride, size_t width,
                                size_t height);

/// Converts an NV12 or NV21 YUV image to RGB. All channels are 8-bit wide.
///
/// Destination data is filled liked this:
/// | R,G,B | R,G,B | R,G,B | ...
/// Where each letter represents one byte of data, and one pixel is represented
/// by 3 bytes. There is no padding between the pixels.
/// If 4-byte alignment is required then intrinsiccv_yuv_sp_to_rgba_u8 can be
/// used.
///
/// @param src_y         Pointer to the input's Y component. Must be non-null.
/// @param src_y_stride  Distance in bytes from the start of one row to the
///                      start of the next row for the input's Y component.
///                      Must not be less than width * sizeof(u8).
/// @param src_uv        Pointer to the input's interleaved UV components.
///                      Must be non-null. If the width parameter is odd, the
///                      width of this input stream still needs to be even.
/// @param src_uv_stride Distance in bytes from the start of one row to the
///                      start of the next row for the input's UV components.
///                      Must not be less than
///                      __builtin_align_up(width, 2) * sizeof(u8).
/// @param dst           Pointer to the destination data. Must be non-null.
/// @param dst_stride    Distance in bytes from the start of one row to the
///                      start of the next row for the destination data. Must
///                      not be less than width * sizeof(type).
/// @param width         Number of elements in a row.
/// @param height        Number of rows in the data.
/// @param is_nv21       If true, input is treated as NV21, otherwise treated
///                      as NV12.
///
void intrinsiccv_yuv_sp_to_rgb_u8(const uint8_t *src_y, size_t src_y_stride,
                                  const uint8_t *src_uv, size_t src_uv_stride,
                                  uint8_t *dst, size_t dst_stride, size_t width,
                                  size_t height, bool is_nv21);

/// Converts an NV12 or NV21 YUV image to BGR. All channels are 8-bit wide.
///
/// Destination data is filled liked this:
/// | B,G,R | B,G,R | B,G,R | ...
/// Where each letter represents one byte of data, and one pixel is represented
/// by 3 bytes. There is no padding between the pixels.
/// If 4-byte alignment is required then intrinsiccv_yuv_sp_to_bgra_u8 can be
/// used.
///
/// @param src_y         Pointer to the input's Y component. Must be non-null.
/// @param src_y_stride  Distance in bytes from the start of one row to the
///                      start of the next row for the input's Y component.
///                      Must not be less than width * sizeof(u8).
/// @param src_uv        Pointer to the input's interleaved UV components.
///                      Must be non-null. If the width parameter is odd, the
///                      width of this input stream still needs to be even.
/// @param src_uv_stride Distance in bytes from the start of one row to the
///                      start of the next row for the input's UV components.
///                      Must not be less than
///                      __builtin_align_up(width, 2) * sizeof(u8).
/// @param dst           Pointer to the destination data. Must be non-null.
/// @param dst_stride    Distance in bytes from the start of one row to the
///                      start of the next row for the destination data. Must
///                      not be less than width * sizeof(type).
/// @param width         Number of elements in a row.
/// @param height        Number of rows in the data.
/// @param is_nv21       If true, input is treated as NV21, otherwise treated
///                      as NV12.
///
void intrinsiccv_yuv_sp_to_bgr_u8(const uint8_t *src_y, size_t src_y_stride,
                                  const uint8_t *src_uv, size_t src_uv_stride,
                                  uint8_t *dst, size_t dst_stride, size_t width,
                                  size_t height, bool is_nv21);

/// Converts an NV12 or NV21 YUV image to RGBA. All channels are 8-bit wide.
/// Alpha channel is set to 0xFF.
///
/// Destination data is filled liked this:
/// | R,G,B,A | R,G,B,A | R,G,B,A | ...
/// Where each letter represents one byte of data, and one pixel is represented
/// by 4 bytes. There is no padding between the pixels.
///
/// @param src_y         Pointer to the input's Y component. Must be non-null.
/// @param src_y_stride  Distance in bytes from the start of one row to the
///                      start of the next row for the input's Y component.
///                      Must not be less than width * sizeof(u8).
/// @param src_uv        Pointer to the input's interleaved UV components.
///                      Must be non-null. If the width parameter is odd, the
///                      width of this input stream still needs to be even.
/// @param src_uv_stride Distance in bytes from the start of one row to the
///                      start of the next row for the input's UV components.
///                      Must not be less than
///                      __builtin_align_up(width, 2) * sizeof(u8).
/// @param dst           Pointer to the destination data. Must be non-null.
/// @param dst_stride    Distance in bytes from the start of one row to the
///                      start of the next row for the destination data. Must
///                      not be less than width * sizeof(type).
/// @param width         Number of elements in a row.
/// @param height        Number of rows in the data.
/// @param is_nv21       If true, input is treated as NV21, otherwise treated
///                      as NV12.
///
void intrinsiccv_yuv_sp_to_rgba_u8(const uint8_t *src_y, size_t src_y_stride,
                                   const uint8_t *src_uv, size_t src_uv_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height, bool is_nv21);

/// Converts an NV12 or NV21 YUV image to BGRA. All channels are 8-bit wide.
/// Alpha channel is set to 0xFF.
///
/// Destination data is filled liked this:
/// | B,G,R,A | B,G,R,A | B,G,R,A | ...
/// Where each letter represents one byte of data, and one pixel is represented
/// by 4 bytes. There is no padding between the pixels.
///
/// @param src_y         Pointer to the input's Y component. Must be non-null.
/// @param src_y_stride  Distance in bytes from the start of one row to the
///                      start of the next row for the input's Y component.
///                      Must not be less than width * sizeof(u8).
/// @param src_uv        Pointer to the input's interleaved UV components.
///                      Must be non-null. If the width parameter is odd, the
///                      width of this input stream still needs to be even.
/// @param src_uv_stride Distance in bytes from the start of one row to the
///                      start of the next row for the input's UV components.
///                      Must not be less than
///                      __builtin_align_up(width, 2) * sizeof(u8).
/// @param dst           Pointer to the destination data. Must be non-null.
/// @param dst_stride    Distance in bytes from the start of one row to the
///                      start of the next row for the destination data. Must
///                      not be less than width * sizeof(type).
/// @param width         Number of elements in a row.
/// @param height        Number of rows in the data.
/// @param is_nv21       If true, input is treated as NV21, otherwise treated
///                      as NV12.
///
void intrinsiccv_yuv_sp_to_bgra_u8(const uint8_t *src_y, size_t src_y_stride,
                                   const uint8_t *src_uv, size_t src_uv_stride,
                                   uint8_t *dst, size_t dst_stride,
                                   size_t width, size_t height, bool is_nv21);

/// Performs a comparison of each element's value in `src` with respect to a
/// caller defined threshold. The strictly larger elements are set to
/// `value` and the rest to 0.
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the source data. Must
///                     not be less than width * sizeof(type).
/// @param dst          Pointer to the first destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the destination data. Must
///                     not be less than width * sizeof(type).
/// @param width        How many elements are in a row for the output.
/// @param height       How many rows are in the output.
/// @param threshold    The value that the elements of the source data are
///                     compared to.
/// @param value        The value that the larger elements are set to.
///
void intrinsiccv_threshold_binary_u8(const uint8_t *src, size_t src_stride,
                                     uint8_t *dst, size_t dst_stride,
                                     size_t width, size_t height,
                                     uint8_t threshold, uint8_t value);

intrinsiccv_morphology_params_t *intrinsiccv_morphology_create(
    intrinsiccv_morphology_params_t *params, intrinsiccv_rectangle_t image);

void intrinsiccv_morphology_release(intrinsiccv_morphology_params_t *params);

void intrinsiccv_dilate_u8(const uint8_t *src, size_t src_stride, uint8_t *dst,
                           size_t dst_stride, size_t width, size_t height,
                           const intrinsiccv_morphology_params_t *params);

void intrinsiccv_erode_u8(const uint8_t *src, size_t src_stride, uint8_t *dst,
                          size_t dst_stride, size_t width, size_t height,
                          const intrinsiccv_morphology_params_t *params);

/// Counts how many nonzero elements are in the source data.
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the source data.
///                     Must not be less than width * sizeof(type).
/// @param width        Number of elements in a row.
/// @param height       Number of rows in the data.
///
size_t intrinsiccv_count_nonzeros_u8(const uint8_t *src, size_t src_stride,
                                     size_t width, size_t height);

void intrinsiccv_resize_to_quarter_u8(const uint8_t *src, size_t src_stride,
                                      size_t src_width, size_t src_height,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t dst_width, size_t dst_height);

void intrinsiccv_sobel_3x3_vertical_s16_u8(const uint8_t *src,
                                           size_t src_stride, int16_t *dst,
                                           size_t dst_stride, size_t width,
                                           size_t height, size_t channel);

void intrinsiccv_sobel_3x3_horizontal_s16_u8(const uint8_t *src,
                                             size_t src_stride, int16_t *dst,
                                             size_t dst_stride, size_t width,
                                             size_t height, size_t channel);

void intrinsiccv_canny_u8(const uint8_t *src, size_t src_stride, uint8_t *dst,
                          size_t dst_stride, size_t width, size_t height,
                          double low_threshold, double high_threshold);

intrinsiccv_filter_params_t *intrinsiccv_filter_create(
    intrinsiccv_filter_params_t *params, intrinsiccv_rectangle_t image);

void intrinsiccv_filter_release(intrinsiccv_filter_params_t *params);

void intrinsiccv_gaussian_blur_3x3_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels,
    intrinsiccv_border_type_t border_type,
    const intrinsiccv_filter_params_t *params);

void intrinsiccv_gaussian_blur_5x5_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels,
    intrinsiccv_border_type_t border_type,
    const intrinsiccv_filter_params_t *params);

/// Splits a multi channel source stream into separate 1-channel streams.
///
/// @param src_data     Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row in the source data. Must not be
///                     less than width * sizeof(type) * channels.
/// @param dst_data     A C style array of pointers to the destination data.
///                     Number of pointers in the array must be the same as the
///                     channel number. All pointers must be non-null.
/// @param dst_strides  A C style array of stride values for the destination
///                     streams. A stride value represents the distance in
///                     bytes from the start of one row to the start of the
///                     next row in the given destination stream. Number of
///                     stride values in the array must be the same as the
///                     channel number. All stride values must not be less than
///                     width * sizeof(type).
/// @param width        Number of elements in a row.
/// @param height       Number of rows in the data.
/// @param channels     Number of channels in the data.
/// @param element_size Size of one element in bytes.
///
void intrinsiccv_split(const void *src_data, size_t src_stride, void **dst_data,
                       const size_t *dst_strides, size_t width, size_t height,
                       size_t channels, size_t element_size);

void intrinsiccv_transpose(const void *src, size_t src_stride, void *dst,
                           size_t dst_stride, size_t src_width,
                           size_t src_height, size_t element_size);

/// Merges separate 1-channel source streams to one multi channel stream.
///
/// @param srcs         A C style array of pointers to the source data.
///                     Number of pointers in the array must be the same as the
///                     channel number. All pointers must be non-null.
/// @param src_strides  A C style array of stride values for the source
///                     streams. A stride value represents the distance in
///                     bytes from the start of one row to the start of the
///                     next row in the given source stream. Number of
///                     stride values in the array must be the same as the
///                     channel number. All stride values must not be less than
///                     width * sizeof(type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row in the destination data. Must not
///                     be less than width * sizeof(type) * channels.
/// @param width        Number of elements in a row.
/// @param height       Number of rows in the data.
/// @param channels     Number of channels in the destination data.
/// @param element_size Size of one element in bytes.
///
void intrinsiccv_merge(const void **srcs, const size_t *src_strides, void *dst,
                       size_t dst_stride, size_t width, size_t height,
                       size_t channels, size_t element_size);

/// Calculates minimum and maximum element value across the source data.
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row in the source data. Must not be
///                     less than width * sizeof(type).
/// @param width        Number of elements in a row.
/// @param height       Number of rows in the data.
/// @param min_value    Pointer to save result minimum value to, or nullptr if
///                     minimum is not to be calculated.
/// @param max_value    Pointer to save result maximum value to, or nullptr if
///                     maximum is not to be calculated.
///
void intrinsiccv_min_max_u8(const uint8_t *src, size_t src_stride, size_t width,
                            size_t height, uint8_t *min_value,
                            uint8_t *max_value);
/// @copydoc intrinsiccv_min_max_u8
void intrinsiccv_min_max_s8(const int8_t *src, size_t src_stride, size_t width,
                            size_t height, int8_t *min_value,
                            int8_t *max_value);
/// @copydoc intrinsiccv_min_max_u8
void intrinsiccv_min_max_u16(const uint16_t *src, size_t src_stride,
                             size_t width, size_t height, uint16_t *min_value,
                             uint16_t *max_value);
/// @copydoc intrinsiccv_min_max_u8
void intrinsiccv_min_max_s16(const int16_t *src, size_t src_stride,
                             size_t width, size_t height, int16_t *min_value,
                             int16_t *max_value);
/// @copydoc intrinsiccv_min_max_u8
void intrinsiccv_min_max_s32(const int32_t *src, size_t src_stride,
                             size_t width, size_t height, int32_t *min_value,
                             int32_t *max_value);

/// Finds minimum and maximum element value across the source data,
/// and returns their location in the source data as offset in bytes
/// from the source beginning.
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row in the source data. Must not be
///                     less than width * sizeof(type).
/// @param width        Number of elements in a row.
/// @param height       Number of rows in the data.
/// @param min_offset   Pointer to save result offset of minimum value to, or
///                     nullptr if minimum is not to be calculated.
/// @param max_offset   Pointer to save result offset of maximum value to, or
///                     nullptr if maximum is not to be calculated.
///
void intrinsiccv_min_max_loc_u8(const uint8_t *src, size_t src_stride,
                                size_t width, size_t height, size_t *min_offset,
                                size_t *max_offset);

void intrinsiccv_scale_u8(const uint8_t *src, size_t src_stride, uint8_t *dst,
                          size_t dst_stride, size_t width, size_t height,
                          float scale, float shift);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // INTRINSICCV_H
