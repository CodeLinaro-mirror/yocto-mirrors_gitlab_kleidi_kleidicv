// SPDX-FileCopyrightText: 2023 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/arithmetics/scale.h"
#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/types.h"

KLEIDICV_MULTIVERSION_C_API_WITHOUT_SME(
    kleidicv_scale_u8, &(kleidicv::neon::scale<uint8_t, uint8_t>), nullptr);

KLEIDICV_MULTIVERSION_C_API_WITH_SME(
    kleidicv_scale_u8_f16, &(kleidicv::neon::scale<uint8_t, float16_t>),
    &(kleidicv::sve2::scale<uint8_t, float16_t>),
    &(kleidicv::sme::scale<uint8_t, float16_t>),
    &(kleidicv::sme2::scale<uint8_t, float16_t>));

KLEIDICV_MULTIVERSION_C_API_WITH_SME(
    kleidicv_scale_f32, &(kleidicv::neon::scale<float, float>),
    KLEIDICV_SVE2_IMPL_IF((&kleidicv::sve2::scale<float, float>)),
    (&kleidicv::sme::scale<float, float>),
    KLEIDICV_SME2_IMPL_IF((&kleidicv::sme2::scale<float, float>)));
