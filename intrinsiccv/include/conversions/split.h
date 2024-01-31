// SPDX-FileCopyrightText: 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef INTRINSICCV_CONVERSIONS_SPLIT_H
#define INTRINSICCV_CONVERSIONS_SPLIT_H

#include <cstddef>

#include "ctypes.h"

namespace intrinsiccv {

namespace neon {

intrinsiccv_error_t split(const void *src_data, size_t src_stride,
                          void **dst_data, const size_t *dst_strides,
                          size_t width, size_t height, size_t channels,
                          size_t element_size);

}  // namespace neon

}  // namespace intrinsiccv

#endif  // INTRINSICCV_CONVERSIONS_SPLIT_H
