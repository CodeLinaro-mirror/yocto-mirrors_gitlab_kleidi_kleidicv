// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_THREAD_H
#define KLEIDICV_THREAD_H

#include "kleidicv/kleidicv.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/// Internal - not part of the public API and its direct use is not supported.
///
/// Signature of a function to be invoked for each thread.
/// @param begin index of first task.
/// @param end index of the last task + 1.
/// @param data opaque pointer to data internal to the callback implementation.
typedef kleidicv_error_t (*kleidicv_thread_callback)(unsigned begin,
                                                     unsigned end, void *data);

/// Internal - not part of the public API and its direct use is not supported.
///
/// Signature of a function to invoke callbacks in parallel.
///
/// KleidiCV has no dependencies on any particular threading library.
/// Therefore, to allow multithreading to work an implementation of this
/// function must be provided to invoke callbacks across multiple threads via
/// the preferred threading library or framework.
///
/// @param callback the function to invoke in parallel.
/// @param callback_data opaque pointer to pass as the callback's data argument.
/// @param parallel_data pointer from kleidicv_thread_multithreading_t, for use
///                      by the implementation of this function.
/// @param task_count number of tasks to run.
typedef kleidicv_error_t (*kleidicv_thread_parallel)(
    kleidicv_thread_callback callback, void *callback_data, void *parallel_data,
    unsigned task_count);

/// Internal - not part of the public API and its direct use is not supported.
///
/// Encapsulates the data required to run callbacks across multiple threads.
typedef struct {
  kleidicv_thread_parallel parallel;
  void *parallel_data;
} kleidicv_thread_multithreading;

/// Internal - not part of the public API and its direct use is not supported.
///
/// Multithreaded implementation of kleidicv_yuv_sp_to_rgb_u8 - see the
/// documentation of that function for more details.
kleidicv_error_t kleidicv_thread_yuv_sp_to_rgb_u8(
    const uint8_t *src_y, size_t src_y_stride, const uint8_t *src_uv,
    size_t src_uv_stride, uint8_t *dst, size_t dst_stride, size_t width,
    size_t height, bool is_nv21, kleidicv_thread_multithreading);

/// Multithreaded implementation of kleidicv_min_max_u8 - see the
/// documentation of that function for more details.
kleidicv_error_t kleidicv_thread_min_max_u8(const uint8_t *src,
                                            size_t src_stride, size_t width,
                                            size_t height, uint8_t *min_value,
                                            uint8_t *max_value,
                                            kleidicv_thread_multithreading);
/// Multithreaded implementation of kleidicv_min_max_s8 - see the
/// documentation of that function for more details.
kleidicv_error_t kleidicv_thread_min_max_s8(const int8_t *src,
                                            size_t src_stride, size_t width,
                                            size_t height, int8_t *min_value,
                                            int8_t *max_value,
                                            kleidicv_thread_multithreading);
/// Multithreaded implementation of kleidicv_thread_min_max_u16 - see the
/// documentation of that function for more details.
kleidicv_error_t kleidicv_thread_min_max_u16(const uint16_t *src,
                                             size_t src_stride, size_t width,
                                             size_t height, uint16_t *min_value,
                                             uint16_t *max_value,
                                             kleidicv_thread_multithreading);
/// Multithreaded implementation of kleidicv_thread_min_max_s16 - see the
/// documentation of that function for more details.
kleidicv_error_t kleidicv_thread_min_max_s16(const int16_t *src,
                                             size_t src_stride, size_t width,
                                             size_t height, int16_t *min_value,
                                             int16_t *max_value,
                                             kleidicv_thread_multithreading);
/// Multithreaded implementation of kleidicv_thread_min_max_s32 - see the
/// documentation of that function for more details.
kleidicv_error_t kleidicv_thread_min_max_s32(const int32_t *src,
                                             size_t src_stride, size_t width,
                                             size_t height, int32_t *min_value,
                                             int32_t *max_value,
                                             kleidicv_thread_multithreading);
/// Multithreaded implementation of kleidicv_thread_min_max_f32 - see the
/// documentation of that function for more details.
kleidicv_error_t kleidicv_thread_min_max_f32(const float *src,
                                             size_t src_stride, size_t width,
                                             size_t height, float *min_value,
                                             float *max_value,
                                             kleidicv_thread_multithreading);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // KLEIDICV_THREAD_H
