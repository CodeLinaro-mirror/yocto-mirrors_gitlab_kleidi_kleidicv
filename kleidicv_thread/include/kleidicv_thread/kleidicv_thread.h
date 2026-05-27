// SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_THREAD_H
#define KLEIDICV_THREAD_H

#include "kleidicv/ctypes.h"
#include "kleidicv/kleidicv.h"

/// @file
/// @brief Public threaded C API for KleidiCV operations.
///
/// The functions declared in this header are threaded variants of the
/// corresponding `kleidicv_*` operations. They use the same operation-specific
/// parameter contracts unless documented otherwise, and add a final
/// @ref kleidicv_thread_multithreading argument that supplies the caller's
/// threading backend.
///
/// KleidiCV does not create worker threads or own a global thread pool. The
/// caller is responsible for providing a scheduler function that executes the
/// task ranges requested by KleidiCV on the caller's preferred threading
/// framework, thread pool, task system, or serial fallback.

/// @defgroup kleidicv_thread_api KleidiCV threaded C API
/// @brief Public threaded C API for KleidiCV.
/// @ingroup kleidicv_api
///
/// Threaded functions mirror the corresponding single-threaded `kleidicv_*`
/// operations and add a final @ref kleidicv_thread_multithreading argument.
/// The image, stride, size, type, in-place, and error contracts are inherited
/// from the corresponding single-threaded operation unless the threaded
/// declaration documents a different contract.

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/// @addtogroup kleidicv_thread_api
/// @{

/// Signature of a function to be invoked for a range of task indices.
///
/// The user-provided @ref kleidicv_thread_parallel implementation calls this
/// function for each range of task indices that it assigns to a worker. Each
/// task index from 0 up to, but not including, `task_count` must be covered
/// exactly once across all callback calls made for one
/// @ref kleidicv_thread_parallel invocation. Each range must satisfy
/// `0 <= begin <= end <= task_count`.
///
/// @param begin Index of the first task in the range, inclusive.
/// @param end Index one past the last task in the range, exclusive.
/// @param data Opaque pointer supplied by KleidiCV. The scheduler must pass it
///             unchanged to this callback.
typedef kleidicv_error_t (*kleidicv_thread_callback)(unsigned begin,
                                                     unsigned end, void *data);

/// Signature of a function to invoke callbacks in parallel.
///
/// KleidiCV has no dependency on any particular threading library. To run a
/// threaded operation, the caller provides an implementation of this function
/// that invokes @p callback through the caller's preferred threading framework,
/// thread pool, task system, or serial fallback.
///
/// The implementation must schedule all task indices from 0 up to, but not
/// including, `task_count` and wait for all scheduled work to finish before
/// returning. If any callback invocation returns a value other than
/// @ref KLEIDICV_OK, this function must return a non-@ref KLEIDICV_OK value.
///
/// @p task_count is the number of KleidiCV work items, not the requested number
/// of worker threads. The implementation decides how many workers to use.
/// @p callback and @p callback_data are valid only for the duration of this
/// function call and must not be retained after this function returns.
///
/// @param callback Function to invoke for one or more task ranges.
/// @param callback_data Opaque pointer to pass as the callback's @p data
///                      argument.
/// @param parallel_data Pointer from
///                      @ref kleidicv_thread_multithreading::parallel_data, for
///                      use by the implementation of this function.
/// @param task_count Number of task indices requested by KleidiCV.
typedef kleidicv_error_t (*kleidicv_thread_parallel)(
    kleidicv_thread_callback callback, void *callback_data, void *parallel_data,
    unsigned task_count);

/// Encapsulates the caller-provided threading backend.
typedef struct {
  /// User-defined scheduler callback used by KleidiCV to ask the caller to
  /// execute task ranges. Must be a valid non-null function pointer.
  kleidicv_thread_parallel parallel;
  /// Optional caller-owned data passed to @ref kleidicv_thread_parallel.
  ///
  /// The pointed-to data must remain valid until the `kleidicv_thread_*`
  /// function returns. This field may be `NULL` if the scheduler callback does
  /// not need additional context.
  void *parallel_data;
} kleidicv_thread_multithreading;

/// @name Threaded operations
/// @{

#define KLEIDICV_THREAD_UNARY_OP(name, src_type, dst_type)                     \
  kleidicv_error_t name(const src_type *src, size_t src_stride, dst_type *dst, \
                        size_t dst_stride, size_t width, size_t height,        \
                        kleidicv_thread_multithreading mt)

