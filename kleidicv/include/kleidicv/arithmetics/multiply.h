// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_ARITHMETICS_MULTIPLY_H
#define KLEIDICV_ARITHMETICS_MULTIPLY_H

#include "kleidicv/ctypes.h"

namespace kleidicv {

namespace neon {

template <typename T>
kleidicv_error_t saturating_multiply(const T *src_a, size_t src_a_stride,
                                     const T *src_b, size_t src_b_stride,
                                     T *dst, size_t dst_stride, size_t width,
                                     size_t height, double scale);

}  // namespace neon

namespace sve2 {

template <typename T>
kleidicv_error_t saturating_multiply(const T *src_a, size_t src_a_stride,
                                     const T *src_b, size_t src_b_stride,
                                     T *dst, size_t dst_stride, size_t width,
                                     size_t height, double scale);

}  // namespace sve2

namespace sme {

template <typename T>
kleidicv_error_t saturating_multiply(const T *src_a, size_t src_a_stride,
                                     const T *src_b, size_t src_b_stride,
                                     T *dst, size_t dst_stride, size_t width,
                                     size_t height, double scale);

}  // namespace sme

namespace sme2 {

template <typename T>
kleidicv_error_t saturating_multiply(const T *src_a, size_t src_a_stride,
                                     const T *src_b, size_t src_b_stride,
                                     T *dst, size_t dst_stride, size_t width,
                                     size_t height, double scale);

}  // namespace sme2

}  // namespace kleidicv

#endif  // KLEIDICV_ARITHMETICS_MULTIPLY_H
