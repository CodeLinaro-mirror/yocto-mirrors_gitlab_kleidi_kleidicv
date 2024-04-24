// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/resize/resize_linear.h"

KLEIDICV_MULTIVERSION_C_API(
    kleidicv_resize_linear_u8, &kleidicv::neon::resize_linear_u8,
    KLEIDICV_SVE2_IMPL_IF(&kleidicv::sve2::resize_linear_u8),
    &kleidicv::sme2::resize_linear_u8);

KLEIDICV_MULTIVERSION_C_API(
    kleidicv_resize_linear_f32, &kleidicv::neon::resize_linear_f32,
    KLEIDICV_SVE2_IMPL_IF(&kleidicv::sve2::resize_linear_f32),
    &kleidicv::sme2::resize_linear_f32);
