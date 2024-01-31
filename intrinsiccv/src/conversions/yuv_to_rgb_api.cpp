// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "conversions/yuv_to_rgb.h"
#include "dispatch.h"
#include "intrinsiccv.h"

namespace intrinsiccv {

#define INTRINSICCV_DEFINE_C_API(name, partialname)                         \
  INTRINSICCV_MULTIVERSION_C_API(                                           \
      name, intrinsiccv::neon::partialname, intrinsiccv::sve2::partialname, \
      intrinsiccv::sme2::partialname, const uint8_t *, size_t,              \
      const uint8_t *, size_t, uint8_t *, size_t, size_t, size_t, bool)

INTRINSICCV_DEFINE_C_API(intrinsiccv_yuv_sp_to_rgb_u8, yuv_sp_to_rgb_u8);
INTRINSICCV_DEFINE_C_API(intrinsiccv_yuv_sp_to_bgr_u8, yuv_sp_to_bgr_u8);
INTRINSICCV_DEFINE_C_API(intrinsiccv_yuv_sp_to_rgba_u8, yuv_sp_to_rgba_u8);
INTRINSICCV_DEFINE_C_API(intrinsiccv_yuv_sp_to_bgra_u8, yuv_sp_to_bgra_u8);

}  // namespace intrinsiccv
