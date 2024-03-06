// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

/// @mainpage
///
/// For documentation of the IntrinsicCV C API see @ref intrinsiccv.h
///
/// Project page: https://gitlab.arm.com/intrinsiccv/intrinsiccv
///
/// IntrinsicCV shall use <a href="https://semver.org/spec/v2.0.0.html">Semantic
/// Versioning 2.0.0</a>. The public API is defined according to what is
/// included in the Doxygen-generated documentation as well as the content of
/// the project's Markdown files. Features without such documentation are not
/// part of the public API and should not be relied upon.
///
/// @see <a href="coverage/coverage_report.html">Code Coverage Report</a>

/// @file
/// @brief For an overview of the functions and their supported types see
/// @ref intrinsiccv/src/supported-types.md.

#ifndef INTRINSICCV_H
#define INTRINSICCV_H

#include "intrinsiccv/config.h"
#include "intrinsiccv/ctypes.h"

#ifndef __aarch64__
#error "IntrinsicCV is only supported for aarch64"
#endif

#ifdef __aarch64__
/// Maximum image size in pixels the library accepts.
///
/// In case of AArch64 it is limited to (almost) 256 terapixels. This way 16 bit
/// is left for any arithmetic operations around image size or width or height.
#define INTRINSICCV_MAX_IMAGE_PIXELS ((1ULL << 48) - 1)
#endif

/// Size in bytes of the largest possible element type
#define INTRINSICCV_MAXIMUM_TYPE_SIZE (8)

/// Maximum number of channels
#define INTRINSICCV_MAXIMUM_CHANNEL_COUNT (8)

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#define INTRINSICCV_BINARY_OP(name, type)                                     \
  intrinsiccv_error_t name(const type *src_a, size_t src_a_stride,            \
                           const type *src_b, size_t src_b_stride, type *dst, \
                           size_t dst_stride, size_t width, size_t height)

#define INTRINSICCV_BINARY_OP_SCALE(name, type, scaletype)                    \
  intrinsiccv_error_t name(const type *src_a, size_t src_a_stride,            \
                           const type *src_b, size_t src_b_stride, type *dst, \
                           size_t dst_stride, size_t width, size_t height,    \
                           scaletype scale)

/// Adds the values of the corresponding elements in `src_a` and `src_b`, and
/// puts the result into `dst`.
///
/// The addition is saturated, i.e. the result is the largest number of the
/// type of the element if the addition result would overflow. Source data
/// length (in bytes) is `stride` * `height`. Width and height are the same
/// for the two sources and for the destination. Number of elements is limited
/// to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src_a        Pointer to the first source data. Must be non-null.
/// @param src_b        Pointer to the second source data. Must be non-null.
/// @param src_a_stride Distance in bytes from the start of one row to the
///                     start of the next row for the first source data.
///                     Must be a multiple of sizeof(type).
///                     Must not be less than width * sizeof(type).
/// @param src_b_stride Distance in bytes from the start of one row to the
///                     start of the next row for the second source data.
///                     Must be a multiple of sizeof(type).
///                     Must not be less than width * sizeof(type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the destination data.
///                     Must be a multiple of sizeof(type).
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
/// Width and height are the same for the two sources and for the destination.
/// Number of elements is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src_a        Pointer to the first source data. Must be non-null.
/// @param src_b        Pointer to the second source data. Must be non-null.
/// @param src_a_stride Distance in bytes from the start of one row to the
///                     start of the next row for the first source data.
///                     Must be a multiple of sizeof(type).
///                     Must not be less than width * sizeof(type).
/// @param src_b_stride Distance in bytes from the start of one row to the
///                     start of the next row for the second source data.
///                     Must be a multiple of sizeof(type).
///                     Must not be less than width * sizeof(type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the destination data.
///                     Must be a multiple of sizeof(type).
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
/// and height are the same for the two sources and for the destination. Number
/// of elements is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src_a        Pointer to the first source data. Must be non-null.
/// @param src_b        Pointer to the second source data. Must be non-null.
/// @param src_a_stride Distance in bytes from the start of one row to the
///                     start of the next row for the first source data.
///                     Must be a multiple of sizeof(type).
///                     Must not be less than width * sizeof(type).
/// @param src_b_stride Distance in bytes from the start of one row to the
///                     start of the next row for the second source data.
///                     Must be a multiple of sizeof(type).
///                     Must not be less than width * sizeof(type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the destination data.
///                     Must be a multiple of sizeof(type).
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
/// same for the two sources and for the destination. Number of elements is
/// limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src_a        Pointer to the first source data. Must be non-null.
/// @param src_b        Pointer to the second source data. Must be non-null.
/// @param src_a_stride Distance in bytes from the start of one row to the
///                     start of the next row for the first source data.
///                     Must be a multiple of sizeof(type).
///                     Must not be less than width * sizeof(type).
/// @param src_b_stride Distance in bytes from the start of one row to the
///                     start of the next row for the second source data.
///                     Must be a multiple of sizeof(type).
///                     Must not be less than width * sizeof(type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the destination data.
///                     Must be a multiple of sizeof(type).
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

