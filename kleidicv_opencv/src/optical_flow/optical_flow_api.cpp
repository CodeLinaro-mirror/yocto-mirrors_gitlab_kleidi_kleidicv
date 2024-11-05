// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/dispatch.h"
#include "kleidicv_opencv/kleidicv_opencv.h"
#include "kleidicv_opencv/optical_flow.h"

KLEIDICV_MULTIVERSION_C_API(
    kleidicv_opencv_optical_flow_u8, &kleidicv_opencv::neon::optical_flow_u8,
    KLEIDICV_SVE2_IMPL_IF(&kleidicv_opencv::sve2::optical_flow_u8),
    KLEIDICV_SME2_IMPL_IF(&kleidicv_opencv::sme2::optical_flow_u8));
