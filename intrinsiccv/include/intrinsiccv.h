// SPDX-FileCopyrightText: 2023-2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_H
#define INTRINSICCV_H

#include "config.h"
#include "ctypes.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#define INTRINSICCV_C_API(name) intrinsiccv_##name

#define INTRINSICCV_BINARY_OP(name, type, ...)                             \
  void INTRINSICCV_C_API(name)(const type *src_a, size_t src_a_stride,     \
                               const type *src_b, size_t src_b_stride,     \
                               type *dst, size_t dst_stride, size_t width, \
                               size_t height __VA_OPT__(, ) __VA_ARGS__)

/// Adds the values of the corresponding elements in `src_a` and `src_b`, and
/// puts the result into `dst`.
///
/// The addition is saturated, i.e. the result is the largest number of the
/// type of the element if the addition result would overflow. Source data
/// length (in bytes) is `stride` * `height`, it must be the same for the two
/// sources.
///
/// @param src_a        Pointer to the first source data. Must be non-null.
/// @param src_b        Pointer to the second source data. Must be non-null.
/// @param src_a_stride Distance in bytes between the row first elements for
///                     the first source data. Must not be less than
///                     width * sizeof(type).
/// @param src_b_stride Distance in bytes between the row first elements for
///                     the second source data. Must not be less than
///                     width * sizeof(type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes between the row first elements for
///                     the destination data. Must not be less than
///                     width * sizeof(type).
/// @param width        How many elements are in a row
/// @param height       How many rows are in the data
///
INTRINSICCV_BINARY_OP(saturating_add_s8, int8_t);
INTRINSICCV_BINARY_OP(saturating_add_u8, uint8_t);
INTRINSICCV_BINARY_OP(saturating_add_s16, int16_t);
INTRINSICCV_BINARY_OP(saturating_add_u16, uint16_t);
INTRINSICCV_BINARY_OP(saturating_add_s32, int32_t);
INTRINSICCV_BINARY_OP(saturating_add_u32, uint32_t);
INTRINSICCV_BINARY_OP(saturating_add_s64, int64_t);
INTRINSICCV_BINARY_OP(saturating_add_u64, uint64_t);

/// Subtracts the value of the corresponding element in `src_b` from `src_a`,
/// and puts the result into `dst`.
///
/// The subtraction is saturated, i.e. the result is 0 (unsigned) or the
/// smallest possible value of the type of the element if the subtraction result
/// would underflow. Source data length (in bytes) is `stride` * `height`,
/// it must be the same for the two sources.
///
/// @param src_a        Pointer to the first source data. Must be non-null.
/// @param src_b        Pointer to the second source data. Must be non-null.
/// @param src_a_stride Distance in bytes between the row first elements for
///                     the first source data. Must not be less than
///                     width * sizeof(type).
/// @param src_b_stride Distance in bytes between the row first elements for
///                     the second source data. Must not be less than
///                     width * sizeof(type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride   Distance in bytes between the row first elements for
///                     the destination data. Must not be less than
///                     width * sizeof(type).
/// @param width        How many elements are in a row
/// @param height       How many rows are in the data
///
INTRINSICCV_BINARY_OP(saturating_sub_s8, int8_t);
INTRINSICCV_BINARY_OP(saturating_sub_u8, uint8_t);
INTRINSICCV_BINARY_OP(saturating_sub_s16, int16_t);
INTRINSICCV_BINARY_OP(saturating_sub_u16, uint16_t);
INTRINSICCV_BINARY_OP(saturating_sub_s32, int32_t);
INTRINSICCV_BINARY_OP(saturating_sub_u32, uint32_t);
INTRINSICCV_BINARY_OP(saturating_sub_s64, int64_t);
INTRINSICCV_BINARY_OP(saturating_sub_u64, uint64_t);

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
///			start of the next row for the src_a.
///			Must not be less than width * sizeof(type).
/// @param src_b_stride Distance in bytes from the start of one row to the
///			start of the next row for the src_b.
///			Must not be less than width * sizeof(type).
/// @param dst          Pointer to the destination data. Must be non-null.
/// @param dst_stride	Distance in bytes from the start of one row to the
///			start of the next row for the destination.
///			Must not be less than width * sizeof(type).
/// @param width        How many elements are in a row
/// @param height       How many rows are in the data
///
INTRINSICCV_BINARY_OP(saturating_absdiff_u8, uint8_t);
INTRINSICCV_BINARY_OP(saturating_absdiff_s8, int8_t);
INTRINSICCV_BINARY_OP(saturating_absdiff_u16, uint16_t);
INTRINSICCV_BINARY_OP(saturating_absdiff_s16, int16_t);
INTRINSICCV_BINARY_OP(saturating_absdiff_s32, int32_t);

