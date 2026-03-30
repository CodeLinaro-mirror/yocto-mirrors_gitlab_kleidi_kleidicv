// SPDX-FileCopyrightText: 2023 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/arithmetics/add.h"
#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"

#define KLEIDICV_DEFINE_C_API(name, type)                           \
  KLEIDICV_MULTIVERSION_C_API_WITH_SME(                             \
      name, &kleidicv::neon::saturating_add<type>,                  \
      KLEIDICV_SVE2_IMPL_IF(&kleidicv::sve2::saturating_add<type>), \
      &kleidicv::sme::saturating_add<type>,                         \
      KLEIDICV_SME2_IMPL_IF(&kleidicv::sme2::saturating_add<type>))

KLEIDICV_DEFINE_C_API(kleidicv_saturating_add_s8, int8_t);
KLEIDICV_DEFINE_C_API(kleidicv_saturating_add_u8, uint8_t);
KLEIDICV_DEFINE_C_API(kleidicv_saturating_add_s16, int16_t);
KLEIDICV_DEFINE_C_API(kleidicv_saturating_add_u16, uint16_t);
KLEIDICV_DEFINE_C_API(kleidicv_saturating_add_s32, int32_t);
KLEIDICV_DEFINE_C_API(kleidicv_saturating_add_u32, uint32_t);
KLEIDICV_DEFINE_C_API(kleidicv_saturating_add_s64, int64_t);
KLEIDICV_DEFINE_C_API(kleidicv_saturating_add_u64, uint64_t);
