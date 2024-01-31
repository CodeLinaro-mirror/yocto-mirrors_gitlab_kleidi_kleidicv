// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "arithmetics/transpose.h"
#include "dispatch.h"
#include "intrinsiccv.h"

namespace intrinsiccv {

INTRINSICCV_MULTIVERSION_C_API(intrinsiccv_transpose,
                               intrinsiccv::neon::transpose, nullptr, nullptr,
                               const void *, size_t, void *, size_t, size_t,
                               size_t, size_t);

}  // namespace intrinsiccv
