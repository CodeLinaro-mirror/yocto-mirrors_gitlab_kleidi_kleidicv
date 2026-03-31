// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_ARITHMETICS_IN_RANGE_H
#define KLEIDICV_ARITHMETICS_IN_RANGE_H

#include "kleidicv/ctypes.h"

namespace kleidicv {

namespace neon {
template <typename T>
kleidicv_error_t in_range(const T *src, size_t src_stride, uint8_t *dst,
                          size_t dst_stride, size_t width, size_t height,
                          T lower_bound, T upper_bound);
}  // namespace neon

namespace sve2 {
template <typename T>
kleidicv_error_t in_range(const T *src, size_t src_stride, uint8_t *dst,
                          size_t dst_stride, size_t width, size_t height,
                          T lower_bound, T upper_bound);
}  // namespace sve2

namespace sme {
template <typename T>
kleidicv_error_t in_range(const T *src, size_t src_stride, uint8_t *dst,
                          size_t dst_stride, size_t width, size_t height,
                          T lower_bound, T upper_bound);
}  // namespace sme

namespace sme2 {
template <typename T>
kleidicv_error_t in_range(const T *src, size_t src_stride, uint8_t *dst,
                          size_t dst_stride, size_t width, size_t height,
                          T lower_bound, T upper_bound);
}  // namespace sme2

}  // namespace kleidicv

#endif  // KLEIDICV_ARITHMETICS_IN_RANGE_H
