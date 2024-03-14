// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/conversions/yuv_to_rgb.h"
#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/intrinsiccv.h"

namespace intrinsiccv {

#define INTRINSICCV_DEFINE_C_API(name, partialname)                    \
  INTRINSICCV_MULTIVERSION_C_API(name, intrinsiccv::neon::partialname, \
                                 intrinsiccv::sve2::partialname,       \
                                 intrinsiccv::sme2::partialname)

INTRINSICCV_DEFINE_C_API(intrinsiccv_yuv_sp_to_rgb_u8, yuv_sp_to_rgb_u8);
INTRINSICCV_DEFINE_C_API(intrinsiccv_yuv_sp_to_bgr_u8, yuv_sp_to_bgr_u8);
INTRINSICCV_DEFINE_C_API(intrinsiccv_yuv_sp_to_rgba_u8, yuv_sp_to_rgba_u8);
INTRINSICCV_DEFINE_C_API(intrinsiccv_yuv_sp_to_bgra_u8, yuv_sp_to_bgra_u8);

}  // namespace intrinsiccv
