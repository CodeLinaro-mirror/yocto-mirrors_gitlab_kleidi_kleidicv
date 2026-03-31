// SPDX-FileCopyrightText: 2023 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_ARITHMETICS_SCALE_H
#define KLEIDICV_ARITHMETICS_SCALE_H

#include <array>

#include "kleidicv/ctypes.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/traits.h"
#include "kleidicv/types.h"

namespace kleidicv {

namespace neon {

std::array<uint8_t, 256> precalculate_scale_table_u8(double scale,
                                                     double shift);

kleidicv_error_t scale_with_precalculated_table_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, double scale, double shift,
    const std::array<uint8_t, 256> &precalculated_table);

template <typename T, typename U>
kleidicv_error_t scale(const T *src, size_t src_stride, U *dst,
                       size_t dst_stride, size_t width, size_t height,
                       double scale, double shift);
}  // namespace neon

namespace sve2 {

template <typename T, typename U>
kleidicv_error_t scale(const T *src, size_t src_stride, U *dst,
                       size_t dst_stride, size_t width, size_t height,
                       double scale, double shift);

}  // namespace sve2

namespace sme {

template <typename T, typename U>
kleidicv_error_t scale(const T *src, size_t src_stride, U *dst,
                       size_t dst_stride, size_t width, size_t height,
                       double scale, double shift);

}  // namespace sme

namespace sme2 {

template <typename T, typename U>
kleidicv_error_t scale(const T *src, size_t src_stride, U *dst,
                       size_t dst_stride, size_t width, size_t height,
                       double scale, double shift);

}  // namespace sme2

}  // namespace kleidicv

#endif  // KLEIDICV_ARITHMETICS_SCALE_H