INTRINSICCV_BINARY_OP(saturating_multiply_u8, uint8_t, double);
INTRINSICCV_BINARY_OP(saturating_multiply_s8, int8_t, double);
INTRINSICCV_BINARY_OP(saturating_multiply_u16, uint16_t, double);
INTRINSICCV_BINARY_OP(saturating_multiply_s16, int16_t, double);
INTRINSICCV_BINARY_OP(saturating_multiply_s32, int32_t, double);

INTRINSICCV_BINARY_OP(add_abs_with_threshold, int16_t, int16_t);

void INTRINSICCV_C_API(gray_to_rgb_u8)(const uint8_t *src, size_t src_stride,
                                       uint8_t *dst, size_t dst_stride,
                                       size_t width, size_t height);

void INTRINSICCV_C_API(gray_to_rgba_u8)(const uint8_t *src, size_t src_stride,
                                        uint8_t *dst, size_t dst_stride,
                                        size_t width, size_t height);

void INTRINSICCV_C_API(rgb_to_bgr_u8)(const uint8_t *src, size_t src_stride,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t width, size_t height);

void INTRINSICCV_C_API(rgb_to_rgb_u8)(const uint8_t *src, size_t src_stride,
                                      uint8_t *dst, size_t dst_stride,
                                      size_t width, size_t height);

void INTRINSICCV_C_API(rgba_to_bgra_u8)(const uint8_t *src, size_t src_stride,
                                        uint8_t *dst, size_t dst_stride,
                                        size_t width, size_t height);

void INTRINSICCV_C_API(rgba_to_rgba_u8)(const uint8_t *src, size_t src_stride,
                                        uint8_t *dst, size_t dst_stride,
                                        size_t width, size_t height);

void INTRINSICCV_C_API(rgb_to_bgra_u8)(const uint8_t *src, size_t src_stride,
                                       uint8_t *dst, size_t dst_stride,
                                       size_t width, size_t height);

void INTRINSICCV_C_API(rgb_to_rgba_u8)(const uint8_t *src, size_t src_stride,
                                       uint8_t *dst, size_t dst_stride,
                                       size_t width, size_t height);

void INTRINSICCV_C_API(rgba_to_bgr_u8)(const uint8_t *src, size_t src_stride,
                                       uint8_t *dst, size_t dst_stride,
                                       size_t width, size_t height);

