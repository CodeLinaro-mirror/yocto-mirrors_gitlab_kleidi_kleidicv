// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_RESIZE_RESIZE_LINEAR_H
#define KLEIDICV_RESIZE_RESIZE_LINEAR_H

#include "kleidicv/kleidicv.h"

namespace kleidicv {

namespace neon {
kleidicv_error_t resize_linear_stripe_u8(const uint8_t *src, size_t src_stride,
                                         size_t src_width, size_t src_height,
                                         size_t y_begin, size_t y_end,
                                         uint8_t *dst, size_t dst_stride,
                                         size_t dst_width, size_t dst_height);
kleidicv_error_t resize_linear_stripe_f32(const float *src, size_t src_stride,
                                          size_t src_width, size_t src_height,
                                          size_t y_begin, size_t y_end,
                                          float *dst, size_t dst_stride,
                                          size_t dst_width, size_t dst_height);
}  // namespace neon

namespace sve2 {
kleidicv_error_t resize_linear_stripe_u8(const uint8_t *src, size_t src_stride,
                                         size_t src_width, size_t src_height,
                                         size_t y_begin, size_t y_end,
                                         uint8_t *dst, size_t dst_stride,
                                         size_t dst_width, size_t dst_height);
kleidicv_error_t resize_linear_stripe_f32(const float *src, size_t src_stride,
                                          size_t src_width, size_t src_height,
                                          size_t y_begin, size_t y_end,
                                          float *dst, size_t dst_stride,
                                          size_t dst_width, size_t dst_height);
}  // namespace sve2

namespace sme2 {
kleidicv_error_t resize_linear_stripe_u8(const uint8_t *src, size_t src_stride,
                                         size_t src_width, size_t src_height,
                                         size_t y_begin, size_t y_end,
                                         uint8_t *dst, size_t dst_stride,
                                         size_t dst_width, size_t dst_height);
kleidicv_error_t resize_linear_stripe_f32(const float *src, size_t src_stride,
                                          size_t src_width, size_t src_height,
                                          size_t y_begin, size_t y_end,
                                          float *dst, size_t dst_stride,
                                          size_t dst_width, size_t dst_height);
}  // namespace sme2

}  // namespace kleidicv

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
/// Internal - not part of the public API and its direct use is not supported.
/// It is used by the multithreaded function.
extern kleidicv_error_t (*kleidicv_resize_linear_stripe_u8)(
    const uint8_t *src, size_t src_stride, size_t src_width, size_t src_height,
    size_t y_begin, size_t y_end, uint8_t *dst, size_t dst_stride,
    size_t dst_width, size_t dst_height);

/// Internal - not part of the public API and its direct use is not supported.
/// It is used by the multithreaded function.
extern kleidicv_error_t (*kleidicv_resize_linear_stripe_f32)(
    const float *src, size_t src_stride, size_t src_width, size_t src_height,
    size_t y_begin, size_t y_end, float *dst, size_t dst_stride,
    size_t dst_width, size_t dst_height);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // KLEIDICV_RESIZE_RESIZE_H
