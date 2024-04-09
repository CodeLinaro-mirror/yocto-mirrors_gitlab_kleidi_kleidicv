// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/conversions/gray_to_rgb.h"
#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/intrinsiccv.h"

#define INTRINSICCV_DEFINE_C_API(name, partialname)              \
  INTRINSICCV_MULTIVERSION_C_API(                                \
      name, &intrinsiccv::neon::partialname,                     \
      INTRINSICCV_SVE2_IMPL_IF(&intrinsiccv::sve2::partialname), \
      &intrinsiccv::sme2::partialname)

INTRINSICCV_DEFINE_C_API(intrinsiccv_gray_to_rgb_u8, gray_to_rgb_u8);
INTRINSICCV_DEFINE_C_API(intrinsiccv_gray_to_rgba_u8, gray_to_rgba_u8);
