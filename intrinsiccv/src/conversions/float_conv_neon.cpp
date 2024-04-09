// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/neon.h"

namespace intrinsiccv::neon {

template <typename InputType, typename OutputType>
intrinsiccv_error_t float_conversion(const InputType*, size_t, OutputType*,
                                     size_t, size_t, size_t) {
  return INTRINSICCV_ERROR_NOT_IMPLEMENTED;
}

#define INTRINSICCV_INSTANTIATE_TEMPLATE(itype, otype)                        \
  template INTRINSICCV_TARGET_FN_ATTRS intrinsiccv_error_t                    \
  float_conversion<itype, otype>(const itype* src, size_t src_stride,         \
                                 otype* dst, size_t dst_stride, size_t width, \
                                 size_t height)

INTRINSICCV_INSTANTIATE_TEMPLATE(float, int8_t);
INTRINSICCV_INSTANTIATE_TEMPLATE(float, uint8_t);
INTRINSICCV_INSTANTIATE_TEMPLATE(int8_t, float);
INTRINSICCV_INSTANTIATE_TEMPLATE(uint8_t, float);

}  // namespace intrinsiccv::neon
