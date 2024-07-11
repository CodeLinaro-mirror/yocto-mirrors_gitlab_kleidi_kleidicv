// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/dispatch.h"
#include "kleidicv/filters/gaussian_blur.h"
#include "kleidicv/kleidicv.h"

KLEIDICV_MULTIVERSION_C_API(
    kleidicv_gaussian_blur_u8, &kleidicv::neon::gaussian_blur_u8,
    KLEIDICV_SVE2_IMPL_IF(kleidicv::sve2::gaussian_blur_u8),
    &kleidicv::sme2::gaussian_blur_u8);
