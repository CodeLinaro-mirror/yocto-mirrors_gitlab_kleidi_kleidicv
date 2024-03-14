// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/arithmetics/transpose.h"
#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/intrinsiccv.h"

namespace intrinsiccv {

INTRINSICCV_MULTIVERSION_C_API(intrinsiccv_transpose,
                               intrinsiccv::neon::transpose, nullptr, nullptr);

}  // namespace intrinsiccv
