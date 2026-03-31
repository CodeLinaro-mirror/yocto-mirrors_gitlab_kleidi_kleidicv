// SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/arithmetics/exp.h"
#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/types.h"

#define KLEIDICV_DEFINE_C_API(name, type)                           \
  KLEIDICV_MULTIVERSION_C_API_WITH_SME(                             \
      name, &kleidicv::neon::exp<type>, &kleidicv::sve2::exp<type>, \
      &kleidicv::sme::exp<type>, &kleidicv::sme2::exp<type>)

KLEIDICV_DEFINE_C_API(kleidicv_exp_f32, float);
