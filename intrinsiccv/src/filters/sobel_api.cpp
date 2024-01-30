// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "dispatch.h"
#include "filters/sobel.h"
#include "intrinsiccv.h"

namespace intrinsiccv {

#define INTRINSICCV_DEFINE_C_API(name, partialname)                     \
  static IFuncImpls name##_impls_builder(void) {                        \
    IFuncImpls impls;                                                   \
    INTRINSICCV_ADD_NEON_IMPL(intrinsiccv::neon::partialname);          \
    INTRINSICCV_ADD_SVE2_IMPL_IF(intrinsiccv::sve2::partialname);       \
    INTRINSICCV_ADD_SME2_IMPL(intrinsiccv::sme2::partialname);          \
    return impls;                                                       \
  }                                                                     \
  INTRINSICCV_MULTIVERSION_C_API(name, name##_impls_builder, void,      \
                                 const uint8_t *src, size_t src_stride, \
                                 int16_t *dst, size_t dst_stride,       \
                                 size_t width, size_t height, size_t channels)

INTRINSICCV_DEFINE_C_API(intrinsiccv_sobel_3x3_horizontal_s16_u8,
                         sobel_3x3_horizontal_s16_u8);
INTRINSICCV_DEFINE_C_API(intrinsiccv_sobel_3x3_vertical_s16_u8,
                         sobel_3x3_vertical_s16_u8);

}  // namespace intrinsiccv