/// Adds the absolute values of the corresponding elements in `src_a` and
/// `src_b`. Then, performs a comparison of each element's value in the result
/// with respect to a caller defined threshold. The strictly larger elements
/// remain unchanged and the rest are set to 0.
///
/// The addition is saturated, i.e. the result is the largest number of the
/// type of the element if the addition result would overflow. Source data
/// length (in bytes) is `stride` * `height`. Width and height are the same
/// for the two sources and for the destination. Number of elements is limited
/// to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src_a        Pointer to the first source data. Must be non-null.
/// @param src_b        Pointer to the second source data. Must be non-null.
/// @param src_a_stride Distance in bytes from the start of one row to the
///                     start of the next row for the first source data.
///                     Must not be less than width * sizeof(type).
///                     Must be a multiple of sizeof(type).
/// @param src_b_stride Distance in bytes from the start of one row to the
///                     start of the next row for the second source data.
///                     Must not be less than width * sizeof(type).
///                     Must be a multiple of sizeof(type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the destination data.
///                     Must not be less than width * sizeof(type).
///                     Must be a multiple of sizeof(type).
/// @param width        Number of elements in a row.
/// @param height       Number of rows in the data.
/// @param threshold    The value that the elements of the addition result
///                     are compared to.
///
intrinsiccv_error_t intrinsiccv_saturating_add_abs_with_threshold_s16(
    const int16_t *src_a, size_t src_a_stride, const int16_t *src_b,
    size_t src_b_stride, int16_t *dst, size_t dst_stride, size_t width,
    size_t height, int16_t threshold);

/// Converts a grayscale image to RGB. All channels are 8-bit wide.
///
/// Destination data is filled as follows: R = G = B = Gray
/// resulting in | R,G,B | R,G,B | R,G,B | ... image
/// where each letter represents one byte of data, and one pixel is represented
/// by 3 bytes. There is no padding between the pixels.
///
/// Width and height are the same for the source and for the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * 3 * sizeof(uint8).
/// @param width       Number of pixels in a row.
/// @param height      Number of rows in the data.
///
intrinsiccv_error_t intrinsiccv_gray_to_rgb_u8(const uint8_t *src,
                                               size_t src_stride, uint8_t *dst,
                                               size_t dst_stride, size_t width,
                                               size_t height);

/// Converts a grayscale image to RGBA. All channels are 8-bit wide.
///
/// Destination data is filled as follows: R = G = B = Gray, A = 0xFF
/// resulting in | R,G,B,A | R,G,B,A | R,G,B,A | ... image
/// where each letter represents one byte of data, and one pixel is represented
/// by 4 bytes. There is no padding between the pixels.
///
/// Width and height are the same for the source and for the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * 4 * sizeof(uint8).
/// @param width       Number of pixels in a row.
/// @param height      Number of rows in the data.
///
intrinsiccv_error_t intrinsiccv_gray_to_rgba_u8(const uint8_t *src,
                                                size_t src_stride, uint8_t *dst,
                                                size_t dst_stride, size_t width,
                                                size_t height);

/// Converts an RGB image to BGR. All channels are 8-bit wide.
///
/// Destination data is filled as follows:
/// | B,G,R | B,G,R | B,G,R | ...
/// Each letter represents one byte of data, and one pixel is represented
/// by 3 bytes. There is no padding between the pixels.
///
/// Width and height are the same for the source and for the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * 3 * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * 3 * sizeof(uint8).
/// @param width       Number of pixels in a row.
/// @param height      Number of rows in the data.
///
intrinsiccv_error_t intrinsiccv_rgb_to_bgr_u8(const uint8_t *src,
                                              size_t src_stride, uint8_t *dst,
                                              size_t dst_stride, size_t width,
                                              size_t height);

/// Copies a source RBG image to destination buffer.
/// All channels are 8-bit wide.
///
/// Width and height are the same for the source and for the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * 3 * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * 3 * sizeof(uint8).
/// @param width       Number of pixels in a row.
/// @param height      Number of rows in the data.
///
intrinsiccv_error_t intrinsiccv_rgb_to_rgb_u8(const uint8_t *src,
                                              size_t src_stride, uint8_t *dst,
                                              size_t dst_stride, size_t width,
                                              size_t height);

/// Converts an RGBA image to BGRA. All channels are 8-bit wide.
///
/// Destination data is filled as follows:
/// | B,G,R,A | B,G,R,A | B,G,R,A | ...
/// Each letter represents one byte of data, and one pixel is represented
/// by 4 bytes. There is no padding between the pixels.
///
/// Width and height are the same for the source and for the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * 4 * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * 4 * sizeof(uint8).
/// @param width       Number of pixels in a row.
/// @param height      Number of rows in the data.
///
intrinsiccv_error_t intrinsiccv_rgba_to_bgra_u8(const uint8_t *src,
                                                size_t src_stride, uint8_t *dst,
                                                size_t dst_stride, size_t width,
                                                size_t height);