/// @copydoc kleidicv_gray_to_rgb_u8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_UNARY_OP(kleidicv_thread_gray_to_rgb_u8, uint8_t, uint8_t);
/// @copydoc kleidicv_gray_to_rgba_u8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_UNARY_OP(kleidicv_thread_gray_to_rgba_u8, uint8_t, uint8_t);
/// @copydoc kleidicv_rgb_to_bgr_u8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_UNARY_OP(kleidicv_thread_rgb_to_bgr_u8, uint8_t, uint8_t);
/*
 * Doxygen 1.9.8 does not combine @copydoc with the additional mt parameter for
 * these identity conversions, so keep their documentation explicit.
 */
/// Copies a source RGB image to destination buffer.
/// All channels are 8-bit wide.
///
/// Width and height are the same for the source and for the destination.
/// Number of pixels is limited to @ref KLEIDICV_MAX_IMAGE_PIXELS.
///
/// In-place operations supported.
///
/// @param src Pointer to the source data. Must be non-null.
/// @param src_stride Distance in bytes from the start of one row to the start
///                   of the next row for the source data. Must not be less
///                   than `3 * width`, except for single-row images.
/// @param dst Pointer to the destination data. Must be non-null.
/// @param dst_stride Distance in bytes from the start of one row to the start
///                   of the next row for the destination data. Must not be less
///                   than `3 * width`, except for single-row images.
/// @param width Number of pixels in a row.
/// @param height Number of rows in the data.
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_rgb_to_rgb_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, kleidicv_thread_multithreading mt);
/// @copydoc kleidicv_rgba_to_bgra_u8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_UNARY_OP(kleidicv_thread_rgba_to_bgra_u8, uint8_t, uint8_t);
/// Copies a source RGBA image to destination buffer.
/// All channels are 8-bit wide.
///
/// Width and height are the same for the source and for the destination.
/// Number of pixels is limited to @ref KLEIDICV_MAX_IMAGE_PIXELS.
///
/// @param src Pointer to the source data. Must be non-null.
/// @param src_stride Distance in bytes from the start of one row to the start
///                   of the next row for the source data. Must not be less
///                   than `4 * width`, except for single-row images.
/// @param dst Pointer to the destination data. Must be non-null.
/// @param dst_stride Distance in bytes from the start of one row to the start
///                   of the next row for the destination data. Must not be less
///                   than `4 * width`, except for single-row images.
/// @param width Number of pixels in a row.
/// @param height Number of rows in the data.
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_rgba_to_rgba_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, kleidicv_thread_multithreading mt);
/// @copydoc kleidicv_rgb_to_bgra_u8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_UNARY_OP(kleidicv_thread_rgb_to_bgra_u8, uint8_t, uint8_t);
/// @copydoc kleidicv_rgb_to_rgba_u8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_UNARY_OP(kleidicv_thread_rgb_to_rgba_u8, uint8_t, uint8_t);
/// @copydoc kleidicv_rgba_to_bgr_u8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_UNARY_OP(kleidicv_thread_rgba_to_bgr_u8, uint8_t, uint8_t);
/// @copydoc kleidicv_rgba_to_rgb_u8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_UNARY_OP(kleidicv_thread_rgba_to_rgb_u8, uint8_t, uint8_t);
/// @copydoc kleidicv_exp_f32
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_UNARY_OP(kleidicv_thread_exp_f32, float, float);
/// @copydoc kleidicv_f32_to_s8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_UNARY_OP(kleidicv_thread_f32_to_s8, float, int8_t);
/// @copydoc kleidicv_f32_to_u8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_UNARY_OP(kleidicv_thread_f32_to_u8, float, uint8_t);
/// @copydoc kleidicv_s8_to_f32
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_UNARY_OP(kleidicv_thread_s8_to_f32, int8_t, float);
/// @copydoc kleidicv_u8_to_f32
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_UNARY_OP(kleidicv_thread_u8_to_f32, uint8_t, float);

#define KLEIDICV_THREAD_INRANGE_OP(name, src_type, dst_type)                   \
  kleidicv_error_t name(const src_type *src, size_t src_stride, dst_type *dst, \
                        size_t dst_stride, size_t width, size_t height,        \
                        src_type lower_bound, src_type upper_bound,            \
                        kleidicv_thread_multithreading mt)

