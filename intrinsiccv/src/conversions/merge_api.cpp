// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "conversions/merge.h"
#include "dispatch.h"
#include "intrinsiccv.h"

namespace intrinsiccv {

static IFuncImpls merge_impls_builder(void) {
  IFuncImpls impls;
  INTRINSICCV_ADD_NEON_IMPL(intrinsiccv::neon::merge);
  return impls;
}
INTRINSICCV_MULTIVERSION_C_API(merge, merge_impls_builder, void, const void **,
                               const size_t *, void *, size_t, size_t, size_t,
                               size_t, size_t);

}  // namespace intrinsiccv
