// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_ARITHMETICS_THRESHOLD_H
#define KLEIDICV_ARITHMETICS_THRESHOLD_H

#include "kleidicv/ctypes.h"

namespace kleidicv {

namespace neon {
template <typename T>
kleidicv_error_t threshold_binary(const T *src, size_t src_stride, T *dst,
                                  size_t dst_stride, size_t width,
                                  size_t height, T threshold, T value);
}  // namespace neon

namespace sve2 {
template <typename T>
kleidicv_error_t threshold_binary(const T *src, size_t src_stride, T *dst,
                                  size_t dst_stride, size_t width,
                                  size_t height, T threshold, T value);
}  // namespace sve2

namespace sme {
template <typename T>
kleidicv_error_t threshold_binary(const T *src, size_t src_stride, T *dst,
                                  size_t dst_stride, size_t width,
                                  size_t height, T threshold, T value);
}  // namespace sme

namespace sme2 {
template <typename T>
kleidicv_error_t threshold_binary(const T *src, size_t src_stride, T *dst,
                                  size_t dst_stride, size_t width,
                                  size_t height, T threshold, T value);
}  // namespace sme2

}  // namespace kleidicv

#endif  // KLEIDICV_ARITHMETICS_THRESHOLD_H