/// Copies a source RBGA image to destination buffer.
/// All channels are 8-bit wide.
///
/// Width and height are the same for the source and for the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * 4 * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * 4 * sizeof(uint8).
/// @param width       Number of pixels in a row.
/// @param height      Number of rows in the data.
///
intrinsiccv_error_t intrinsiccv_rgba_to_rgba_u8(const uint8_t *src,
                                                size_t src_stride, uint8_t *dst,
                                                size_t dst_stride, size_t width,
                                                size_t height);

/// Converts an RGB image to BGRA. All channels are 8-bit wide.
///
/// Corresponding colours are set while Alpha channel is set to 0xFF.
/// Destination data is filled as follows:
/// | B,G,R,A | B,G,R,A | B,G,R,A | ...
/// Each letter represents one byte of data, and one pixel is represented
/// by 4 bytes. There is no padding between the pixels.
///
/// Width and height are the same for the source and for the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * 3 * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * 4 * sizeof(uint8).
/// @param width       Number of pixels in a row.
/// @param height      Number of rows in the data.
///
intrinsiccv_error_t intrinsiccv_rgb_to_bgra_u8(const uint8_t *src,
                                               size_t src_stride, uint8_t *dst,
                                               size_t dst_stride, size_t width,
                                               size_t height);

/// Converts an RGB image to RGBA. All channels are 8-bit wide.
///
/// Corresponding colours are set while Alpha channel is set to 0xFF.
/// Destination data is filled as follows:
/// | R,G,B,A | R,G,B,A | R,G,B,A | ...
/// Each letter represents one byte of data, and one pixel is represented
/// by 4 bytes. There is no padding between the pixels.
///
/// Width and height are the same for the source and for the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * 3 * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * 4 * sizeof(uint8).
/// @param width       Number of pixels in a row.
/// @param height      Number of rows in the data.
///
intrinsiccv_error_t intrinsiccv_rgb_to_rgba_u8(const uint8_t *src,
                                               size_t src_stride, uint8_t *dst,
                                               size_t dst_stride, size_t width,
                                               size_t height);

/// Converts an RGBA image to BGR. All channels are 8-bit wide.
///
/// Corresponding colours are set while Alpha channel is discarded.
/// Destination data is filled as follows:
/// | B,G,R | B,G,R | B,G,R | ...
/// Each letter represents one byte of data, and one pixel is represented
/// by 3 bytes. There is no padding between the pixels.
///
/// Width and height are the same for the source and for the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * 4 * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * 3 * sizeof(uint8).
/// @param width       Number of pixels in a row.
/// @param height      Number of rows in the data.
///
intrinsiccv_error_t intrinsiccv_rgba_to_bgr_u8(const uint8_t *src,
                                               size_t src_stride, uint8_t *dst,
                                               size_t dst_stride, size_t width,
                                               size_t height);

