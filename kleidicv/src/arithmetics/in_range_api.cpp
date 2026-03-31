// SPDX-FileCopyrightText: 2023 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/arithmetics/in_range.h"
#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"

#define KLEIDICV_DEFINE_C_API(name, type)                     \
  KLEIDICV_MULTIVERSION_C_API_WITH_SME(                       \
      name, &kleidicv::neon::in_range<type>,                  \
      KLEIDICV_SVE2_IMPL_IF(&kleidicv::sve2::in_range<type>), \
      &kleidicv::sme::in_range<type>,                         \
      KLEIDICV_SME2_IMPL_IF(&kleidicv::sme2::in_range<type>))

KLEIDICV_DEFINE_C_API(kleidicv_in_range_u8, uint8_t);
KLEIDICV_DEFINE_C_API(kleidicv_in_range_f32, float);
