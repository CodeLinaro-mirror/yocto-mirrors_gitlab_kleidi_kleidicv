// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "conversions/merge.h"
#include "dispatch.h"
#include "intrinsiccv.h"

namespace intrinsiccv {

INTRINSICCV_MULTIVERSION_C_API(intrinsiccv_merge, intrinsiccv::neon::merge,
                               nullptr, nullptr, void, const void **,
                               const size_t *, void *, size_t, size_t, size_t,
                               size_t, size_t);

}  // namespace intrinsiccv
