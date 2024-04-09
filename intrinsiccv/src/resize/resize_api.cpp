// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/resize/resize.h"

INTRINSICCV_MULTIVERSION_C_API(
    intrinsiccv_resize_to_quarter_u8, &intrinsiccv::neon::resize_to_quarter_u8,
    INTRINSICCV_SVE2_IMPL_IF(&intrinsiccv::sve2::resize_to_quarter_u8),
    &intrinsiccv::sme2::resize_to_quarter_u8);
