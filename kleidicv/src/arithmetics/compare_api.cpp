// SPDX-FileCopyrightText: 2023 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/arithmetics/compare.h"
#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"

#define KLEIDICV_DEFINE_CMP_EQ_API(name, type)                     \
  KLEIDICV_MULTIVERSION_C_API_WITH_SME(                            \
      name, &kleidicv::neon::compare_equal<type>,                  \
      KLEIDICV_SVE2_IMPL_IF(&kleidicv::sve2::compare_equal<type>), \
      &kleidicv::sme::compare_equal<type>,                         \
      KLEIDICV_SME2_IMPL_IF(&kleidicv::sme2::compare_equal<type>))

#define KLEIDICV_DEFINE_CMP_GT_API(name, type)                       \
  KLEIDICV_MULTIVERSION_C_API_WITH_SME(                              \
      name, &kleidicv::neon::compare_greater<type>,                  \
      KLEIDICV_SVE2_IMPL_IF(&kleidicv::sve2::compare_greater<type>), \
      &kleidicv::sme::compare_greater<type>,                         \
      KLEIDICV_SME2_IMPL_IF(&kleidicv::sme2::compare_greater<type>))

KLEIDICV_DEFINE_CMP_EQ_API(kleidicv_compare_equal_u8, uint8_t);
KLEIDICV_DEFINE_CMP_GT_API(kleidicv_compare_greater_u8, uint8_t);
