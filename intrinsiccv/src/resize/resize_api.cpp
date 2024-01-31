// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "dispatch.h"
#include "intrinsiccv.h"
#include "resize/resize.h"

namespace intrinsiccv {

INTRINSICCV_MULTIVERSION_C_API(
    intrinsiccv_resize_to_quarter_u8, intrinsiccv::neon::resize_to_quarter_u8,
    INTRINSICCV_SVE2_IMPL_IF(intrinsiccv::sve2::resize_to_quarter_u8),
    intrinsiccv::sme2::resize_to_quarter_u8, const uint8_t *src,
    size_t src_stride, size_t src_width, size_t src_height, uint8_t *dst,
    size_t dst_stride, size_t dst_width, size_t dst_height);

}  // namespace intrinsiccv