/// Converts an RGBA image to RGB. All channels are 8-bit wide.
///
/// Corresponding colours are set while Alpha channel is discarded.
/// Destination data is filled as follows:
/// | R,G,B | R,G,B | R,G,B | ...
/// Each letter represents one byte of data, and one pixel is represented
/// by 3 bytes. There is no padding between the pixels.
///
/// Width and height are the same for the source and for the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src         Pointer to the source data. Must be non-null.
/// @param src_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the source data.
///                    Must not be less than width * 4 * sizeof(uint8).
/// @param dst         Pointer to the destination data. Must be non-null.
/// @param dst_stride  Distance in bytes from the start of one row to the
///                    start of the next row for the destination data.
///                    Must not be less than width * 3 * sizeof(uint8).
/// @param width       Number of pixels in a row.
/// @param height      Number of rows in the data.
///
intrinsiccv_error_t intrinsiccv_rgba_to_rgb_u8(const uint8_t *src,
                                               size_t src_stride, uint8_t *dst,
                                               size_t dst_stride, size_t width,
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
/// Width and height are the same for the source and for the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
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
///                      not be less than width * 3 * sizeof(type).
/// @param width         Number of pixels in a row.
/// @param height        Number of rows in the data.
/// @param is_nv21       If true, input is treated as NV21, otherwise treated
///                      as NV12.
///
intrinsiccv_error_t intrinsiccv_yuv_sp_to_rgb_u8(
    const uint8_t *src_y, size_t src_y_stride, const uint8_t *src_uv,
    size_t src_uv_stride, uint8_t *dst, size_t dst_stride, size_t width,
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
/// Width and height are the same for the source and for the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
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
///                      not be less than width * 3 * sizeof(type).
/// @param width         Number of pixels in a row.
/// @param height        Number of rows in the data.
/// @param is_nv21       If true, input is treated as NV21, otherwise treated
///                      as NV12.
///
intrinsiccv_error_t intrinsiccv_yuv_sp_to_bgr_u8(
    const uint8_t *src_y, size_t src_y_stride, const uint8_t *src_uv,
    size_t src_uv_stride, uint8_t *dst, size_t dst_stride, size_t width,
    size_t height, bool is_nv21);

/// Converts an NV12 or NV21 YUV image to RGBA. All channels are 8-bit wide.
/// Alpha channel is set to 0xFF.
///
/// Destination data is filled liked this:
/// | R,G,B,A | R,G,B,A | R,G,B,A | ...
/// Where each letter represents one byte of data, and one pixel is represented
/// by 4 bytes. There is no padding between the pixels.
///
/// Width and height are the same for the source and for the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
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
///                      not be less than width * 4 * sizeof(type).
/// @param width         Number of pixels in a row.
/// @param height        Number of rows in the data.
/// @param is_nv21       If true, input is treated as NV21, otherwise treated
///                      as NV12.
///
intrinsiccv_error_t intrinsiccv_yuv_sp_to_rgba_u8(
    const uint8_t *src_y, size_t src_y_stride, const uint8_t *src_uv,
    size_t src_uv_stride, uint8_t *dst, size_t dst_stride, size_t width,
    size_t height, bool is_nv21);

/// Converts an NV12 or NV21 YUV image to BGRA. All channels are 8-bit wide.
/// Alpha channel is set to 0xFF.
///
/// Destination data is filled liked this:
/// | B,G,R,A | B,G,R,A | B,G,R,A | ...
/// Where each letter represents one byte of data, and one pixel is represented
/// by 4 bytes. There is no padding between the pixels.
///
/// Width and height are the same for the source and for the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
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
///                      not be less than width * 4 * sizeof(type).
/// @param width         Number of pixels in a row.
/// @param height        Number of rows in the data.
/// @param is_nv21       If true, input is treated as NV21, otherwise treated
///                      as NV12.
///
intrinsiccv_error_t intrinsiccv_yuv_sp_to_bgra_u8(
    const uint8_t *src_y, size_t src_y_stride, const uint8_t *src_uv,
    size_t src_uv_stride, uint8_t *dst, size_t dst_stride, size_t width,
    size_t height, bool is_nv21);

/// Performs a comparison of each element's value in `src` with respect to a
/// caller defined threshold. The strictly larger elements are set to
/// `value` and the rest to 0.
///
/// Width and height are the same for the source and for the destination. Number
/// of elements is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the source data. Must
///                     not be less than width * sizeof(type).
///                     Must be a multiple of sizeof(type).
/// @param dst          Pointer to the first destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the destination data. Must
///                     not be less than width * sizeof(type).
///                     Must be a multiple of sizeof(type).
/// @param width        Number of elements in a row.
/// @param height       Number of rows in the data.
/// @param threshold    The value that the elements of the source data are
///                     compared to.
/// @param value        The value that the larger elements are set to.
///
intrinsiccv_error_t intrinsiccv_threshold_binary_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, uint8_t threshold, uint8_t value);

/// Creates a morphology context according to the parameters.
///
/// Before a @ref intrinsiccv_dilate_u8 "dilate" or @ref intrinsiccv_erode_u8
/// "erode" operation, this initialization is needed. After the operation is
/// finished, the context needs to be released using @ref
/// intrinsiccv_morphology_release.
///
/// @param context       Pointer where to return the created context's address.
/// @param kernel        Width and height of the kernel. Its size must not be
///                      more than @ref INTRINSICCV_MAX_IMAGE_PIXELS.
/// @param anchor        Location in the kernel which is aligned to the actual
///                      point in the source data. Must not point out of the
///                      kernel.
/// @param border_type   Way of handling the border. The supported border types
///                      are: \n
///                         - @ref INTRINSICCV_BORDER_TYPE_CONSTANT \n
///                         - @ref INTRINSICCV_BORDER_TYPE_REPLICATE
/// @param border_values Border values if the border_type is
///                      @ref INTRINSICCV_BORDER_TYPE_CONSTANT.
/// @param channels      Number of channels in the data. Must be not more than
///                      @ref INTRINSICCV_MAXIMUM_CHANNEL_COUNT.
/// @param iterations    Number of times to do the morphology operation.
/// @param type_size     Element size in bytes. Must not be more than
///                      @ref INTRINSICCV_MAXIMUM_TYPE_SIZE.
/// @param image         Image dimensions. Its size must not be more than
///                      @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
intrinsiccv_error_t intrinsiccv_morphology_create(
    intrinsiccv_morphology_context_t **context, intrinsiccv_rectangle_t kernel,
    intrinsiccv_point_t anchor, intrinsiccv_border_type_t border_type,
    intrinsiccv_border_values_t border_values, size_t channels,
    size_t iterations, size_t type_size, intrinsiccv_rectangle_t image);

/// Releases a morphology context that was previously created using @ref
/// intrinsiccv_morphology_create.
///
/// @param context      Pointer to morphology context. Must not be nullptr.
///
intrinsiccv_error_t intrinsiccv_morphology_release(
    intrinsiccv_morphology_context_t *context);

/// Calculates maximum (dilate) or minimum (erode) element value of `src`
/// values using a given kernel which has a rectangular shape, and puts the
/// result into `dst`.
///
/// Width and height are the same for the source and the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// The kernel has an anchor point, it is usually the center of the kernel.
/// The algorithm takes a rectangle from the source data using the kernel and
/// the anchor, and calculates the max/min value in that rectangle.
///
/// Example for dilate:
/// ```
///    [ 2, 8, 9, 3, 6 ]  kernel: (3, 3)        [ 8, 9, 9, 9, 6 ]
///    [ 7, 2, 4, 1, 5 ]  anchor: (1, 1)     -> [ 8, 9, 9, 9, 8 ]
///    [ 4, 3, 6, 8, 1 ]  border: replicate     [ 7, 7, 8, 8, 8 ]
///    [ 1, 2, 5, 3, 7 ]                        [ 4, 6, 8, 8, 8 ]
/// ```
/// Usage:
///
/// Before using this function, a context must be created using
/// @ref intrinsiccv_morphology_create, and after finished, it has to be
/// released using @ref intrinsiccv_morphology_release.
/// The context must be created with the same image dimensions as width and
/// height parameters, with sizeof(uint8) as size_type, and with the channel
/// number of the data as channels.
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the source data.
///                     Must not be less than width * channels * sizeof(uint8).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the destination data. Must
///                     not be less than width * channels * sizeof(uint8).
/// @param width        Number of pixels in a row.
/// @param height       Number of rows in the data.
/// @param context      Pointer to morphology context.
///
intrinsiccv_error_t intrinsiccv_dilate_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, intrinsiccv_morphology_context_t *context);

