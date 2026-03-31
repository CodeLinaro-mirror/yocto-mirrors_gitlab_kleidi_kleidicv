// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_ARITHMETICS_COMPARE_H
#define KLEIDICV_ARITHMETICS_COMPARE_H

#include "kleidicv/ctypes.h"

namespace kleidicv {

namespace neon {
template <typename ScalarType>
kleidicv_error_t compare_equal(const ScalarType *src_a, size_t src_a_stride,
                               const ScalarType *src_b, size_t src_b_stride,
                               ScalarType *dst, size_t dst_stride, size_t width,
                               size_t height);

template <typename ScalarType>
kleidicv_error_t compare_greater(const ScalarType *src_a, size_t src_a_stride,
                                 const ScalarType *src_b, size_t src_b_stride,
                                 ScalarType *dst, size_t dst_stride,
                                 size_t width, size_t height);

}  // namespace neon

namespace sve2 {
template <typename ScalarType>
kleidicv_error_t compare_equal(const ScalarType *src_a, size_t src_a_stride,
                               const ScalarType *src_b, size_t src_b_stride,
                               ScalarType *dst, size_t dst_stride, size_t width,
                               size_t height);

template <typename ScalarType>
kleidicv_error_t compare_greater(const ScalarType *src_a, size_t src_a_stride,
                                 const ScalarType *src_b, size_t src_b_stride,
                                 ScalarType *dst, size_t dst_stride,
                                 size_t width, size_t height);
}  // namespace sve2

namespace sme {
template <typename ScalarType>
kleidicv_error_t compare_equal(const ScalarType *src_a, size_t src_a_stride,
                               const ScalarType *src_b, size_t src_b_stride,
                               ScalarType *dst, size_t dst_stride, size_t width,
                               size_t height);

template <typename ScalarType>
kleidicv_error_t compare_greater(const ScalarType *src_a, size_t src_a_stride,
                                 const ScalarType *src_b, size_t src_b_stride,
                                 ScalarType *dst, size_t dst_stride,
                                 size_t width, size_t height);
}  // namespace sme

namespace sme2 {
template <typename ScalarType>
kleidicv_error_t compare_equal(const ScalarType *src_a, size_t src_a_stride,
                               const ScalarType *src_b, size_t src_b_stride,
                               ScalarType *dst, size_t dst_stride, size_t width,
                               size_t height);

template <typename ScalarType>
kleidicv_error_t compare_greater(const ScalarType *src_a, size_t src_a_stride,
                                 const ScalarType *src_b, size_t src_b_stride,
                                 ScalarType *dst, size_t dst_stride,
                                 size_t width, size_t height);
}  // namespace sme2

}  // namespace kleidicv

#endif  // KLEIDICV_ARITHMETICS_COMPARE_H
