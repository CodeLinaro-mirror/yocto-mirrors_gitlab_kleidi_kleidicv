// SPDX-FileCopyrightText: 2023 - 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#include "intrinsiccv/dispatch.h"
#include "intrinsiccv/intrinsiccv.h"
#include "intrinsiccv/types.h"

namespace intrinsiccv {

namespace neon {

template <typename T>
intrinsiccv_error_t saturating_multiply(const T *src_a, size_t src_a_stride,
                                        const T *src_b, size_t src_b_stride,
                                        T *dst, size_t dst_stride, size_t width,
                                        size_t height, double scale);

}  // namespace neon

namespace sve2 {

template <typename T>
intrinsiccv_error_t saturating_multiply(const T *src_a, size_t src_a_stride,
                                        const T *src_b, size_t src_b_stride,
                                        T *dst, size_t dst_stride, size_t width,
                                        size_t height, double scale);

}  // namespace sve2

// namespace sme2 {

// template <typename T>
// intrinsiccv_error_t saturating_multiply(const T *src_a, size_t src_a_stride,
// const T *src_b,
//               size_t src_b_stride, T *dst, size_t dst_stride,
//               size_t width, size_t height, double scale);

// } // namespace sme2

}  // namespace intrinsiccv

#define KLEIDICV_DEFINE_C_API(name, type)                                   \
  KLEIDICV_MULTIVERSION_C_API(                                              \
      name, &intrinsiccv::neon::saturating_multiply<type>,                  \
      KLEIDICV_SVE2_IMPL_IF(&intrinsiccv::sve2::saturating_multiply<type>), \
      nullptr)

KLEIDICV_DEFINE_C_API(intrinsiccv_saturating_multiply_u8, uint8_t);
KLEIDICV_DEFINE_C_API(intrinsiccv_saturating_multiply_s8, int8_t);
KLEIDICV_DEFINE_C_API(intrinsiccv_saturating_multiply_u16, uint16_t);
KLEIDICV_DEFINE_C_API(intrinsiccv_saturating_multiply_s16, int16_t);
KLEIDICV_DEFINE_C_API(intrinsiccv_saturating_multiply_s32, int32_t);