/// @copydoc intrinsiccv_dilate_u8
///
intrinsiccv_error_t intrinsiccv_erode_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, intrinsiccv_morphology_context_t *context);

/// Counts how many nonzero elements are in the source data. Number of elements
/// is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the source data.
///                     Must be a multiple of sizeof(type).
///                     Must not be less than width * sizeof(type).
/// @param width        Number of elements in a row.
/// @param height       Number of rows in the data.
/// @param count        Pointer to variable to store result. Must be non-null.
///
intrinsiccv_error_t intrinsiccv_count_nonzeros_u8(const uint8_t *src,
                                                  size_t src_stride,
                                                  size_t width, size_t height,
                                                  size_t *count);

/// Resizes source data by averaging 4 elements to one.
///
/// For even source dimensions `(2*N, 2*M)` destination dimensions should be
/// `(N, M)`.
/// In case of odd source dimensions `(2*N+1, 2*M+1)` destination
/// dimensions could be either `(N+1, M+1)` or `(N, M)` or combination of both.
/// For later cases last respective row or column of source data will not be
/// processed. Currently only supports single-channel data. Number of pixels in
/// the source is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// Even dimension example of 2x2 to 1x1 conversion:
/// ```
/// | a | b | --> | (a+b+c+d)/4 |
/// | c | d |
/// ```
/// Odd dimension example of 3x3 to 2x2 conversion:
/// ```
/// | a | b | c |     | (a+b+c+d)/4 | (c+f)/2 |
/// | d | e | f | --> |   (g+h)/2   |    i    |
/// | g | h | i |
/// ```
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the source data.
///                     Must be a multiple of sizeof(type).
///                     Must not be less than width * sizeof(type).
/// @param src_width    Number of elements in the source row.
/// @param src_height   Number of rows in the source data.
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the destination data.
///                     Must be a multiple of sizeof(type).
///                     Must not be less than width * sizeof(type).
/// @param dst_width    Number of elements in the destination row.
///                     Must be src_width / 2 for even src_width.
///                     For odd src_width it must be either src_width / 2
///                     or (src_width / 2) + 1.
/// @param dst_height   Number of rows in the destination data.
///                     Must be src_height / 2 for even src_height.
///                     For odd src_height it must be either src_height / 2
///                     or (src_height / 2) + 1.
///
intrinsiccv_error_t intrinsiccv_resize_to_quarter_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height);

/// Calculates vertical derivative approximation with Sobel filter.
///
/// The used convolution kernel is:
/// ```
/// [  1  2  1 ]
/// [  0  0  0 ]
/// [ -1 -2 -1 ]
/// ```
/// Note, that the kernel is mirrored both vertically and horizontally during
/// the convolution.
///
/// The only supported border type is @ref INTRINSICCV_BORDER_TYPE_REPLICATE
///
/// Width and height are the same for the source and for the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row in the source data. Must not be
///                     less than width * sizeof(src type) * channels.
///                     Must be a multiple of sizeof(src type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row in the destination data. Must not
///                     be less than width * sizeof(dst type) * channels.
///                     Must be a multiple of sizeof(dst type).
/// @param width        Number of pixels in the data. (One pixel consists of
///                     'channels' number of elements.)
/// @param height       Number of rows in the data.
/// @param channels     Number of channels in the data. Must be not more than
///                     @ref INTRINSICCV_MAXIMUM_CHANNEL_COUNT.
///
intrinsiccv_error_t intrinsiccv_sobel_3x3_vertical_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels);

