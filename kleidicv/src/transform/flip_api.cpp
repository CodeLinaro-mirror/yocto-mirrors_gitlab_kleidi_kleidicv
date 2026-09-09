// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "kleidicv/dispatch.h"
#include "kleidicv/kleidicv.h"
#include "kleidicv/transform/flip.h"

KLEIDICV_MULTIVERSION_C_API_WITH_SME(kleidicv_flip, &kleidicv::neon::flip,
                                     &kleidicv::sve2::flip,
                                     &kleidicv::sme::flip,
                                     &kleidicv::sme2::flip);
