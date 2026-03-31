// SPDX-FileCopyrightText: 2024 - 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/analysis/sum.h"
#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"

KLEIDICV_MULTIVERSION_C_API_WITH_SME(
    kleidicv_sum_f32, (&kleidicv::neon::sum<float, double>),
    KLEIDICV_SVE2_IMPL_IF((&kleidicv::sve2::sum<float, double>)),
    (&kleidicv::sme::sum<float, double>),
    (&kleidicv::sme2::sum<float, double>));