/// Calculates horizontal derivative approximation with Sobel filter.
///
/// The used convolution kernel is:
/// ```
/// [  1  0 -1 ]
/// [  2  0 -2 ]
/// [  1  0 -1 ]
/// ```
/// Note, that the kernel is mirrored both vertically and horizontally during
/// the convolution.
///
/// The only supported border type is @ref INTRINSICCV_BORDER_TYPE_REPLICATE
///
/// Width and height are the same for the source and for the destination. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row in the source data. Must not be
///                     less than width * sizeof(src type) * channels.
///                     Must be a multiple of sizeof(src type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row in the destination data. Must not
///                     be less than width * sizeof(dst type) * channels.
///                     Must be a multiple of sizeof(dst type).
/// @param width        Number of pixels in the data. (One pixel consists of
///                     'channels' number of elements.)
/// @param height       Number of rows in the data.
/// @param channels     Number of channels in the data. Must be not more than
///                     @ref INTRINSICCV_MAXIMUM_CHANNEL_COUNT.
///
intrinsiccv_error_t intrinsiccv_sobel_3x3_horizontal_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels);

#if INTRINSICCV_EXPERIMENTAL_FEATURE_CANNY
/// Canny edge detector for uint8_t grayscale input. Output is also a uint8_t
/// grayscale image. Width and height are the same for input and output. Number
/// of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// The steps:
///  - Execute horizontal and vertical Sobel filtering with 3*3 kernels to
///    calculate the gradient approximation in both directions.
///  - Calculate magnitude approximation (by summing horizontal and vertical
///    gradient approximations) and apply lower threshold.
///  - Perform non-maxima suppression and high thresholding.
///  - Perform hysteresis: promote weak edges, which are connected to strong
///    edges, to strong edges.
///  - Suppress remaining weak edges.
///
/// @param src            Pointer to the source data. Must be non-null.
/// @param src_stride     Distance in bytes from the start of one row to the
///                       start of the next row in the source data. Must not be
///                       less than width.
/// @param dst            Pointer to the destination data. Must be non-null.
/// @param dst_stride     Distance in bytes from the start of one row to the
///                       start of the next row in the destination data. Must
///                       not be less than width.
/// @param width          Number of elements in a row.
/// @param height         Number of rows in the data.
/// @param low_threshold  Low threshold for the edge detector algorithm.
/// @param high_threshold High threshold for the edge detector algorithm.
///
intrinsiccv_error_t intrinsiccv_canny_u8(const uint8_t *src, size_t src_stride,
                                         uint8_t *dst, size_t dst_stride,
                                         size_t width, size_t height,
                                         double low_threshold,
                                         double high_threshold);
#endif  // INTRINSICCV_EXPERIMENTAL_FEATURE_CANNY

/// Creates a filter context according to the parameters.
///
/// Before a gaussian_blur operation, this initialization is needed.
/// After the operation is finished, the context needs to be released
/// using @ref intrinsiccv_filter_release.
///
/// @param context       Pointer where to return the created context's address.
/// @param channels      Number of channels in the data. Must be not more than
///                      @ref INTRINSICCV_MAXIMUM_CHANNEL_COUNT.
/// @param type_size     Size of buffer element in bytes. It must be double the
///                      size of the type the filter operation is executed on.
/// @param image         Image dimensions. Its size must not be more than
///                      @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
intrinsiccv_error_t intrinsiccv_filter_create(
    intrinsiccv_filter_context_t **context, size_t channels, size_t type_size,
    intrinsiccv_rectangle_t image);

/// Releases a filter context that was previously created using @ref
/// intrinsiccv_filter_create.
///
/// @param context      Pointer to filter context. Must not be nullptr.
///
intrinsiccv_error_t intrinsiccv_filter_release(
    intrinsiccv_filter_context_t *context);

