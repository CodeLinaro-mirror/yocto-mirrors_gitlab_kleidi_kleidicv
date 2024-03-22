// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/neon.h"

namespace intrinsiccv::neon {

template <typename T>
intrinsiccv_error_t type_conversion_float_to_int(const float*, size_t, T*,
                                                 size_t, size_t, size_t) {
  return INTRINSICCV_ERROR_NOT_IMPLEMENTED;
}

#define INTRINSICCV_INSTANTIATE_TEMPLATE(type)                            \
  template INTRINSICCV_TARGET_FN_ATTRS intrinsiccv_error_t                \
  type_conversion_float_to_int<type>(const float* src, size_t src_stride, \
                                     type* dst, size_t dst_stride,        \
                                     size_t width, size_t height)

INTRINSICCV_INSTANTIATE_TEMPLATE(int8_t);
INTRINSICCV_INSTANTIATE_TEMPLATE(uint8_t);

}  // namespace intrinsiccv::neon