/// @copydoc kleidicv_in_range_u8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_INRANGE_OP(kleidicv_thread_in_range_u8, uint8_t, uint8_t);
/// @copydoc kleidicv_in_range_f32
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_INRANGE_OP(kleidicv_thread_in_range_f32, float, uint8_t);

/// @copydoc kleidicv_yuv_to_rgb_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_yuv_to_rgb_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, kleidicv_color_conversion_t color_format,
    kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_yuv_semiplanar_to_rgb_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_yuv_semiplanar_to_rgb_u8(
    const uint8_t *src_y, size_t src_y_stride, const uint8_t *src_uv,
    size_t src_uv_stride, uint8_t *dst, size_t dst_stride, size_t width,
    size_t height, kleidicv_color_conversion_t color_format,
    kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_rgb_to_yuv_semiplanar_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_rgb_to_yuv_semiplanar_u8(
    const uint8_t *src, size_t src_stride, uint8_t *y_dst, size_t y_stride,
    uint8_t *uv_dst, size_t uv_stride, size_t width, size_t height,
    kleidicv_color_conversion_t color_format,
    kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_rgb_to_yuv_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_rgb_to_yuv_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, kleidicv_color_conversion_t color_format,
    kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_min_max_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_min_max_u8(const uint8_t *src,
                                            size_t src_stride, size_t width,
                                            size_t height, uint8_t *min_value,
                                            uint8_t *max_value,
                                            kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_min_max_s8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_min_max_s8(const int8_t *src,
                                            size_t src_stride, size_t width,
                                            size_t height, int8_t *min_value,
                                            int8_t *max_value,
                                            kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_min_max_u16
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_min_max_u16(const uint16_t *src,
                                             size_t src_stride, size_t width,
                                             size_t height, uint16_t *min_value,
                                             uint16_t *max_value,
                                             kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_min_max_s16
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_min_max_s16(const int16_t *src,
                                             size_t src_stride, size_t width,
                                             size_t height, int16_t *min_value,
                                             int16_t *max_value,
                                             kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_min_max_s32
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_min_max_s32(const int32_t *src,
                                             size_t src_stride, size_t width,
                                             size_t height, int32_t *min_value,
                                             int32_t *max_value,
                                             kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_min_max_f32
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_min_max_f32(const float *src,
                                             size_t src_stride, size_t width,
                                             size_t height, float *min_value,
                                             float *max_value,
                                             kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_min_max_loc_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_min_max_loc_u8(
    const uint8_t *src, size_t src_stride, size_t width, size_t height,
    size_t *min_offset, size_t *max_offset, kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_threshold_binary_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_threshold_binary_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, uint8_t threshold, uint8_t value,
    kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_scale_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_scale_u8(const uint8_t *src, size_t src_stride,
                                          uint8_t *dst, size_t dst_stride,
                                          size_t width, size_t height,
                                          double scale, double shift,
                                          kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_scale_f32
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_scale_f32(const float *src, size_t src_stride,
                                           float *dst, size_t dst_stride,
                                           size_t width, size_t height,
                                           double scale, double shift,
                                           kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_scale_u8_f16
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_scale_u8_f16(
    const uint8_t *src, size_t src_stride, float16_t *dst, size_t dst_stride,
    size_t width, size_t height, double scale, double shift,
    kleidicv_thread_multithreading mt);

#define KLEIDICV_THREAD_BINARY_OP(name, type)                              \
  kleidicv_error_t name(const type *src_a, size_t src_a_stride,            \
                        const type *src_b, size_t src_b_stride, type *dst, \
                        size_t dst_stride, size_t width, size_t height,    \
                        kleidicv_thread_multithreading mt)

#define KLEIDICV_THREAD_BINARY_OP_SCALE(name, type, scaletype)             \
  kleidicv_error_t name(const type *src_a, size_t src_a_stride,            \
                        const type *src_b, size_t src_b_stride, type *dst, \
                        size_t dst_stride, size_t width, size_t height,    \
                        scaletype scale, kleidicv_thread_multithreading mt)

/// @copydoc kleidicv_saturating_add_s8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_add_s8, int8_t);
/// @copydoc kleidicv_saturating_add_u8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_add_u8, uint8_t);
/// @copydoc kleidicv_saturating_add_s16
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_add_s16, int16_t);
/// @copydoc kleidicv_saturating_add_u16
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_add_u16, uint16_t);
/// @copydoc kleidicv_saturating_add_s32
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_add_s32, int32_t);
/// @copydoc kleidicv_saturating_add_u32
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_add_u32, uint32_t);
/// @copydoc kleidicv_saturating_add_s64
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_add_s64, int64_t);
/// @copydoc kleidicv_saturating_add_u64
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_add_u64, uint64_t);
/// @copydoc kleidicv_saturating_sub_s8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_sub_s8, int8_t);
/// @copydoc kleidicv_saturating_sub_u8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_sub_u8, uint8_t);
/// @copydoc kleidicv_saturating_sub_s16
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_sub_s16, int16_t);
/// @copydoc kleidicv_saturating_sub_u16
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_sub_u16, uint16_t);
/// @copydoc kleidicv_saturating_sub_s32
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_sub_s32, int32_t);
/// @copydoc kleidicv_saturating_sub_u32
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_sub_u32, uint32_t);
/// @copydoc kleidicv_saturating_sub_s64
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_sub_s64, int64_t);
/// @copydoc kleidicv_saturating_sub_u64
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_sub_u64, uint64_t);
/// @copydoc kleidicv_saturating_absdiff_u8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_absdiff_u8, uint8_t);
/// @copydoc kleidicv_saturating_absdiff_s8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_absdiff_s8, int8_t);
/// @copydoc kleidicv_saturating_absdiff_u16
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_absdiff_u16, uint16_t);
/// @copydoc kleidicv_saturating_absdiff_s16
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_absdiff_s16, int16_t);
/// @copydoc kleidicv_saturating_absdiff_s32
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_saturating_absdiff_s32, int32_t);
/// @copydoc kleidicv_saturating_multiply_u8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP_SCALE(kleidicv_thread_saturating_multiply_u8, uint8_t,
                                double);
