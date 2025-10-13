// SPDX-FileCopyrightText: 2023 - 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include <array>

#include "kleidicv/ctypes.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/traits.h"
#include "kleidicv/types.h"

namespace kleidicv::neon {

std::array<uint8_t, 256> precalculate_scale_table_u8(double scale,
                                                     double shift);

kleidicv_error_t scale_with_precalculated_table_u8(
    const uint8_t *src, size_t src_stride, uint8_t *dst, size_t dst_stride,
    size_t width, size_t height, double scale, double shift,
    const std::array<uint8_t, 256> &precalculated_table);

}  // namespace kleidicv::neon

namespace kleidicv::sve2 {

std::array<float16_t, 256> precalculate_scale_table_u8_f16(
    double dscale, double dshift) KLEIDICV_STREAMING;

kleidicv_error_t scale_with_precalculated_table_u8_f16(
    const uint8_t *src, size_t src_stride, float16_t *dst, size_t dst_stride,
    size_t width, size_t height,
    const std::array<float16_t, 256> &precalculated_table);
}  // namespace kleidicv::sve2

namespace kleidicv::sme {

std::array<float16_t, 256> precalculate_scale_table_u8_f16(
    double dscale, double dshift) KLEIDICV_STREAMING;

kleidicv_error_t scale_with_precalculated_table_u8_f16(
    const uint8_t *src, size_t src_stride, float16_t *dst, size_t dst_stride,
    size_t width, size_t height,
    const std::array<float16_t, 256> &precalculated_table) KLEIDICV_STREAMING;
}  // namespace kleidicv::sme

namespace kleidicv::sme2 {

std::array<float16_t, 256> precalculate_scale_table_u8_f16(
    double dscale, double dshift) KLEIDICV_STREAMING;

kleidicv_error_t scale_with_precalculated_table_u8_f16(
    const uint8_t *src, size_t src_stride, float16_t *dst, size_t dst_stride,
    size_t width, size_t height,
    const std::array<float16_t, 256> &precalculated_table) KLEIDICV_STREAMING;
}  // namespace kleidicv::sme2
