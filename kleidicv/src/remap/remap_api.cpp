// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/remap/remap.h"

#define KLEIDICV_DEFINE_C_API(outer_name, inner_name, type)                  \
  KLEIDICV_MULTIVERSION_C_API(outer_name, &kleidicv::neon::inner_name<type>, \
                              nullptr, nullptr)

KLEIDICV_DEFINE_C_API(kleidicv_remap_s16_u8, remap_s16, uint8_t);
