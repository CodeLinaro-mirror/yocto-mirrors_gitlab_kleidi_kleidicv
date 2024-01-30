// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "conversions/gray_to_rgb.h"
#include "dispatch.h"
#include "intrinsiccv.h"

namespace intrinsiccv {

#define INTRINSICCV_DEFINE_C_API(name, partialname)                     \
  INTRINSICCV_MULTIVERSION_C_API(                                       \
      name, intrinsiccv::neon::partialname,                             \
      INTRINSICCV_SVE2_IMPL_IF(intrinsiccv::sve2::partialname),         \
      intrinsiccv::sme2::partialname, void, const uint8_t *src,         \
      size_t src_stride, uint8_t *dst, size_t dst_stride, size_t width, \
      size_t height)

INTRINSICCV_DEFINE_C_API(intrinsiccv_gray_to_rgb_u8, gray_to_rgb_u8);
INTRINSICCV_DEFINE_C_API(intrinsiccv_gray_to_rgba_u8, gray_to_rgba_u8);

}  // namespace intrinsiccv
