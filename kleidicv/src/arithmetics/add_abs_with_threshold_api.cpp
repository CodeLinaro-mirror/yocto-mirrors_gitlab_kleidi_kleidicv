// SPDX-FileCopyrightText: 2023 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/arithmetics/add_abs_with_threshold.h"
#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/types.h"

#define KLEIDICV_DEFINE_C_API(name, type)                             \
  KLEIDICV_MULTIVERSION_C_API_WITH_SME(                               \
      name, &kleidicv::neon::saturating_add_abs_with_threshold<type>, \
      KLEIDICV_SVE2_IMPL_IF(                                          \
          &kleidicv::sve2::saturating_add_abs_with_threshold<type>),  \
      &kleidicv::sme::saturating_add_abs_with_threshold<type>,        \
      KLEIDICV_SME2_IMPL_IF(                                          \
          &kleidicv::sme2::saturating_add_abs_with_threshold<type>))

KLEIDICV_DEFINE_C_API(kleidicv_saturating_add_abs_with_threshold_s16, int16_t);
