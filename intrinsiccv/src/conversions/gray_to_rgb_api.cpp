// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "conversions/gray_to_rgb.h"
#include "dispatch.h"
#include "intrinsiccv.h"

namespace intrinsiccv {

#define INTRINSICCV_DEFINE_C_API(name)                                         \
  static IFuncImpls name##_impls_builder(void) {                               \
    IFuncImpls impls;                                                          \
    INTRINSICCV_ADD_NEON_IMPL(intrinsiccv::neon::name);                        \
    INTRINSICCV_ADD_SVE2_IMPL_IF(intrinsiccv::sve2::name);                     \
    INTRINSICCV_ADD_SME2_IMPL(intrinsiccv::sme2::name);                        \
    return impls;                                                              \
  }                                                                            \
  INTRINSICCV_MULTIVERSION_C_API(                                              \
      name, name##_impls_builder, void, const uint8_t *src, size_t src_stride, \
      uint8_t *dst, size_t dst_stride, size_t width, size_t height)

INTRINSICCV_DEFINE_C_API(gray_to_rgb_u8);
INTRINSICCV_DEFINE_C_API(gray_to_rgba_u8);

}  // namespace intrinsiccv