/// Convolves the source image with the specified Gaussian kernel.
/// In-place filtering is not supported.
///
/// 3x3 Gaussian Blur filter for uint8_t types:
/// ```
///        [ 1, 2, 1 ]
/// 1/16 * [ 2, 4, 2 ]
///        [ 1, 2, 1 ]
/// ```
/// 5x5 Gaussian Blur filter for uint8_t types:
/// ```
///         [ 1,  4,  6,  4, 1 ]
///         [ 4, 16, 24, 16, 4 ]
/// 1/256 * [ 6, 24, 36, 24, 6 ]
///         [ 4, 16, 24, 16, 4 ]
///         [ 1,  4,  6,  4, 1 ]
/// ```
///
/// Width and height are the same for the source and for the destination. Number
/// of elements is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// Usage: \n
/// Before using this function, a context must be created using
/// intrinsiccv_filter_create, and after finished, it has to be released
/// using intrinsiccv_filter_release. The context must be created with the same
/// image dimensions as width and height parameters, with sizeof(uint8) as
/// size_type, and with the channel number of the data as channels. \n
/// Note, from the border types only these are supported: \n
///                       - @ref INTRINSICCV_BORDER_TYPE_REPLICATE \n
///                       - @ref INTRINSICCV_BORDER_TYPE_REFLECT \n
///                       - @ref INTRINSICCV_BORDER_TYPE_WRAP \n
///                       - @ref INTRINSICCV_BORDER_TYPE_REVERSE
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row in the source data. Must not be
///                     less than width * sizeof(type) * channels.
///                     Must be a multiple of sizeof(type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row in the destination data. Must not
///                     be less than width * sizeof(type) * channels.
///                     Must be a multiple of sizeof(type).
/// @param width        Number of columns in the data. (One column consists of
///                     'channels' number of elements.)
/// @param height       Number of rows in the data.
/// @param channels     Number of channels in the data. Must be not more than
///                     @ref INTRINSICCV_MAXIMUM_CHANNEL_COUNT.
/// @param border_type  Way of handling the border.
/// @param context      Pointer to filter context.
///
intrinsiccv_error_t intrinsiccv_gaussian_blur_3x3_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels,
    intrinsiccv_border_type_t border_type,
    intrinsiccv_filter_context_t *context);

/// @copydoc intrinsiccv_gaussian_blur_3x3_u8
///
intrinsiccv_error_t intrinsiccv_gaussian_blur_5x5_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels,
    intrinsiccv_border_type_t border_type,
    intrinsiccv_filter_context_t *context);

/// Splits a multi channel source stream into separate 1-channel streams. Width
/// and height are the same for the source stream and for all the destination
/// streams. Number of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src_data     Pointer to the source data. Must be non-null.
///                     Must be aligned to element_size.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row in the source data. Must not be
///                     less than width * element_size * channels.
///                     Must be a multiple of element_size.
/// @param dst_data     A C style array of pointers to the destination data.
///                     Number of pointers in the array must be the same as the
///                     channel number. All pointers must be non-null.
///                     All pointers must be aligned to element_size.
/// @param dst_strides  A C style array of stride values for the destination
///                     streams. A stride value represents the distance in
///                     bytes from the start of one row to the start of the
///                     next row in the given destination stream. Number of
///                     stride values in the array must be the same as the
///                     channel number. All stride values must be a multiple of
///                     element_size and not be less than width * element_size.
/// @param width        Number of pixels in one row of the source data. (One
///                     pixel consists of 'channels' number of elements.)
/// @param height       Number of rows in the source data.
/// @param channels     Number of channels in the source data. Must be 2, 3 or
///                     4.
/// @param element_size Size of one element in bytes. Must be 1, 2, 4 or 8.
///
intrinsiccv_error_t intrinsiccv_split(const void *src_data, size_t src_stride,
                                      void **dst_data,
                                      const size_t *dst_strides, size_t width,
                                      size_t height, size_t channels,
                                      size_t element_size);

/// Matrix transpose operation.
/// Inplace transpose ('src == dst') is only supported for
/// square matrixes (`src_width == src_height`).
///
/// Example for `src[4,3]` to `dst[3,4]`:
/// ```
/// | 0 | 2 | 2 | 2 |    | 0 | 1 | 1 |
/// | 1 | 0 | 2 | 2 | -> | 2 | 0 | 1 |
/// | 1 | 1 | 0 | 0 |    | 2 | 2 | 0 |
///                      | 2 | 2 | 2 |
/// ```
///
/// Number of elements is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src          Pointer to the source data. Must be non-null.
///                     Must be aligned to element_size.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the source data.
///                     Must be a multiple of element_size.
///                     Must not be less than width * element_size.
/// @param dst          Pointer to the destination data. Must be non-null.
///                     Can be the same as source data for inplace operation.
///                     Must be aligned to element_size.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the destination data.
///                     Must be a multiple of element_size.
///                     Must not be less than height * element_size.
/// @param src_width    Number of elements in a row.
/// @param src_height   Number of rows in the data.
/// @param element_size Size of one element in bytes. Must be 1, 2, 4 or 8.
///
intrinsiccv_error_t intrinsiccv_transpose(const void *src, size_t src_stride,
                                          void *dst, size_t dst_stride,
                                          size_t src_width, size_t src_height,
                                          size_t element_size);

