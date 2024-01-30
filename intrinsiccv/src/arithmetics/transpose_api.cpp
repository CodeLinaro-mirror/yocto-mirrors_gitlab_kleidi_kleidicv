// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "arithmetics/transpose.h"
#include "dispatch.h"
#include "intrinsiccv.h"

namespace intrinsiccv {

static IFuncImpls transpose_impls_builder(void) {
  IFuncImpls impls;
  INTRINSICCV_ADD_NEON_IMPL(intrinsiccv::neon::transpose);
  return impls;
}
INTRINSICCV_MULTIVERSION_C_API(intrinsiccv_transpose, transpose_impls_builder,
                               void, const void *, size_t, void *, size_t,
                               size_t, size_t, size_t);

}  // namespace intrinsiccv
