// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/resize/resize_linear.h"

INTRINSICCV_MULTIVERSION_C_API(
    intrinsiccv_resize_linear_u8, &intrinsiccv::neon::resize_linear_u8,
    INTRINSICCV_SVE2_IMPL_IF(&intrinsiccv::sve2::resize_linear_u8),
    &intrinsiccv::sme2::resize_linear_u8);
