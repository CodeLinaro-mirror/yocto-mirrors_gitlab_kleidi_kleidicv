// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/conversions/merge.h"
#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/intrinsiccv.h"

namespace intrinsiccv {

INTRINSICCV_MULTIVERSION_C_API(intrinsiccv_merge, intrinsiccv::neon::merge,
                               nullptr, nullptr);

}  // namespace intrinsiccv
