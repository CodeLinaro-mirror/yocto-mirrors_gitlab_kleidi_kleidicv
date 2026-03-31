// SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/arithmetics/bitwise_and.h"
#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"

#define KLEIDICV_DEFINE_C_API(name, type)                        \
  KLEIDICV_MULTIVERSION_C_API_WITH_SME(                          \
      name, &kleidicv::neon::bitwise_and<type>,                  \
      KLEIDICV_SVE2_IMPL_IF(&kleidicv::sve2::bitwise_and<type>), \
      &kleidicv::sme::bitwise_and<type>,                         \
      KLEIDICV_SME2_IMPL_IF(&kleidicv::sme2::bitwise_and<type>))

KLEIDICV_DEFINE_C_API(kleidicv_bitwise_and, uint8_t);
