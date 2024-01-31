// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "conversions/split.h"
#include "dispatch.h"
#include "intrinsiccv.h"

namespace intrinsiccv {

INTRINSICCV_MULTIVERSION_C_API(intrinsiccv_split, intrinsiccv::neon::split,
                               nullptr, nullptr, const void *src_data,
                               size_t src_stride, void **dst_data,
                               const size_t *dst_strides, size_t width,
                               size_t height, size_t channels,
                               size_t element_size);

}  // namespace intrinsiccv
