// SPDX-FileCopyrightText: 2023 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/analysis/min_max.h"
#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/types.h"

#define KLEIDICV_DEFINE_MINMAX_API(name, type)               \
  KLEIDICV_MULTIVERSION_C_API_WITH_SME(                      \
      name, &kleidicv::neon::min_max<type>,                  \
      KLEIDICV_SVE2_IMPL_IF(&kleidicv::sve2::min_max<type>), \
      &kleidicv::sme::min_max<type>,                         \
      KLEIDICV_SME2_IMPL_IF(&kleidicv::sme2::min_max<type>))

KLEIDICV_DEFINE_MINMAX_API(kleidicv_min_max_u8, uint8_t);
KLEIDICV_DEFINE_MINMAX_API(kleidicv_min_max_s8, int8_t);
KLEIDICV_DEFINE_MINMAX_API(kleidicv_min_max_u16, uint16_t);
KLEIDICV_DEFINE_MINMAX_API(kleidicv_min_max_s16, int16_t);
KLEIDICV_DEFINE_MINMAX_API(kleidicv_min_max_s32, int32_t);
KLEIDICV_DEFINE_MINMAX_API(kleidicv_min_max_f32, float);

#define KLEIDICV_DEFINE_MINMAXLOC_API(name, type) \
  KLEIDICV_MULTIVERSION_C_API_WITHOUT_SME(        \
      name, &kleidicv::neon::min_max_loc<type>, nullptr)

KLEIDICV_DEFINE_MINMAXLOC_API(kleidicv_min_max_loc_u8, uint8_t);