/// @copydoc kleidicv_saturating_multiply_s8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP_SCALE(kleidicv_thread_saturating_multiply_s8, int8_t,
                                double);
/// @copydoc kleidicv_saturating_multiply_u16
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP_SCALE(kleidicv_thread_saturating_multiply_u16,
                                uint16_t, double);
/// @copydoc kleidicv_saturating_multiply_s16
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP_SCALE(kleidicv_thread_saturating_multiply_s16,
                                int16_t, double);
/// @copydoc kleidicv_saturating_multiply_s32
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP_SCALE(kleidicv_thread_saturating_multiply_s32,
                                int32_t, double);
/// @copydoc kleidicv_bitwise_and
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_bitwise_and, uint8_t);
/// @copydoc kleidicv_compare_equal_u8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_compare_equal_u8, uint8_t);
/// @copydoc kleidicv_compare_greater_u8
/// @param mt Caller-provided threading backend.
KLEIDICV_THREAD_BINARY_OP(kleidicv_thread_compare_greater_u8, uint8_t);

/// @copydoc kleidicv_saturating_add_abs_with_threshold_s16
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_saturating_add_abs_with_threshold_s16(
    const int16_t *src_a, size_t src_a_stride, const int16_t *src_b,
    size_t src_b_stride, int16_t *dst, size_t dst_stride, size_t width,
    size_t height, int16_t threshold, kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_transpose
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_transpose(const void *src, size_t src_stride,
                                           void *dst, size_t dst_stride,
                                           size_t src_width, size_t src_height,
                                           size_t pixel_size,
                                           kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_rotate
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_rotate(const void *src, size_t src_stride,
                                        size_t width, size_t height, void *dst,
                                        size_t dst_stride, int angle,
                                        size_t pixel_size,
                                        kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_gaussian_blur_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_gaussian_blur_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, size_t kernel_width,
    size_t kernel_height, float sigma_x, float sigma_y,
    kleidicv_border_type_t border_type, kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_separable_filter_2d_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_separable_filter_2d_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, const uint8_t *kernel_x,
    size_t kernel_width, const uint8_t *kernel_y, size_t kernel_height,
    kleidicv_border_type_t border_type, kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_separable_filter_2d_u16
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_separable_filter_2d_u16(
    const uint16_t *src, size_t src_stride, uint16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, const uint16_t *kernel_x,
    size_t kernel_width, const uint16_t *kernel_y, size_t kernel_height,
    kleidicv_border_type_t border_type, kleidicv_thread_multithreading mt);

/// Threaded variant of blur and downsample for `uint8_t` images.
kleidicv_error_t kleidicv_thread_blur_and_downsample_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t channels,
    kleidicv_border_type_t border_type, kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_sobel_3x3_horizontal_s16_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_sobel_3x3_horizontal_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels,
    kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_median_blur_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_median_blur_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, size_t kernel_width,
    size_t kernel_height, kleidicv_border_type_t border_type,
    kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_median_blur_s16
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_median_blur_s16(
    const int16_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, size_t kernel_width,
    size_t kernel_height, kleidicv_border_type_t border_type,
    kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_median_blur_u16
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_median_blur_u16(
    const uint16_t *src, size_t src_stride, uint16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, size_t kernel_width,
    size_t kernel_height, kleidicv_border_type_t border_type,
    kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_median_blur_f32
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_median_blur_f32(
    const float *src, size_t src_stride, float *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels, size_t kernel_width,
    size_t kernel_height, kleidicv_border_type_t border_type,
    kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_sobel_3x3_vertical_s16_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_sobel_3x3_vertical_s16_u8(
    const uint8_t *src, size_t src_stride, int16_t *dst, size_t dst_stride,
    size_t width, size_t height, size_t channels,
    kleidicv_thread_multithreading mt);

/// Threaded variant of interleaved Scharr filtering from `uint8_t` input to
/// `int16_t` output.
kleidicv_error_t kleidicv_thread_scharr_interleaved_s16_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    size_t src_channels, int16_t *dst, size_t dst_stride,
    kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_resize_linear_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_resize_linear_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_resize_linear_f32
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_resize_linear_f32(
    const float *src, size_t src_stride, size_t src_width, size_t src_height,
    float *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_remap_s16_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_remap_s16_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, const int16_t *mapxy, size_t mapxy_stride,
    kleidicv_border_type_t border_type, const uint8_t *border_value,
    kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_remap_s16_u16
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_remap_s16_u16(
    const uint16_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint16_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, const int16_t *mapxy, size_t mapxy_stride,
    kleidicv_border_type_t border_type, const uint16_t *border_value,
    kleidicv_thread_multithreading mt);

#ifndef DOXYGEN
/// Threaded fixed-point remap variant for `uint8_t` images.
///
/// Functionality is similar to @ref kleidicv_thread_remap_s16_u8, but the
/// fractional part is stored separately in @p mapfrac.
kleidicv_error_t kleidicv_thread_remap_s16point5_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, const int16_t *mapxy, size_t mapxy_stride,
    const uint16_t *mapfrac, size_t mapfrac_stride,
    kleidicv_border_type_t border_type, const uint8_t *border_value,
    kleidicv_thread_multithreading mt);

/// Threaded fixed-point remap variant for `uint16_t` images.
///
/// Functionality is similar to @ref kleidicv_thread_remap_s16_u16, but the
/// fractional part is stored separately in @p mapfrac.
kleidicv_error_t kleidicv_thread_remap_s16point5_u16(
    const uint16_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint16_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, const int16_t *mapxy, size_t mapxy_stride,
    const uint16_t *mapfrac, size_t mapfrac_stride,
    kleidicv_border_type_t border_type, const uint16_t *border_value,
    kleidicv_thread_multithreading mt);
#endif  // DOXYGEN

/// @copydoc kleidicv_remap_f32_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_remap_f32_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, const float *mapx, size_t mapx_stride, const float *mapy,
    size_t mapy_stride, kleidicv_interpolation_type_t interpolation,
    kleidicv_border_type_t border_type, const uint8_t *border_value,
    kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_remap_f32_u16
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_remap_f32_u16(
    const uint16_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint16_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    size_t channels, const float *mapx, size_t mapx_stride, const float *mapy,
    size_t mapy_stride, kleidicv_interpolation_type_t interpolation,
    kleidicv_border_type_t border_type, const uint16_t *border_value,
    kleidicv_thread_multithreading mt);

/// @copydoc kleidicv_warp_perspective_u8
/// @param mt Caller-provided threading backend.
kleidicv_error_t kleidicv_thread_warp_perspective_u8(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    uint8_t *dst, size_t dst_stride, size_t dst_width, size_t dst_height,
    const float transformation[9], size_t channels,
    kleidicv_interpolation_type_t interpolation,
    kleidicv_border_type_t border_type, const uint8_t *border_value,
    kleidicv_thread_multithreading mt);

/// @}
/// @}

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // KLEIDICV_THREAD_H
