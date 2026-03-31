// SPDX-FileCopyrightText: 2023 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/morphology/morphology.h"

#define KLEIDICV_DEFINE_C_API(name, tname, type)           \
  KLEIDICV_MULTIVERSION_C_API_WITH_SME(                    \
      name, &kleidicv::neon::tname<type>,                  \
      KLEIDICV_SVE2_IMPL_IF(&kleidicv::sve2::tname<type>), \
      &kleidicv::sme::tname<type>,                         \
      KLEIDICV_SME2_IMPL_IF(&kleidicv::sme2::tname<type>))

KLEIDICV_DEFINE_C_API(kleidicv_dilate_u8, dilate, uint8_t);
KLEIDICV_DEFINE_C_API(kleidicv_erode_u8, erode, uint8_t);