/// Merges separate 1-channel source streams to one multi channel stream. Width
/// and height are the same for all the source streams and for the destination.
/// Number of pixels is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param srcs         A C style array of pointers to the source data.
///                     Number of pointers in the array must be the same as the
///                     channel number. All pointers must be non-null.
///                     All pointers must be aligned to element_size.
/// @param src_strides  A C style array of stride values for the source
///                     streams. A stride value represents the distance in
///                     bytes from the start of one row to the start of the
///                     next row in the given source stream. Number of
///                     stride values in the array must be the same as the
///                     channel number. All stride values must be a multiple of
///                     element_size and not be less than width * element_size.
/// @param dst          Pointer to the destination data. Must be non-null.
///                     Must be aligned to element_size.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row in the destination data. Must not
///                     be less than width * element_size * channels.
///                     Must be a multiple of element_size.
/// @param width        Number of elements in a row for the source streams,
///                     number of pixels in a row for the destination data.
/// @param height       Number of rows in the data.
/// @param channels     Number of channels in the destination data. Must be 2,
///                     3 or 4.
/// @param element_size Size of one element in bytes. Must be 1, 2, 4 or 8.
///
intrinsiccv_error_t intrinsiccv_merge(const void **srcs,
                                      const size_t *src_strides, void *dst,
                                      size_t dst_stride, size_t width,
                                      size_t height, size_t channels,
                                      size_t element_size);

/// Calculates minimum and maximum element value across the source data. Number
/// of elements is limited to @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row in the source data. Must not be
///                     less than width * sizeof(type).
///                     Must be a multiple of sizeof(type).
/// @param width        Number of elements in a row. Must be greater than 0.
/// @param height       Number of rows in the data. Must be greater than 0.
/// @param min_value    Pointer to save result minimum value to, or nullptr if
///                     minimum is not to be calculated.
/// @param max_value    Pointer to save result maximum value to, or nullptr if
///                     maximum is not to be calculated.
///
intrinsiccv_error_t intrinsiccv_min_max_u8(const uint8_t *src,
                                           size_t src_stride, size_t width,
                                           size_t height, uint8_t *min_value,
                                           uint8_t *max_value);
/// @copydoc intrinsiccv_min_max_u8
intrinsiccv_error_t intrinsiccv_min_max_s8(const int8_t *src, size_t src_stride,
                                           size_t width, size_t height,
                                           int8_t *min_value,
                                           int8_t *max_value);
/// @copydoc intrinsiccv_min_max_u8
intrinsiccv_error_t intrinsiccv_min_max_u16(const uint16_t *src,
                                            size_t src_stride, size_t width,
                                            size_t height, uint16_t *min_value,
                                            uint16_t *max_value);
/// @copydoc intrinsiccv_min_max_u8
intrinsiccv_error_t intrinsiccv_min_max_s16(const int16_t *src,
                                            size_t src_stride, size_t width,
                                            size_t height, int16_t *min_value,
                                            int16_t *max_value);
/// @copydoc intrinsiccv_min_max_u8
intrinsiccv_error_t intrinsiccv_min_max_s32(const int32_t *src,
                                            size_t src_stride, size_t width,
                                            size_t height, int32_t *min_value,
                                            int32_t *max_value);

/// Finds minimum and maximum element value across the source data,
/// and returns their location in the source data as offset in bytes
/// from the source beginning. Number of elements is limited to
/// @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row in the source data. Must not be
///                     less than width * sizeof(type).
///                     Must be a multiple of sizeof(type).
/// @param width        Number of elements in a row. Must be greater than 0.
/// @param height       Number of rows in the data. Must be greater than 0.
/// @param min_offset   Pointer to save result offset of minimum value to, or
///                     nullptr if minimum is not to be calculated.
/// @param max_offset   Pointer to save result offset of maximum value to, or
///                     nullptr if maximum is not to be calculated.
///
intrinsiccv_error_t intrinsiccv_min_max_loc_u8(const uint8_t *src,
                                               size_t src_stride, size_t width,
                                               size_t height,
                                               size_t *min_offset,
                                               size_t *max_offset);

/// Multiplies the elements in `src` by `scale`, then adds `shift` to the
/// result and stores it in `dst`.
///
/// The result is saturated, i.e. it is the smallest/largest number of the
/// type of the element if the result would underflow/overflow. Source data
/// length (in bytes) is `stride` * `height`. Width and height are the same
/// for the source and destination. Number of elements is limited to
/// @ref INTRINSICCV_MAX_IMAGE_PIXELS.
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the source data.
///                     Must not be less than width * sizeof(type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes from the start of one row to the
///                     start of the next row for the destination data.
///                     Must not be less than width * sizeof(type).
/// @param width        Number of elements in a row.
/// @param height       Number of rows in the data.
/// @param scale        Value to multiply the input by.
/// @param shift        Value to add to the result.
///
intrinsiccv_error_t intrinsiccv_scale_u8(const uint8_t *src, size_t src_stride,
                                         uint8_t *dst, size_t dst_stride,
                                         size_t width, size_t height,
                                         float scale, float shift);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // INTRINSICCV_H
