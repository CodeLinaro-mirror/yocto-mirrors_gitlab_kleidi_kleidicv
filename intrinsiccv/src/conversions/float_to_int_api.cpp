// SPDX-FileCopyrightText: 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/types.h"

namespace intrinsiccv {

namespace neon {

template <typename T>
intrinsiccv_error_t type_conversion_float_to_int(const float* src,
                                                 size_t src_stride, T* dst,
                                                 size_t dst_stride,
                                                 size_t width, size_t height);

}  // namespace neon

namespace sve2 {

template <typename T>
intrinsiccv_error_t type_conversion_float_to_int(const float* src,
                                                 size_t src_stride, T* dst,
                                                 size_t dst_stride,
                                                 size_t width, size_t height);

}  // namespace sve2

namespace sme2 {

template <typename T>
intrinsiccv_error_t type_conversion_float_to_int(const float* src,
                                                 size_t src_stride, T* dst,
                                                 size_t dst_stride,
                                                 size_t width, size_t height);

}  // namespace sme2

#define INTRINSICCV_DEFINE_C_API(name, type)                       \
  INTRINSICCV_MULTIVERSION_C_API(                                  \
      name, intrinsiccv::neon::type_conversion_float_to_int<type>, \
      INTRINSICCV_SVE2_IMPL_IF(                                    \
          intrinsiccv::sve2::type_conversion_float_to_int<type>),  \
      intrinsiccv::sme2::type_conversion_float_to_int<type>)

INTRINSICCV_DEFINE_C_API(intrinsiccv_type_conversion_f32_s8, int8_t);
INTRINSICCV_DEFINE_C_API(intrinsiccv_type_conversion_f32_u8, uint8_t);

}  // namespace intrinsiccv