void INTRINSICCV_C_API(rgba_to_rgb_u8)(const uint8_t *src, size_t src_stride,
                                       uint8_t *dst, size_t dst_stride,
                                       size_t width, size_t height);

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
/// @param width         How many pixels are in a row for the input & output.
/// @param height        How many rows are in the input & output.
/// @param is_nv21       If true, input is treated as NV21, otherwise treated
///                      as NV12.
///
void INTRINSICCV_C_API(yuv_sp_to_rgb_u8)(const uint8_t *src_y,
                                         size_t src_y_stride,
                                         const uint8_t *src_uv,
                                         size_t src_uv_stride, uint8_t *dst,
                                         size_t dst_stride, size_t width,
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
/// @param width         How many pixels are in a row for the input & output.
/// @param height        How many rows are in the input & output.
/// @param is_nv21       If true, input is treated as NV21, otherwise treated
///                      as NV12.
///
void INTRINSICCV_C_API(yuv_sp_to_bgr_u8)(const uint8_t *src_y,
                                         size_t src_y_stride,
                                         const uint8_t *src_uv,
                                         size_t src_uv_stride, uint8_t *dst,
                                         size_t dst_stride, size_t width,
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
/// @param width         How many pixels are in a row for the input & output.
/// @param height        How many rows are in the input & output.
/// @param is_nv21       If true, input is treated as NV21, otherwise treated
///                      as NV12.
///
void INTRINSICCV_C_API(yuv_sp_to_rgba_u8)(const uint8_t *src_y,
                                          size_t src_y_stride,
                                          const uint8_t *src_uv,
                                          size_t src_uv_stride, uint8_t *dst,
                                          size_t dst_stride, size_t width,
                                          size_t height, bool is_nv21);

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
/// @param width         How many pixels are in a row for the input & output.
/// @param height        How many rows are in the input & output.
/// @param is_nv21       If true, input is treated as NV21, otherwise treated
///                      as NV12.
///
void INTRINSICCV_C_API(yuv_sp_to_bgra_u8)(const uint8_t *src_y,
                                          size_t src_y_stride,
                                          const uint8_t *src_uv,
                                          size_t src_uv_stride, uint8_t *dst,
                                          size_t dst_stride, size_t width,
                                          size_t height, bool is_nv21);

void INTRINSICCV_C_API(threshold_binary_u8)(const uint8_t *src,
                                            size_t src_stride, uint8_t *dst,
                                            size_t dst_stride, size_t width,
                                            size_t height, uint8_t threshold,
                                            uint8_t value);

intrinsiccv_morphology_params_t *INTRINSICCV_C_API(morphology_create)(
    intrinsiccv_morphology_params_t *params, intrinsiccv_rectangle_t image);

void INTRINSICCV_C_API(morphology_release)(
    intrinsiccv_morphology_params_t *params);

void INTRINSICCV_C_API(dilate_u8)(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, const intrinsiccv_morphology_params_t *params);

void INTRINSICCV_C_API(erode_u8)(const uint8_t *src, size_t src_stride,
                                 uint8_t *dst, size_t dst_stride, size_t width,
                                 size_t height,
                                 const intrinsiccv_morphology_params_t *params);

/// Counts how many nonzero elements are in the source data.
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes between the row first elements for
///                     the source data. Must not be less than
///                     width * (element size in bytes).
/// @param width        How many elements are in a row
/// @param height       How many rows are in the data
///
size_t INTRINSICCV_C_API(count_nonzeros_u8)(const uint8_t *src,
                                            size_t src_stride, size_t width,
                                            size_t height);

void INTRINSICCV_C_API(resize_to_quarter_u8)(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height);

void INTRINSICCV_C_API(sobel_3x3_vertical_s16_u8)(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channel);

void INTRINSICCV_C_API(sobel_3x3_horizontal_s16_u8)(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channel);

void INTRINSICCV_C_API(canny_u8)(const uint8_t *src, size_t src_stride,
                                 uint8_t *dst, size_t dst_stride, size_t width,
                                 size_t height, double low_threshold,
                                 double high_threshold);

intrinsiccv_filter_params_t *INTRINSICCV_C_API(filter_create)(
    intrinsiccv_filter_params_t *params, intrinsiccv_rectangle_t image);

void INTRINSICCV_C_API(filter_release)(intrinsiccv_filter_params_t *params);

void INTRINSICCV_C_API(gaussian_blur_3x3_u8)(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels,
    intrinsiccv_border_type_t border_type,
    const intrinsiccv_filter_params_t *params);

void INTRINSICCV_C_API(gaussian_blur_5x5_u8)(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels,
    intrinsiccv_border_type_t border_type,
    const intrinsiccv_filter_params_t *params);

void INTRINSICCV_C_API(split)(const void *src_data, size_t src_stride,
                              void **dst_data, size_t *dst_strides,
                              size_t width, size_t height, size_t channels,
                              size_t element_size);

void INTRINSICCV_C_API(transpose)(const void *src, size_t src_stride, void *dst,
                                  size_t dst_stride, size_t src_width,
                                  size_t src_height, size_t element_size);

void INTRINSICCV_C_API(merge)(const void **srcs, const size_t *src_strides,
                              void *dst, size_t dst_stride, size_t width,
                              size_t height, size_t channels,
                              size_t element_size);

/// Calculates minimum and maximum element value across the source data.
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row in the source data. Must not be
///                     less than width * (element size in bytes).
/// @param width        How many elements are in a row
/// @param height       How many rows are in the data
/// @param min_value    Pointer to save result minimum value to, or nullptr if
///                     minimum is not to be calculated.
/// @param max_value    Pointer to save result maximum value to, or nullptr if
///                     maximum is not to be calculated.
///
void INTRINSICCV_C_API(min_max_u8)(const uint8_t *src, size_t src_stride,
                                   size_t width, size_t height,
                                   uint8_t *min_value, uint8_t *max_value);

void INTRINSICCV_C_API(min_max_s8)(const int8_t *src, size_t src_stride,
                                   size_t width, size_t height,
                                   int8_t *min_value, int8_t *max_value);

void INTRINSICCV_C_API(min_max_u16)(const uint16_t *src, size_t src_stride,
                                    size_t width, size_t height,
                                    uint16_t *min_value, uint16_t *max_value);

void INTRINSICCV_C_API(min_max_s16)(const int16_t *src, size_t src_stride,
                                    size_t width, size_t height,
                                    int16_t *min_value, int16_t *max_value);

void INTRINSICCV_C_API(min_max_s32)(const int32_t *src, size_t src_stride,
                                    size_t width, size_t height,
                                    int32_t *min_value, int32_t *max_value);

/// Finds minimum and maximum element value across the source data,
/// and returns their location in the source data as offset in bytes
/// from the source beginning.
///
/// @param src          Pointer to the source data. Must be non-null.
/// @param src_stride   Distance in bytes from the start of one row to the
///                     start of the next row in the source data. Must not be
///                     less than width * (element size in bytes).
/// @param width        How many elements are in a row
/// @param height       How many rows are in the data
/// @param min_offset   Pointer to save result offset of minimum value to, or
///                     nullptr if minimum is not to be calculated.
/// @param max_offset   Pointer to save result offset of maximum value to, or
///                     nullptr if maximum is not to be calculated.
///
void INTRINSICCV_C_API(min_max_loc_u8)(const uint8_t *src, size_t src_stride,
                                       size_t width, size_t height,
                                       size_t *min_offset, size_t *max_offset);

void INTRINSICCV_C_API(scale_u8)(const uint8_t *src, size_t src_stride,
                                 uint8_t *dst, size_t dst_stride, size_t width,
                                 size_t height, float scale, float shift);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // INTRINSICCV_H
