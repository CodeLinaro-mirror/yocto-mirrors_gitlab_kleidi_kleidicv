// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "conversions/split.h"
#include "dispatch.h"
#include "intrinsiccv.h"

namespace intrinsiccv {

static IFuncImpls split_impls_builder(void) {
  IFuncImpls impls;
  INTRINSICCV_ADD_NEON_IMPL(intrinsiccv::neon::split);
  return impls;
}
INTRINSICCV_MULTIVERSION_C_API(intrinsiccv_split, split_impls_builder, void,
                               const void *src_data, size_t src_stride,
                               void **dst_data, size_t *dst_strides,
                               size_t width, size_t height, size_t channels,
                               size_t element_size);

}  // namespace intrinsiccv
